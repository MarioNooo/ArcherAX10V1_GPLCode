/*
* <:copyright-BRCM:2016:DUAL/GPL:standard
* 
*    Copyright (c) 2016 Broadcom 
*    All Rights Reserved
* 
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License, version 2, as published by
* the Free Software Foundation (the "GPL").
* 
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
* 
* 
* A copy of the GPL is available at http://www.broadcom.com/licenses/GPLv2.php, or by
* writing to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
* Boston, MA 02111-1307, USA.
* 
* :> 
*/


/***************************************************************************
 * File Name  : bcm63xx_led.c
 *
 * Description: 
 *    This file contains bcm963xx board led control API functions. 
 *
 ***************************************************************************/

/* Includes. */
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/capability.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <asm/uaccess.h>
#include <linux/version.h>

#include <bcm_assert_locks.h>
#include <bcm_map_part.h>
#include <board.h>
#include <boardparms.h>
#include <shared_utils.h>
#include <bcm_led.h>
#include <bcmtypes.h>

extern spinlock_t bcm_gpio_spinlock;

#define kFastBlinkCount     0          // 125ms
#define kSlowBlinkCount     1          // 250ms

#define kLedOff             0
#define kLedOn              1

#define kLedGreen           0
#define kLedRed             1

// uncomment // for debug led
// #define DEBUG_LED

typedef int (*BP_LED_FUNC) (unsigned short *);

typedef struct {
    BOARD_LED_NAME ledName;
    BP_LED_FUNC bpFunc;
    BP_LED_FUNC bpFuncFail;
} BP_LED_INFO, *PBP_LED_INFO;

typedef struct {
    short ledGreenGpio;             // GPIO # for LED
    short ledRedGpio;               // GPIO # for Fail LED
    BOARD_LED_STATE ledState;       // current led state
    short blinkCountDown;           // Count for blink states
} LED_CTRL, *PLED_CTRL;

static BP_LED_INFO bpLedInfo[] =
{
    {kLedAdsl, BpGetAdslLedGpio, BpGetAdslFailLedGpio},
    {kLedSecAdsl, BpGetSecAdslLedGpio, BpGetSecAdslFailLedGpio},
    {kLedWanData, BpGetWanDataLedGpio, BpGetWanErrorLedGpio},
    {kLedSes, BpGetWirelessSesLedGpio, NULL},
    {kLedVoip, BpGetVoipLedGpio, NULL},
    {kLedVoip1, BpGetVoip1LedGpio, BpGetVoip1FailLedGpio},
    {kLedVoip2, BpGetVoip2LedGpio, BpGetVoip2FailLedGpio},
    {kLedPots, BpGetPotsLedGpio, NULL},
    {kLedDect, BpGetDectLedGpio, NULL},
    {kLedGpon, BpGetGponLedGpio, BpGetGponFailLedGpio},
    {kLedMoCA, BpGetMoCALedGpio, BpGetMoCAFailLedGpio},
    {kLedWL0, BpGetWL0ActLedGpio, NULL},
    {kLedWL1, BpGetWL1ActLedGpio, NULL},
#ifdef CONFIG_BCM_PON
    {kLedOpticalLink,  NULL, BpGetOpticalLinkFailLedGpio},
    {kLedUSB, BpGetUSBLedGpio, NULL},
    {kLedSim, BpGetGpioLedSim, NULL},
    {kLedSimITMS, BpGetGpioLedSimITMS, NULL},
    {kLedEpon, BpGetEponLedGpio, BpGetEponFailLedGpio},
#endif
#ifdef CONFIG_TP_IMAGE
    {kLedPower, BpGetPowerLedGpio, NULL},
    {kLedAnyLanLink, BpGetAnyLanLinkLedGpio, NULL},
    {kLedInternetOrg, BpGetInterOrgLedGpio, NULL},
    {kLedInternetBlue, BpGetInterBlueLedGpio, NULL},
    {kLedWps, BpGetWpsLedGpio, NULL},
    {kLedWL24G, BpGetWireless2GLedGpio, NULL},
    {kLedWL5G, BpGetWireless5GLedGpio, NULL},
#endif
    {kLedEnd, NULL, NULL}
};

