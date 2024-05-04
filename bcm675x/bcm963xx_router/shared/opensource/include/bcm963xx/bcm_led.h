
#if !defined(_BCM_LED_H_)
#define _BCM_LED_H_

#define BCM_LED_OFF 0
#define BCM_LED_ON  1

void bcm_common_led_init(void);
void bcm_led_driver_set(unsigned short num, unsigned short state);
void bcm_led_driver_toggle(unsigned short num);
void bcm_common_led_setAllSoftLedsOff(void);
void bcm_common_led_setInitial(void);
short * bcm_led_driver_get_optled_map(void);
void bcm_ethsw_led_init(void);
void bcm_led_zero_flash_rate(int channel);
void bcm_led_set_source(unsigned int serial_sel, unsigned int hwled_sel);

#if defined(CONFIG_TP_IMAGE)
#define BLINK_FREQ_NORMAL	4	/* 3.125Hz */
void bcm_led_driver_brightness(unsigned short pin, unsigned brt);
void bcm_led_driver_blink (unsigned short pin, unsigned reg);
void bcm_led_deinit(void);
#endif

#endif  /* _BCM_LED_H_ */