// global variables:
static struct timer_list gLedTimer;
static PLED_CTRL gLedCtrl = NULL;
static int gTimerOn = FALSE;
static int gTimerOnRequests = 0;
static unsigned int gLedRunningCounter = 0;  // only used by WLAN

int g_ledInitialized = 0;

void (*ethsw_led_control)(unsigned long ledMask, int value) = NULL;
EXPORT_SYMBOL(ethsw_led_control);

/** This spinlock protects all access to gLedCtrl, gTimerOn
 *  gTimerOnRequests, gLedRunningCounter, and ledTimerStart function.
 *  Use spin_lock_irqsave to lock the spinlock because ledTimerStart
 *  may be called from interrupt handler (WLAN?)
 */
static spinlock_t brcm_ledlock;
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 19, 0)
static void ledTimerExpire(void);
#else
static void ledTimerExpire(struct timer_list*);
#endif

//**************************************************************************************
// LED operations
//**************************************************************************************

// turn led on and set the ledState
static void setLed (PLED_CTRL pLed, unsigned short led_state, unsigned short led_type)
{
    short led_gpio;
    unsigned short gpio_state;
    unsigned long flags;


    if (led_type == kLedRed)
        led_gpio = pLed->ledRedGpio;
    else
        led_gpio = pLed->ledGreenGpio;

        dev_dbg(NULL,  "********************************************************\n");
        dev_dbg(NULL,  "setLed %d %x\n", led_gpio&0xff, led_gpio);
        dev_dbg(NULL,  "********************************************************\n");

    if (led_gpio == -1)
        return;

    if (((led_gpio & BP_ACTIVE_LOW) && (led_state == kLedOn)) || 
        (!(led_gpio & BP_ACTIVE_LOW) && (led_state == kLedOff)))
        gpio_state = 0;
    else
        gpio_state = 1;

    /* spinlock to protect access to GPIODir, GPIOio */
    spin_lock_irqsave(&bcm_gpio_spinlock, flags);

#if defined(CONFIG_BCM963268) 
    if (led_gpio & BP_LED_USE_GPIO) {
        /* This Led is a GPIO, set/unset led directly */
        /* Take over high GPIOs from WLAN block */
        if ((led_gpio & BP_GPIO_NUM_MASK) > 35)
            GPIO->GPIOCtrl |= GPIO_NUM_TO_MASK(led_gpio - 32);
        /* set led to output */
        GPIO->GPIODir |= GPIO_NUM_TO_MASK(led_gpio);
        /* set value */
        if( gpio_state )
            GPIO->GPIOio |= GPIO_NUM_TO_MASK(led_gpio);
        else
            GPIO->GPIOio &= ~GPIO_NUM_TO_MASK(led_gpio);
    } else {
        /* Enable LED controller to drive this GPIO */
        if (!(led_gpio & BP_GPIO_SERIAL))
            GPIO->LEDCtrl |= GPIO_NUM_TO_MASK(led_gpio);

        LED->ledMode &= ~(LED_MODE_MASK << GPIO_NUM_TO_LED_MODE_SHIFT(led_gpio));
        if( gpio_state )
            LED->ledMode |= (LED_MODE_OFF << GPIO_NUM_TO_LED_MODE_SHIFT(led_gpio));
        else
            LED->ledMode |= (LED_MODE_ON << GPIO_NUM_TO_LED_MODE_SHIFT(led_gpio));
    }
#elif defined(CONFIG_BCM96838)
    if ( (led_gpio&BP_LED_PIN) || (led_gpio&BP_GPIO_SERIAL) )
    {
        LED->ledMode &= ~(LED_MODE_MASK << GPIO_NUM_TO_LED_MODE_SHIFT(led_gpio));
        if( gpio_state )
            LED->ledMode |= (LED_MODE_OFF << GPIO_NUM_TO_LED_MODE_SHIFT(led_gpio));
        else
            LED->ledMode |= (LED_MODE_ON << GPIO_NUM_TO_LED_MODE_SHIFT(led_gpio));
    }
    else
    {
        led_gpio &= BP_GPIO_NUM_MASK;
        gpio_set_dir(led_gpio, 1);
        gpio_set_data(led_gpio, gpio_state);
    }
#else
    bcm_led_driver_set(led_gpio, led_state);
#endif

    spin_unlock_irqrestore(&bcm_gpio_spinlock, flags);
}

// toggle the LED
static void ledToggle(PLED_CTRL pLed)
{
    short led_gpio;
    short green_led_gpio , red_led_gpio;

   green_led_gpio = pLed->ledGreenGpio ;
   red_led_gpio = pLed->ledRedGpio ;

    if ((-1== green_led_gpio) && (-1== red_led_gpio))
        return;

    /* Currently all the chips don't support blinking of RED LED */
    if (-1== green_led_gpio)
        return;
  
   led_gpio = green_led_gpio ;

#if defined(CONFIG_BCM963268)
    LED->ledMode ^= (LED_MODE_MASK << GPIO_NUM_TO_LED_MODE_SHIFT(led_gpio));
#elif defined(CONFIG_BCM96838)
    if ( (led_gpio&BP_LED_PIN) || (led_gpio&BP_GPIO_SERIAL) )
        LED->ledMode ^= (LED_MODE_MASK << GPIO_NUM_TO_LED_MODE_SHIFT(led_gpio));
    else
    {
        unsigned long flags;
		led_gpio &= BP_GPIO_NUM_MASK;
        spin_lock_irqsave(&bcm_gpio_spinlock, flags);
        gpio_set_data(led_gpio, 1^gpio_get_data(led_gpio));
        spin_unlock_irqrestore(&bcm_gpio_spinlock, flags);
    }
#elif defined(CONFIG_BCM960333)
    {
        unsigned long flags;
        led_gpio &= BP_GPIO_NUM_MASK;
        spin_lock_irqsave(&bcm_gpio_spinlock, flags);
        bcm_led_driver_toggle(led_gpio);
        spin_unlock_irqrestore(&bcm_gpio_spinlock, flags);
    }
#else
    bcm_led_driver_toggle(led_gpio);
#endif
}   

/** Start the LED timer
 *
 * Must be called with brcm_ledlock held
 */
static void ledTimerStart(void)
{
#if defined(DEBUG_LED)
    printk("led: add_timer\n");
#endif

    BCM_ASSERT_HAS_SPINLOCK_C(&brcm_ledlock);

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 19, 0)
    init_timer(&gLedTimer);
    gLedTimer.function = (void*)ledTimerExpire;
#else
    timer_setup(&gLedTimer, ledTimerExpire, 0);
#endif
    gLedTimer.expires = jiffies + msecs_to_jiffies(125);        // timer expires in ~125ms
    add_timer (&gLedTimer);
} 


// led timer expire kicks in about ~100ms and perform the led operation according to the ledState and
// restart the timer according to ledState
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 19, 0)
static void ledTimerExpire(void)
#else
static void ledTimerExpire(struct timer_list *tl)
#endif
{
    int i;
    PLED_CTRL pCurLed;
    unsigned long flags;

    BCM_ASSERT_NOT_HAS_SPINLOCK_V(&brcm_ledlock);

    for (i = 0, pCurLed = gLedCtrl; i < kLedEnd; i++, pCurLed++)
    {
#if defined(DEBUG_LED)
        printk("led[%d]: Mask=0x%04x, State = %d, blcd=%d\n", i, pCurLed->ledGreenGpio, pCurLed->ledState, pCurLed->blinkCountDown);
#endif
        spin_lock_irqsave(&brcm_ledlock, flags);        // LEDs can be changed from ISR
        switch (pCurLed->ledState)
        {
        case kLedStateOn:
        case kLedStateOff:
        case kLedStateFail:
            pCurLed->blinkCountDown = 0;            // reset the blink count down
            spin_unlock_irqrestore(&brcm_ledlock, flags);
            break;

        case kLedStateSlowBlinkContinues:
            if (pCurLed->blinkCountDown-- == 0)
            {
                pCurLed->blinkCountDown = kSlowBlinkCount;
                ledToggle(pCurLed);
            }
            gTimerOnRequests++;
            spin_unlock_irqrestore(&brcm_ledlock, flags);
            break;

        case kLedStateFastBlinkContinues:
            if (pCurLed->blinkCountDown-- == 0)
            {
                pCurLed->blinkCountDown = kFastBlinkCount;
                ledToggle(pCurLed);
            }
            gTimerOnRequests++;
            spin_unlock_irqrestore(&brcm_ledlock, flags);
            break;

        case kLedStateUserWpsInProgress:          /* 200ms on, 100ms off */
            if (pCurLed->blinkCountDown-- == 0)
            {
                pCurLed->blinkCountDown = (((gLedRunningCounter++)&1)? kFastBlinkCount: kSlowBlinkCount);
                ledToggle(pCurLed);
            }
            gTimerOnRequests++;
            spin_unlock_irqrestore(&brcm_ledlock, flags);
            break;             

        case kLedStateUserWpsError:               /* 100ms on, 100ms off */
            if (pCurLed->blinkCountDown-- == 0)
            {
                pCurLed->blinkCountDown = kFastBlinkCount;
                ledToggle(pCurLed);
            }
            gTimerOnRequests++;
            spin_unlock_irqrestore(&brcm_ledlock, flags);
            break;        

        case kLedStateUserWpsSessionOverLap:      /* 100ms on, 100ms off, 5 times, off for 500ms */        
            if (pCurLed->blinkCountDown-- == 0)
            {
                if(((++gLedRunningCounter)%10) == 0) {
                    pCurLed->blinkCountDown = 4;
                    setLed(pCurLed, kLedOff, kLedGreen);
                    pCurLed->ledState = kLedStateUserWpsSessionOverLap;
                }
                else
                {
                    pCurLed->blinkCountDown = kFastBlinkCount;
                    ledToggle(pCurLed);
                }
            }
            gTimerOnRequests++;
            spin_unlock_irqrestore(&brcm_ledlock, flags);
            break;        

        default:
            spin_unlock_irqrestore(&brcm_ledlock, flags);
            printk("Invalid state = %d\n", pCurLed->ledState);
        }
    }

    // Restart the timer if any of our previous LED operations required
    // us to restart the timer, or if any other threads requested a timer
    // restart.  Note that throughout this function, gTimerOn == TRUE, so
    // any other thread which want to restart the timer would only
    // increment the gTimerOnRequests and not call ledTimerStart.
    spin_lock_irqsave(&brcm_ledlock, flags);
    if (gTimerOnRequests > 0)
    {
        ledTimerStart();
        gTimerOnRequests = 0;
    }
    else
    {
        gTimerOn = FALSE;
    }
    spin_unlock_irqrestore(&brcm_ledlock, flags);
}

void __init boardLedInit(void)
{
    PBP_LED_INFO pInfo;
    unsigned short i;
    short gpio;

    spin_lock_init(&brcm_ledlock);

    gLedCtrl = (PLED_CTRL) kmalloc((kLedEnd * sizeof(LED_CTRL)), GFP_KERNEL);

    if( gLedCtrl == NULL ) {
        printk( "LED memory allocation error.\n" );
        return;
    }

    /* Initialize LED control */
    for (i = 0; i < kLedEnd; i++) {
        gLedCtrl[i].ledGreenGpio = -1;
        gLedCtrl[i].ledRedGpio = -1;
        gLedCtrl[i].ledState = kLedStateOff;
        gLedCtrl[i].blinkCountDown = 0;            // reset the blink count down
    }

    for( pInfo = bpLedInfo; pInfo->ledName != kLedEnd; pInfo++ ) {
        if( pInfo->bpFunc && (*pInfo->bpFunc) (&gpio) == BP_SUCCESS )
        {
            gLedCtrl[pInfo->ledName].ledGreenGpio = gpio;
        }
        if( pInfo->bpFuncFail && (*pInfo->bpFuncFail)(&gpio)==BP_SUCCESS )
        {
            gLedCtrl[pInfo->ledName].ledRedGpio = gpio;
        }
        /* this will off power led, no need off these leds, they has been off at somewhere else.*/
#if !defined(CONFIG_TP_IMAGE)     
        setLed(&gLedCtrl[pInfo->ledName], kLedOff, kLedGreen);
        setLed(&gLedCtrl[pInfo->ledName], kLedOff, kLedRed);
#endif 
    }

#if defined(DEBUG_LED)
    for (i=0; i < kLedEnd; i++)
        printk("initLed: led[%d]: Gpio=0x%04x, FailGpio=0x%04x\n", i, gLedCtrl[i].ledGreenGpio, gLedCtrl[i].ledRedGpio);
#endif
    g_ledInitialized = 1;
}

// led ctrl.  Maps the ledName to the corresponding ledInfoPtr and perform the led operation
void boardLedCtrl(BOARD_LED_NAME ledName, BOARD_LED_STATE ledState)
{
    unsigned long flags;
    PLED_CTRL pLed;

    BCM_ASSERT_NOT_HAS_SPINLOCK_V(&brcm_ledlock);

    spin_lock_irqsave(&brcm_ledlock, flags);     // LED can be changed from ISR

    if( (int) ledName < kLedEnd ) {

        pLed = &gLedCtrl[ledName];

        // If the state is kLedStateFail and there is not a failure LED defined
        // in the board parameters, change the state to kLedStateSlowBlinkContinues.
        if( ledState == kLedStateFail && pLed->ledRedGpio == -1 )
            ledState = kLedStateSlowBlinkContinues;

        // Save current LED state
        pLed->ledState = ledState;

        // Start from LED Off and turn it on later as needed
        setLed (pLed, kLedOff, kLedGreen);
        setLed (pLed, kLedOff, kLedRed);

        // Disable HW control for WAN Data LED. 
        // It will be enabled only if LED state is On
#if defined(CONFIG_BCM963268) || defined(CONFIG_BCM96838)
        if (ledName == kLedWanData)
            LED->ledHWDis |= GPIO_NUM_TO_MASK(pLed->ledGreenGpio);
#endif

        switch (ledState) {
        case kLedStateOn:
            // Enable SAR to control INET LED
#if defined(CONFIG_BCM963268) || defined(CONFIG_BCM96838)
            if (ledName == kLedWanData)
                LED->ledHWDis &= ~GPIO_NUM_TO_MASK(pLed->ledGreenGpio);
#endif
            setLed (pLed, kLedOn, kLedGreen);
            break;

        case kLedStateOff:
            break;

        case kLedStateFail:
            setLed (pLed, kLedOn, kLedRed);
            break;

        case kLedStateSlowBlinkContinues:
            pLed->blinkCountDown = kSlowBlinkCount;
            gTimerOnRequests++;
            break;

        case kLedStateFastBlinkContinues:
            pLed->blinkCountDown = kFastBlinkCount;
            gTimerOnRequests++;
            break;

        case kLedStateUserWpsInProgress:          /* 200ms on, 100ms off */
            pLed->blinkCountDown = kFastBlinkCount;
            gLedRunningCounter = 0;
            gTimerOnRequests++;
            break;             

        case kLedStateUserWpsError:               /* 100ms on, 100ms off */
            pLed->blinkCountDown = kFastBlinkCount;
            gLedRunningCounter = 0;
            gTimerOnRequests++;
            break;        

        case kLedStateUserWpsSessionOverLap:      /* 100ms on, 100ms off, 5 times, off for 500ms */
            pLed->blinkCountDown = kFastBlinkCount;
            gTimerOnRequests++;
            break;        

        default:
            printk("Invalid led state\n");
        }
    }

    // If gTimerOn is false, that means ledTimerExpire has already finished
    // running and has not restarted the timer.  Then we can start it here.
    // Otherwise, we only leave the gTimerOnRequests > 0 which will be
    // noticed at the end of the ledTimerExpire function.
    if (!gTimerOn && gTimerOnRequests > 0)
    {
        ledTimerStart();
        gTimerOn = TRUE;
        gTimerOnRequests = 0;
    }
    spin_unlock_irqrestore(&brcm_ledlock, flags);
}

#if defined(CONFIG_NEW_LEDS)
#include <linux/leds.h>
#define LED_AUTONAME_MAX_SIZE    25
#define MAX_LEDS 64


struct sysfsled {
    struct led_classdev cdev;
    int bcm_led_pin; /* This also means pins connected on a shift
    register controlled by the LED controller (see BP_GPIO_SERIAL).*/
#if defined(CONFIG_TP_IMAGE)
    struct timer_list timer;
#endif 
    char name[LED_AUTONAME_MAX_SIZE];
};

struct sysfsled_ctx {
    int    num_leds;
    struct sysfsled leds[MAX_LEDS];
};

static struct sysfsled_ctx bcmctx = {0, {}};

/* Herbert Yuan <yuanjianpeng@tp-link.com.cn>, 2017/5/31
 * BCM4908's led controller support hardware blink and brightness,
 * we add support to led class dev. bcm's blink is quite simple,
 * so here we just add a simple interface to support this, other than 
 * using the led on-shot trigger.
 */

#if defined(CONFIG_TP_IMAGE)

struct blink_t
{
    char *name;
    unsigned mHz;
    unsigned reg;
};

static struct blink_t hw_supported_blink[] = 
{
    {"off",         0,      0},     /* {name, mHz, reg} */
    {"xx-slow",     390,    7},     
    {"x-slow",      781,    6},
    {"slow",        1562,   5},
    {"normal",      3125,   4},
    {"fast",        6250,   3},
    {"x-fast",      12500,  2},
    {"xx-fast",     25000,  1},
};

void setBrightness(unsigned short pin, unsigned brt)
{
    unsigned long flags;
    spin_lock_irqsave(&bcm_gpio_spinlock, flags);
    bcm_led_driver_blink(pin, 0);
    if (brt) {
        bcm_led_driver_brightness(pin, brt);
        bcm_led_driver_set(pin, 1);
    }
    else {
        bcm_led_driver_set(pin, 0);
    }
    spin_unlock_irqrestore(&bcm_gpio_spinlock, flags);
}

void setBlink(unsigned short pin, unsigned reg)
{
    unsigned long flags;
    spin_lock_irqsave(&bcm_gpio_spinlock, flags);
    bcm_led_driver_blink(pin, reg);
    bcm_led_driver_set(pin, 1);
    spin_unlock_irqrestore(&bcm_gpio_spinlock, flags);
}

static void timer_expired(unsigned long data)
{
    unsigned long flags;
     unsigned short pin = (unsigned short)data;
     unsigned action = data >> (8 * sizeof(unsigned short));
     
     spin_lock_irqsave(&bcm_gpio_spinlock, flags);
     if (action) {
         bcm_led_driver_set(pin, 1);
     }
     else {
         bcm_led_driver_set(pin, 0);
     }
     spin_unlock_irqrestore(&bcm_gpio_spinlock, flags);
}

static ssize_t blink_get(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    struct led_classdev *led_cdev = dev_get_drvdata(dev);
    int i;
    ssize_t n = 0;

    if (led_cdev->blink_delay_on >= sizeof(hw_supported_blink)/sizeof(hw_supported_blink[0]))
        return -1;
    
    n = sprintf(buf, "%u mHz: ", hw_supported_blink[led_cdev->blink_delay_on].mHz);
    for (i = 0; i < sizeof(hw_supported_blink)/sizeof(hw_supported_blink[0]); i++) {
        char *fmt = NULL;
        if (i == 0)
            fmt = "%u(%s)";
        else if (i == sizeof(hw_supported_blink)/sizeof(hw_supported_blink[0]) - 1)
            fmt = ", %u(%s)\n";
        else
            fmt = ", %u(%s)";
        n += snprintf(buf + n, PAGE_SIZE - n, fmt, hw_supported_blink[i].mHz, hw_supported_blink[i].name);
    }

    return n;
}

static ssize_t blink_set(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t size)
{
    struct led_classdev *led_cdev = dev_get_drvdata(dev);
    struct sysfsled *led = container_of(led_cdev, struct sysfsled, cdev);
    unsigned long mhz;
    int i;
    int ret;
    int pin = led->bcm_led_pin & BP_GPIO_NUM_MASK;

    if (pin > 31)
        return -EOPNOTSUPP;

    for (i = 0; i < sizeof(hw_supported_blink)/sizeof(hw_supported_blink[0]); i++) 
    {
        /* echo will append a \n to the tail */
        if (size >= strlen(hw_supported_blink[i].name) &&
            memcmp(buf, hw_supported_blink[i].name, strlen(hw_supported_blink[i].name)) == 0)
            break;
    }

    if ( i >= sizeof(hw_supported_blink)/sizeof(hw_supported_blink[0])) 
    {
        ret = kstrtoul(buf, 0, &mhz);
        if (ret)
            return ret;     
        
        for (i = 0; i < sizeof(hw_supported_blink)/sizeof(hw_supported_blink[0]); i++) {
            if (mhz <= hw_supported_blink[i].mHz)
                break;
        }

        if ( i >= sizeof(hw_supported_blink)/sizeof(hw_supported_blink[0])) 
            i = sizeof(hw_supported_blink)/sizeof(hw_supported_blink[0]) - 1;
        
        /* round to a nearest value */
        else if ( i > 0 && mhz < (hw_supported_blink[i].mHz + hw_supported_blink[i - 1].mHz) / 2)
            i--;
    }

    /* using blink_delay_on store the blink index */
    led_cdev->blink_delay_on = i;
    setBlink(led->bcm_led_pin, hw_supported_blink[i].reg);
    return size;
}

static ssize_t timer_set(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t size)
{
    struct led_classdev *led_cdev = dev_get_drvdata(dev);
    struct sysfsled *led = container_of(led_cdev, struct sysfsled, cdev);
    unsigned ms = 0;
    unsigned action = 0;
    char buf2[128];

    if (size > sizeof(buf2) - 1)
        return -1;
    memcpy(buf2, buf, size);    
    buf2[size] = '\0';
    
    if (2 != sscanf(buf, "%u %u", &ms, &action))
        return -1;

    del_timer_sync(&led->timer);
    if (ms > 0) {
        led->timer.expires = jiffies + msecs_to_jiffies(ms); 
        led->timer.data = ((unsigned short)led->bcm_led_pin) | (action << ( 8*sizeof(unsigned short)));
        add_timer(&led->timer);
    }
    return size;
}

static DEVICE_ATTR(blink, 0644, blink_get, blink_set);
static DEVICE_ATTR(timer, 0200, NULL, timer_set);

#endif      /* #if defined(CONFIG_TP_IMAGE) */

static void brightness_set(struct led_classdev *cdev, enum led_brightness value)
{
    struct sysfsled *led = container_of(cdev,
                    struct sysfsled, cdev);

#if defined(CONFIG_TP_IMAGE)
    setBrightness(led->bcm_led_pin, (unsigned)value);
#else   
    /* On Broadcom chips, LEDs connected to any kind of pin can be
     controlled via setLed. setLed only needs the pin ("GPIO") from
     the LED_CTRL argument. Fake a LED_CTRL to pass the pin to setLed.
     The color obviously doesn't matter here. */
    LED_CTRL ledctrl = {.ledGreenGpio=led->bcm_led_pin};
    setLed(&ledctrl, (value == LED_FULL)?kLedOn:kLedOff, kLedGreen);
#endif 
}

static void __exit bcmsysfsleds_unregister(void *opaque)
{
    struct sysfsled_ctx *ctx = opaque;
    int i;

    for (i = 0; i < MAX_LEDS; i++) {
        if (ctx->leds[i].cdev.brightness_set) {
            led_classdev_unregister(&ctx->leds[i].cdev);
            ctx->leds[i].cdev.brightness_set = NULL;
        }
    }
}

static int __init bcmsysfsleds_register(int bcm_led_pin, char *led_name)
{
    int i;

    if (bcmctx.num_leds >= MAX_LEDS) {
        printk(KERN_ERR "Too many BCM LEDs registered.\n");
        return -ENOMEM;
    }

    if(led_name != NULL) {
        i = bcmctx.num_leds;
#if defined(CONFIG_TP_IMAGE)
        /* we don't want the pin number appear in the name, the ledctrl doesn't care the pin number,
         * it want a fixed name.
         */
        strncpy(bcmctx.leds[i].name, led_name, LED_AUTONAME_MAX_SIZE-1);
#else        
        snprintf(bcmctx.leds[i].name, LED_AUTONAME_MAX_SIZE-1,
            "%s_%d%c", led_name, bcm_led_pin&BP_GPIO_NUM_MASK, (bcm_led_pin&BP_GPIO_SERIAL)?'s':'\0');
#endif 
        bcmctx.leds[i].cdev.name = bcmctx.leds[i].name;
        bcmctx.leds[i].cdev.brightness  = LED_OFF;
        bcmctx.leds[i].cdev.brightness_set = brightness_set;
        bcmctx.leds[i].bcm_led_pin = bcm_led_pin;
#if defined(CONFIG_TP_IMAGE)
        init_timer(&bcmctx.leds[i].timer);
        bcmctx.leds[i].timer.function = timer_expired;
        bcmctx.leds[i].cdev.max_brightness  = 8;
#endif

        if (led_classdev_register(NULL, &bcmctx.leds[i].cdev)) {
            printk(KERN_ERR "BCM LEDs registration failed. %d\n", bcm_led_pin&BP_GPIO_NUM_MASK);
            bcmctx.leds[i].cdev.brightness_set = NULL;
            return -1;
        }
#if defined(CONFIG_TP_IMAGE)
        device_create_file(bcmctx.leds[i].cdev.dev, &dev_attr_blink);   
        device_create_file(bcmctx.leds[i].cdev.dev, &dev_attr_timer);           
#endif        
        bcmctx.num_leds++;
    }

#if defined(CONFIG_TP_IMAGE)
    /* this led has been registered with a human friendly name, 
     * no need register the same one using the pin number as the name
     */
    return 0;
#endif

    i = bcmctx.num_leds;
    snprintf(bcmctx.leds[i].name, LED_AUTONAME_MAX_SIZE-1,
        "%d%c", bcm_led_pin&BP_GPIO_NUM_MASK, (bcm_led_pin&BP_GPIO_SERIAL)?'s':'\0');
    bcmctx.leds[i].cdev.name = bcmctx.leds[i].name;
    bcmctx.leds[i].cdev.brightness  = LED_OFF;
    bcmctx.leds[i].cdev.brightness_set = brightness_set;
    bcmctx.leds[i].bcm_led_pin = bcm_led_pin;
    if (led_classdev_register(NULL, &bcmctx.leds[i].cdev)) {
        printk(KERN_ERR "BCM LEDs registration failed. %d\n", bcm_led_pin&BP_GPIO_NUM_MASK);
        bcmctx.leds[i].cdev.brightness_set = NULL;
        return -1;
    }

    bcmctx.num_leds++;

    return 0;
}
#endif

int __init bcmsysfsleds_init(void)
{
#if defined(CONFIG_NEW_LEDS)
        unsigned short bcmledgpioid;
	int index=0, rc;
	void* token=NULL;
        char *ledName=NULL;
        for(;;) {
                ledName=NULL;
                rc = BpGetLedName(index, &token,  &bcmledgpioid, &ledName);
                if (rc == BP_MAX_ITEM_EXCEEDED) {
                    break;
                }
                if(rc == BP_SUCCESS) {
                        bcmsysfsleds_register(bcmledgpioid, ledName);
                }
                else {
                        index++;
                        token=NULL;
                }
        }
#endif
        return 0;
}

void __exit bcmsysfsleds_exit(void)
{
#if defined(CONFIG_NEW_LEDS)
    int i;

    for (i = 0; i < MAX_LEDS; i++) {
        if (bcmctx.leds[i].cdev.brightness_set) {
            led_classdev_unregister(&bcmctx.leds[i].cdev);
            bcmctx.leds[i].cdev.brightness_set = NULL;
        }
    }
#endif
}

module_init(bcmsysfsleds_init);
