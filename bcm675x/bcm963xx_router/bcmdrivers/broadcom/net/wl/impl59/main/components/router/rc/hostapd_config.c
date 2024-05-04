/*
 * Router hostapd control script
 *
 * Copyright (C) 2019, Broadcom. All Rights Reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 *
 * <<Broadcom-WL-IPTag/Open:>>
 *
 * $Id: hostapd_config.c 774872 2019-05-09 09:37:32Z $
 */

#ifdef CONFIG_HOSTAPD

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <bcmnvram.h>
#include <shutils.h>
#include <wlutils.h>
#include <common_utils.h>
#include <wlif_utils.h>
#include <bcmutils.h>
#include <time.h>
#include <md5.h>
#ifdef BCA_HNDROUTER
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/poll.h>
#include <sys/ioctl.h>
#include <board.h>
#endif /* BCA_HNDROUTER */
#include <pthread.h>
#include <dirent.h>
#include <ctype.h>

#ifndef IFNAMSIZ
#define IFNAMSIZ			16
#endif	/* IFNAMSIZ */

#ifndef MAX_RADIO_NUM
#define MAX_RADIO_NUM			3	// Max num of radio interfaces
#endif /* MAX_RADIO_NUM */

#ifndef MAX
#define MAX(X, Y)			((X) > (Y) ? (X) : (Y))
#endif	/* MAX */

#ifndef MIN
#define MIN(X, Y)			((X) < (Y) ? (X) : (Y))
#endif	/* MIN */

/* Swap the bytes in a 16-bits word */
#define swap16(x) (\
	(uint16)(((uint16)(x) & 0xff) << 8 | ((uint16)(x) & 0xff00) >> 8)  \
)

#define	HAPD_MAX_BUF			512
#define	HAPD_MIN_BUF			128
#define WPA2_PSK_HEXLEN			64

#define HAPD_DIR			"/var/run/hostapd"
#define HAPD_FILE_DIR			"/tmp"
#define HAPD_FILENAME_SUFFIX		"hapd.conf"
#define HAPD_PSKFILE_SUFFIX		"hapd.psk"
#define HAPD_PINFILE_SUFFIX		"hapd.pin-req"

// Config flags
#define HAPD_CFG_DEFER			0x1	// Flag to handle a prev entry to defer
#define HAPD_CFG_IFR			0x2	// Flag to indicate interface specific settings
#define	HAPD_CFG_BSS			0x4	// Flag to indicate bss specific settings
#define HAPD_CFG_PERBSS			0x8	// Flag to indicate that nvram is per bss
#define HAPD_CFG_USEDEFAULT		0x10	// Flag to write the default value in config file
#define HAPD_CFG_COMMON			(HAPD_CFG_BSS | HAPD_CFG_IFR)
#define HAPD_CFG_LRG_BUF		0x20	// Flag to allocate large size buffer to write to
						//  conf file
#define WPA_SUP_CFG_GBL			0x40	// Flag to indicate global settings for supplicant
// Flag to indicate if need to check psk or passphrase/cb_fn
#define WPA_CFG_PSK_CHECK		0x80

// auth algo
#define HAPD_AUTH_OPEN_SYSTEM		1
#define HAPD_AUTH_SHARED_KEY		2

// Operation mode or hw_mode
#define	HAPD_HW_MODE_80211_A		"a"	// When operating in 5G and HT or VHT phy
#define HAPD_HW_MODE_80211_G		"g"	// when Operating in 2.4G for 802.11n phy
#define HAPD_HW_MODE_80211_B		"b"	// When operating in 2.4G for 802.11n phy

// Internal akm value bitflags
#define HAPD_AKM_OPEN_WEP		0x0
#define HAPD_AKM_PSK			0x1
#define HAPD_AKM_PSK2			0x2
#define HAPD_AKM_WPA3_SAE		0x4
#define HAPD_AKM_WPA3_SAE_FT		0x8
#define HAPD_AKM_PSK2_FT		0x10

/* Start of Enterprise akm */
#define HAPD_AKM_WPA			0x20
#define HAPD_AKM_WPA2			0x40
#define HAPD_AKM_WPA2_OSEN		0x80

// Security type
#define	HAPD_SEC_OPEN			0	// Open Security system
#define	HAPD_SEC_WPA			1	// IEEE 802.11i/D3.0
#define HAPD_SEC_WPA2			2	// Full  IEEE 802.11i/RSN

// Set of accepted key management algorithms WPA-PSK, WPA-EAP, or both
#define HAPD_KEY_MGMT_WPA		"WPA-PSK"
#define HAPD_KEY_MGMT_EAP		"WPA-EAP"
#define HAPD_KEY_MGMT_WPA_PSK_FT	"WPA-PSK FT-PSK"
#define HAPD_KEY_MGMT_EAP_FT		"WPA-EAP FT-EAP"
#define HAPD_KEY_MGMT_WPA_SHA256	"WPA-PSK-SHA256"
#define HAPD_KEY_MGMT_EAP_SHA256	"WPA-EAP-SHA256"
#define HAPD_KEY_MGMT_SAE		"SAE"
#define HAPD_KEY_MGMT_WPA_PSK_SAE	"WPA-PSK SAE"
#define HAPD_KEY_MGMT_WPA_SHA256_SAE	"WPA-PSK-SHA256 SAE"
#define HAPD_KEY_MGMT_FT_SAE		"FT-SAE"
#define HAPD_KEY_MGMT_OSEN		"OSEN"

#define	HAPD_CIPHER_SUITE_CCMP		"CCMP"
#define	HAPD_CIPHER_SUITE_TKIP		"TKIP"

#define HAPD_IEEE8021X_SUPPORTED	"1"
#define HAPD_SUPPORTED			"1"
#define HAPD_UNSUPPORTED		"0"

// Wps device pin default value
#define HAPD_WPS_DEVICE_PIN_DEFAULT_VAL	"12345670"
#define HAPD_WPS_DEVICE_PIN_LEN		8

// Wps states
#define	HAPD_WPS_DISABLED		0
#define	HAPD_WPS_UNCONFIGURED		1
#define HAPD_WPS_CONFIGURED		2

// Wps configuration methods
#define HAPD_WPS_CONFIG_LABEL		0x0004	/* Label */
#define HAPD_WPS_CONFIG_DISPLAY		0x0008	/* Display */
#define HAPD_WPS_CONFIG_PBC		0x0080	/* Push btn either physical or virtual */
#define HAPD_WPS_CONFIG_KEYPAD		0x0100	/* Keypad */
#define HAPD_WPS_CONFIG_VPBC		0x0280	/* Virtual push button */
#define HAPD_WPS_CONFIG_PHYPBC		0x0480	/* Physical push button */
#define HAPD_WPS_CONFIG_VPIN		0x2008	/* Virtual display pin */
#define HAPD_WPS_CONFIG_PHYPIN		0x4008	/* Physical display pin */

// Wps auth types
#define HAPD_WPS_AUTH_OPEN		0x01
#define HAPD_WPS_AUTH_WPAPSK		0x02
#define HAPD_WPS_AUTH_WPA2PSK		0x20

//Wps encryption types
#define HAPD_WPS_ENCR_NONE		0x01
#define HAPD_WPS_ENCR_TKIP		0x04
#define HAPD_WPS_ENCR_AES		0x08

// Wps cred processing attribute values
#define HAPD_WPS_DO_NOT_PROCESS_CREDS_PASS_TO_CTRL_INTERFACE		"1"
#define HAPD_WPS_PROCESS_CREDS_AND_PASS_TO_CTRL_INTERFACE		"2"

#define WPA_SUPP_FILE_DIR		"/tmp"
#define WPA_SUPP_FILENAME_SUFFIX	"wpa_supplicant.conf"

// Security type
#define	WPA_SUPP_SEC_OPEN_OR_WEP	"NONE"	// Open Security or WEP
#define	WPA_SUPP_SEC_WPA		"WPA"	// IEEE 802.11i/D3.0
#define WPA_SUPP_SEC_WPA2		"WPA2"  // Full  IEEE 802.11i/RSN

#define HAPD_START_DEFER		4	// Defer hostapd start by so much sec

#ifndef HAPD_WPASUPP_DBG
#define HAPD_WPASUPP_DBG
#endif /* HAPD_WPASUPP_DBG */

#define HSFLG_DGAF_DS			10      /* DGAF BIT in HS FLAG.
						* Should be in sync with passpoint
						*/
#define HAPD_ENABLE			1
#define HAPD_DISABLE			0

#define HAPD_MFP_OFF			0	// PMF Off
#define HAPD_MFP_CAP			1	// PMF Capable
#define HAPD_MFP_REQ			2	// PMF Required

#ifdef HAPD_WPASUPP_DBG
#define PRINT_IFLIST(iflist)	do {		\
	char ifname[IFNAMSIZ] = {0};		\
	char *next = NULL;			\
	foreach(ifname, iflist, next) {		\
		printf("%s ", ifname);		\
	}					\
	printf("\n");				\
} while (0);
#endif /* HAPD_WPASUPP_DBG */

#define HAPD_ON_PRIMARY_IFACE		0x0	/* run hostapd on primary interface */
#define HAPD_ON_VIRTUAL_IFACE		0x1	/* run hostapd on virtual interface */

#define WPAX_FIELDS_NUM			6	/* Non WEP - WPA/WPA2/WPA3 and ptk,gtk rekey */
#define SAE_FIELDS_NUM			4
#define FBT_FIELDS_NUM			9
#define RADIUS_FIELDS_NUM		7
#define PREAUTH_FIELDS_NUM		1
#define WPS_FIELDS_NUM			18
#define MAP_FIELDS_NUM			5

extern void RAND_bytes(unsigned char *buf, int num);

typedef struct hapd_nvram_config hapd_nvram_config_t;

/** Fn ptr for the conversion functions
 * @param nvi_ifname		interface name in nvi infame form.
 * @param name			nvram name.
 * @out_val			output value after conversion.
 * @out_sz			length of the out_val buffer.
 *
 * @return			0 on success -1 on error any +ve value implies next defer count.
 */
typedef int (*hapd_convert_fn)(char *nvi_ifname, char *name, char *out_val, int out_sz);

/* hostapd callbacks */
static int hapd_br_conv_fn(char *nvi_ifname, char *name, char *out_val, int out_sz);
static int hapd_phytype_conv_fn(char *nvi_ifname, char *name, char *out_val, int out_sz);
static int hapd_channel_conv_fn(char *nvi_ifname, char *name, char *out_val, int out_sz);
static int hapd_auth_conv_fn(char *nvi_ifname, char *name, char *out_val, int out_sz);
static int hapd_wep_conv_fn(char *nvi_ifname, char *name, char *out_val, int out_sz);
static int hapd_wep_key_conv_fn(char *nvi_ifname, char *name, char *out_val, int out_sz);
static int hapd_fbt_enabled(char *nvi_ifname);
static int hapd_fbt_mdid_conv_fn(char *nvi_ifname, char *name, char *out_val, int out_sz);
static int hapd_r0kh_id_conv_fn(char *nvi_ifname, char *name, char *out_val, int out_sz);
static int hapd_r1kh_id_conv_fn(char *nvi_ifname, char *name, char *out_val, int out_sz);
static int hapd_fbt_reassoc_time_conv_fn(char *nvi_ifname, char *name, char *out_val, int out_sz);
static int hapd_fbtoverds_conv_fn(char *nvi_ifname, char *name, char *out_val, int out_sz);
static int hapd_fbtap_r0kh_conv_fn(char *nvi_ifname, char *name, char *out_val, int out_sz);
static int hapd_fbtap_r1kh_conv_fn(char *nvi_ifname, char *name, char *out_val, int out_sz);
static int hapd_fbtap_psk_generate_local(char *nvi_ifname, char *name, char *out_val, int out_sz);
static int hapd_key_mgmt_conv_fn(char *nvi_ifname, char *name, char *out_val, int out_sz);
static int hapd_preauth_conv_fn(char *nvi_ifname, char *name, char *out_val, int out_sz);
static int hapd_preauth_interfaces_conv_fn(char *nvi_ifname, char *name, char *out_val, int out_sz);
static int hapd_radius_akm_conv_fn(char *nvi_ifname, char *name, char *out_val, int out_sz);
static int hapd_wpsstate_conv_fn(char *nvi_ifname, char *name, char *out_val, int out_sz);
static int hapd_wps_aplockdown_conv_fn(char *nvi_ifname, char *name, char *out_val, int out_sz);
static int hapd_wps_device_pin_conv_fn(char *nvi_ifname, char *name, char *out_val, int out_sz);
static int hapd_wps_rf_bands_conv_fn(char *nvi_ifname, char *name, char *out_val, int out_sz);
static int hapd_wpsuuid_conv_fn(char *nvi_ifname, char *name, char *out_val, int out_sz);
static int hapd_wpsconfig_method_conv_fn(char *nvi_ifname, char *name, char *out_val, int out_sz);
static int hapd_mfp_conv_fn(char *nvi_ifname, char *name, char *out_val, int out_sz);
#if defined(MULTIAP)
static int hapd_map_conv_fn(char *nvi_ifname, char *name, char *out_val, int out_sz);
static int hapd_wpsmap_conv_fn(char *nvi_ifname, char *name, char *out_val, int out_sz);
static int wpa_supp_map_conv_fv(char *nvi_ifname, char *name, char *out_val, int out_sz);
static int wpa_supp_scan_ssid_conv_fn(char *nvi_ifname, char *name, char *out_val, int out_sz);
#endif	/* MULTIAP */
static int hapd_wmmconfig_conv_fn(char *nvi_ifname, char *name, char *out_val, int out_sz);
static int hapd_osen_conv_fn(char *nvi_ifname, char *name, char *out_val, int out_sz);
static int hapd_dgaf_conv_fn(char *nvi_ifname, char *name, char *out_val, int out_sz);
static int hapd_11d_conv_fn(char *nvi_ifname, char *name, char *out_val, int out_sz);
static int hapd_11h_conv_fn(char *nvi_ifname, char *name, char *out_val, int out_sz);
static int hapd_cc_conv_fn(char *nvi_ifname, char *name, char *out_val, int out_sz);
static int hapd_dtim_conv_fn(char *nvi_ifname, char *name, char *out_val, int out_sz);
static int hapd_bi_conv_fn(char *nvi_ifname, char *name, char *out_val, int out_sz);

/* hostapd, wpa_supplicant common helper functions */
static bool hapd_wpasupp_is_ifce_ap(const char *ifname);
static bool hapd_wpasupp_is_bss_enabled(const char* ifname);
static bool hapd_wpasupp_is_primary_ifce(const char *ifname);
static void hapd_wpasupp_get_security_details(char *nvi_ifname, int *out_akm);
static void hapd_wpasupp_set_deferred(int cur_pos, int count);
static void hapd_wpasupp_get_filtered_ifnames_list(char *iflist, char *fltrd_iflist, int flist_sz);
static void hapd_wpasupp_get_primary_virtual_iflist(char *flist, char *plist, char *slist, int sz);
static void hapd_wpasupp_get_radio_list(char *ifnames_list, char *rlist, int rlistsz, int idx);
static int hapd_wpasupp_get_primary_ifce(char *nvi_ifname, char *wlpr_ifname, int sz);

/* hostapd helper functions */
static void hapd_get_key_mgmt(char *nvi_ifname, int fbt_enabled, int akm, char *key_mgmt);
static void hapd_fill_nvrams_to_config_file(FILE *fp, char *name, uint32 flags);
static int hapd_get_config_filename(char *ifname, char *o_fname, int sz, uint32 *o_flgs, int mode);
static int hapd_create_config_file(char *ifname, char *filename, uint32 flags);

/* wpa_supplicant helper functions */
static int wpa_supp_create_config_file(char *prefix, char *filename, uint32 flags);
static int wpa_supp_get_config_filename(char *prefix, char *o_fname, int size, uint32 *o_flgs);
static void wpa_supp_set_deferred(int cur_pos, int count);
static void wpa_supp_fill_nvrams_to_config_file(FILE *fp, char *ifname, uint32 flags,
	hapd_nvram_config_t *cfg_arr);

/* wpa_supplicant callbacks */
static int wpa_supp_wps_cred_processing_conv_fn(char *nvi_ifname, char *name,
	char *out_val, int out_sz);
static int wpa_supp_key_mgmt_conv_fn(char *nvi_ifname, char *name, char *out_val, int out_sz);
static int wpa_supp_ssid_conv_fn(char *nvi_ifname, char *nv_name, char *out_val, int out_sz);

static int start_hostapd(char *fltrd_iflist, char *pri_iflist, char *vr_iflist);
static int start_wpa_supplicant(char *primary_iflist);

static void stop_hostapd_per_radio(int radio_idx);
static void stop_wpa_supplicant_per_radio(int radio_idx);

int start_hapd_wpasupp();
void stop_hapd_wpasupp();
int start_hapd_wpasupp_per_radio(int radio_idx);
void stop_hapd_wpasupp_per_radio(int radio_idx);

struct hapd_nvram_config
{
	char *nv_name;			/* Nvram Name */
	char *placeholder;		/* Hostapd config file placeholder */
	uint32 flags;			/* Flags of type CFG_XXX */
	char *def_val;			/* Default value can be used for testing, in some cases
					 * we can directly write into hostapd config file
					 */
	hapd_convert_fn cb_fn;		/* Convertion function ptr */
};

/* In a repeater with multiple virtual/secondary interfaces as BSS' AP, the radio parameters such as
 * interface, hw_mode and channel fields of hostapd.conf file are to be filled only once for the
 * first virtual interface. Rest of BSS specific settings are filled as usual for all the BSS'.
 * In this case, the first virtual interface acts like a regualr primary interface (ethX)
 */

bool radio_params_set[MAX_RADIO_NUM] = {FALSE};

/* cache primary ifname's in wlX format */
char ifname_arr[MAX_RADIO_NUM][IFNAMSIZ] = {{0}};

/*
 * Presently the nvram order is important as for nvram's some specific value
 * we are defering the next array elements.
 * For exp: when wep is disabled we are defering key1, key2 key3 and key4 entries.
 */
static hapd_nvram_config_t cfg_arr[] =
{
	{"ifname", "interface", (HAPD_CFG_IFR | HAPD_CFG_PERBSS), "eth0", NULL},
	{"phytype", "hw_mode", (HAPD_CFG_IFR | HAPD_CFG_PERBSS), "g", hapd_phytype_conv_fn},
	{"chanspec", "channel", (HAPD_CFG_IFR | HAPD_CFG_PERBSS), "11", hapd_channel_conv_fn},
	{"lan_ifname", "bridge", HAPD_CFG_IFR, "br0", hapd_br_conv_fn},
	{"country_code", "country_code", (HAPD_CFG_IFR | HAPD_CFG_PERBSS), "US", hapd_cc_conv_fn},
	{"reg_mode", "ieee80211d", (HAPD_CFG_IFR | HAPD_CFG_PERBSS), "1", hapd_11d_conv_fn},
	{"reg_mode", "ieee80211h", (HAPD_CFG_IFR | HAPD_CFG_PERBSS), "1", hapd_11h_conv_fn},
	{"bcn", "beacon_int", (HAPD_CFG_IFR | HAPD_CFG_PERBSS), "100", hapd_bi_conv_fn},
	{"ifname", "bss", (HAPD_CFG_BSS | HAPD_CFG_PERBSS), "wl0.1", NULL},
	{"dtim", "dtim_period", (HAPD_CFG_COMMON | HAPD_CFG_PERBSS), "2", hapd_dtim_conv_fn},
	{"", "ctrl_interface", (HAPD_CFG_COMMON | HAPD_CFG_PERBSS | HAPD_CFG_USEDEFAULT),
		HAPD_DIR, NULL},
	{"hwaddr", "bssid", (HAPD_CFG_BSS | HAPD_CFG_PERBSS), "00:00:00:00:00:00", NULL},
	{"ssid", "ssid", (HAPD_CFG_COMMON | HAPD_CFG_PERBSS), "Broadcom", NULL},
	{"auth", "auth_algs", (HAPD_CFG_COMMON | HAPD_CFG_PERBSS), "1", hapd_auth_conv_fn},
	{"ap_isolate", "ap_isolate", (HAPD_CFG_COMMON | HAPD_CFG_PERBSS), "0", NULL},
	{"closed", "ignore_broadcast_ssid", (HAPD_CFG_COMMON | HAPD_CFG_PERBSS), "0", NULL},
	// wep_def_key and from key0 till key4 vals are dependent on the wep nvram.
	{"key", "wep_default_key", (HAPD_CFG_COMMON | HAPD_CFG_PERBSS), "0", hapd_wep_conv_fn},
	{"key1", "wep_key0", (HAPD_CFG_COMMON | HAPD_CFG_PERBSS), "1234567890",
		hapd_wep_key_conv_fn},
	{"key2", "wep_key1", (HAPD_CFG_COMMON | HAPD_CFG_PERBSS), "1234567890",
		hapd_wep_key_conv_fn},
	{"key3", "wep_key2", (HAPD_CFG_COMMON | HAPD_CFG_PERBSS), "1234567890",
		hapd_wep_key_conv_fn},
	{"key4", "wep_key3", (HAPD_CFG_COMMON | HAPD_CFG_PERBSS), "1234567890",
		hapd_wep_key_conv_fn},
	// wpa, akm rsn and wpa_psk vals fbt are dependent on the akm and wep nvram.
	{"wpa",	"wpa", (HAPD_CFG_COMMON | HAPD_CFG_PERBSS), "0", hapd_key_mgmt_conv_fn},
	{"akm",	"wpa_key_mgmt",	(HAPD_CFG_COMMON | HAPD_CFG_PERBSS),
		"WPA-PSK", hapd_key_mgmt_conv_fn},
	{"crypto", "wpa_pairwise", (HAPD_CFG_COMMON | HAPD_CFG_PERBSS),
		"CCMP", hapd_key_mgmt_conv_fn},
	{"wpa_gtk_rekey", "wpa_group_rekey", (HAPD_CFG_COMMON | HAPD_CFG_PERBSS), "0", NULL},
	{"wpa_ptk_rekey", "wpa_ptk_rekey", (HAPD_CFG_COMMON | HAPD_CFG_PERBSS), "0", NULL},
	{"wpa_psk", "wpa_passphrase", (HAPD_CFG_COMMON | HAPD_CFG_PERBSS | WPA_CFG_PSK_CHECK),
		"12345678", hapd_key_mgmt_conv_fn},
	{"mfp",	"sae_require_mfp", (HAPD_CFG_COMMON | HAPD_CFG_PERBSS), "0", hapd_key_mgmt_conv_fn},
	{"sae_anti_clog_threshold", "sae_anti_clogging_threshold",
		(HAPD_CFG_COMMON | HAPD_CFG_PERBSS), "5", NULL},
	{"sae_sync", "sae_sync", (HAPD_CFG_COMMON | HAPD_CFG_PERBSS), "5", NULL},
	{"sae_groups", "sae_groups", (HAPD_CFG_COMMON | HAPD_CFG_PERBSS), "19", NULL},
	{"mfp",	"ieee80211w", (HAPD_CFG_COMMON | HAPD_CFG_PERBSS), "0", hapd_mfp_conv_fn},
	{"fbt_mdid", "mobility_domain", (HAPD_CFG_COMMON | HAPD_CFG_PERBSS), NULL,
			hapd_fbt_mdid_conv_fn},
	{"r0kh_id", "nas_identifier", (HAPD_CFG_COMMON | HAPD_CFG_PERBSS), NULL,
			hapd_r0kh_id_conv_fn},
	{"r1kh_id", "r1_key_holder", (HAPD_CFG_COMMON | HAPD_CFG_PERBSS), NULL,
			hapd_r1kh_id_conv_fn},
	{"fbt_reassoc_time", "reassociation_deadline", (HAPD_CFG_COMMON | HAPD_CFG_PERBSS),
			"1000", hapd_fbt_reassoc_time_conv_fn},
	{"fbtoverds", "ft_over_ds", (HAPD_CFG_COMMON | HAPD_CFG_PERBSS), NULL,
			hapd_fbtoverds_conv_fn},
	{"r0kh", "r0kh", (HAPD_CFG_COMMON | HAPD_CFG_PERBSS | HAPD_CFG_LRG_BUF), NULL,
			hapd_fbtap_r0kh_conv_fn},
	{"r1kh", "r1kh", (HAPD_CFG_COMMON | HAPD_CFG_PERBSS | HAPD_CFG_LRG_BUF), NULL,
			hapd_fbtap_r1kh_conv_fn},
	{"fbt_generate_local", "ft_psk_generate_local", (HAPD_CFG_COMMON | HAPD_CFG_PERBSS), "0",
			hapd_fbtap_psk_generate_local},
	{"", "pmk_r1_push", (HAPD_CFG_COMMON | HAPD_CFG_PERBSS | HAPD_CFG_USEDEFAULT), "1", NULL},
	// Setting for radius server
	{"akm", "ieee8021x", (HAPD_CFG_COMMON | HAPD_CFG_PERBSS), "0", hapd_radius_akm_conv_fn},
	{"preauth", "rsn_preauth", (HAPD_CFG_COMMON | HAPD_CFG_PERBSS), "1", hapd_preauth_conv_fn},
	{"preauth", "rsn_preauth_interfaces", (HAPD_CFG_COMMON | HAPD_CFG_PERBSS), "",
		hapd_preauth_interfaces_conv_fn},
	{"own_ip_addr", "own_ip_addr", (HAPD_CFG_COMMON | HAPD_CFG_USEDEFAULT), "127.0.0.1", NULL},
	{"radius_ipaddr", "auth_server_addr", (HAPD_CFG_COMMON | HAPD_CFG_PERBSS), "0.0.0.0", NULL},
	{"radius_port", "auth_server_port", (HAPD_CFG_COMMON | HAPD_CFG_PERBSS), "1812", NULL},
	{"radius_key", "auth_server_shared_secret", (HAPD_CFG_COMMON | HAPD_CFG_PERBSS),
		"12345678", NULL},
	{"hsflag", "disable_dgaf", (HAPD_CFG_COMMON | HAPD_CFG_PERBSS), "0", hapd_dgaf_conv_fn},
	{"hs20_deauth_req_timeout", "hs20_deauth_req_timeout",
			(HAPD_CFG_COMMON | HAPD_CFG_PERBSS), "0", NULL},
	{"akm", "osen", (HAPD_CFG_COMMON | HAPD_CFG_PERBSS), "0", hapd_osen_conv_fn},
	// Settings for wps
	{"wps_mode", "wps_state", (HAPD_CFG_COMMON | HAPD_CFG_PERBSS), "2", hapd_wpsstate_conv_fn},
	{"", "wps_independent", (HAPD_CFG_COMMON | HAPD_CFG_USEDEFAULT), "1", NULL},
	{"wps_aplockdown", "ap_setup_locked", (HAPD_CFG_COMMON), "0", hapd_wps_aplockdown_conv_fn},
	{"", "uuid", (HAPD_CFG_COMMON), "", hapd_wpsuuid_conv_fn},
	{"wps_device_name", "device_name", (HAPD_CFG_COMMON), "BroadcomAP", NULL},
	{"wps_mfstring", "manufacturer", (HAPD_CFG_COMMON), "Broadcom", NULL},
	{"wps_modelname", "model_name", (HAPD_CFG_COMMON), "Broadcom", NULL},
	{"wps_modelnum", "model_number", (HAPD_CFG_COMMON), "1234", NULL},
	{"boardnum", "serial_number", (HAPD_CFG_COMMON), "267", NULL},
	{"", "os_version", (HAPD_CFG_COMMON | HAPD_CFG_USEDEFAULT), "134217728", NULL},
	{"", "device_type", (HAPD_CFG_COMMON | HAPD_CFG_USEDEFAULT), "6-0050F204-1", NULL},
	{"wps_config_method", "config_methods", (HAPD_CFG_COMMON),
		"label display push_button keypad", hapd_wpsconfig_method_conv_fn},
	{"", "wps_cred_processing", (HAPD_CFG_COMMON | HAPD_CFG_USEDEFAULT),
		HAPD_WPS_PROCESS_CREDS_AND_PASS_TO_CTRL_INTERFACE, NULL},
	{"wps_device_pin", "ap_pin", (HAPD_CFG_COMMON), "", hapd_wps_device_pin_conv_fn},
	{"", "wps_rf_bands", (HAPD_CFG_COMMON), "", hapd_wps_rf_bands_conv_fn},
	{"", "upnp_iface", (HAPD_CFG_COMMON | HAPD_CFG_PERBSS), "ethx", hapd_br_conv_fn},
	{"", "friendly_name", (HAPD_CFG_COMMON | HAPD_CFG_USEDEFAULT), "WPS Access Point", NULL},
	{"", "eap_server", (HAPD_CFG_COMMON | HAPD_CFG_USEDEFAULT), "1", NULL},
#if defined(MULTIAP)
	{"map", "map", (HAPD_CFG_COMMON | HAPD_CFG_PERBSS), "1", hapd_map_conv_fn},
	{"bh_ssid", "map_bh_ssid", (HAPD_CFG_COMMON | HAPD_CFG_PERBSS), "BH_SSID",
		hapd_wpsmap_conv_fn},
	{"bh_akm", "map_bh_auth", (HAPD_CFG_COMMON | HAPD_CFG_PERBSS), "CCMP",
		hapd_wpsmap_conv_fn},
	{"bh_crypto", "map_bh_encr", (HAPD_CFG_COMMON | HAPD_CFG_PERBSS), "WPA-PSK",
		hapd_wpsmap_conv_fn},
	{"bh_psk", "map_bh_psk", (HAPD_CFG_COMMON | HAPD_CFG_PERBSS), "1@sa^hg_z",
		hapd_wpsmap_conv_fn},
#endif	/* MULTIAP */
	{"wme_bss_disable", "wmm_enabled", (HAPD_CFG_COMMON | HAPD_CFG_PERBSS), "0",
		hapd_wmmconfig_conv_fn},
	{0, 0, 0, 0, 0}
};

/* wpa_supplicant config array */
static hapd_nvram_config_t wpa_supp_cfg_arr[] =
{
	{"wps_device_name", "device_name", (WPA_SUP_CFG_GBL), "BroadcomAP", NULL},
	{"wps_mfstring", "manufacturer", (WPA_SUP_CFG_GBL), "Broadcom", NULL},
	{"wps_modelname", "model_name", (WPA_SUP_CFG_GBL), "Broadcom", NULL},
	{"wps_modelnum", "model_number", (WPA_SUP_CFG_GBL), "1234", NULL},
	{"boardnum", "serial_number", (WPA_SUP_CFG_GBL), "267", NULL},
	{"", "os_version", (WPA_SUP_CFG_GBL | HAPD_CFG_USEDEFAULT), "134217728", NULL},
	{"", "device_type", (WPA_SUP_CFG_GBL | HAPD_CFG_USEDEFAULT), "6-0050F204-1", NULL},
	{"wps_config_method", "config_methods", (WPA_SUP_CFG_GBL),
		"label display push_button keypad", hapd_wpsconfig_method_conv_fn},
	{"", "wps_cred_processing", (WPA_SUP_CFG_GBL | HAPD_CFG_PERBSS), "",
		wpa_supp_wps_cred_processing_conv_fn},
#ifdef MULTIAP
	{"map", "map_bh_sta", (WPA_SUP_CFG_GBL | HAPD_CFG_PERBSS), "", wpa_supp_map_conv_fv},
#endif	/* MULTIAP */
	{0, 0, 0, 0, 0}
};

/* wpa_supplicant network block config array */
static hapd_nvram_config_t wpa_supp_nw_cfg_arr[] =
{
	{"ssid", "ssid", (HAPD_CFG_COMMON | HAPD_CFG_PERBSS), "Broadcom", wpa_supp_ssid_conv_fn},
	{"akm", "key_mgmt", (HAPD_CFG_COMMON | HAPD_CFG_PERBSS), "NONE", wpa_supp_key_mgmt_conv_fn},
	{"proto", "proto", (HAPD_CFG_COMMON | HAPD_CFG_PERBSS), "RSN", wpa_supp_key_mgmt_conv_fn},
	{"crypto", "pairwise", (HAPD_CFG_COMMON | HAPD_CFG_PERBSS), "CCMP TKIP",
		wpa_supp_key_mgmt_conv_fn},
	{"wpa_psk", "psk", (HAPD_CFG_COMMON | HAPD_CFG_PERBSS), "1234567890",
		wpa_supp_key_mgmt_conv_fn},
#ifdef MULTIAP
	{"map_onboarded", "scan_ssid", (HAPD_CFG_COMMON | HAPD_CFG_PERBSS),
		"0", wpa_supp_scan_ssid_conv_fn},
#endif	/* MULTIAP */
	{0, 0, 0, 0, 0}
};

#ifdef MULTIAP
/* wpa_supplicant Multi-AP network block config array */
static hapd_nvram_config_t wpa_supp_map_nw_cfg_arr[] =
{
	{"ssid", "ssid", (HAPD_CFG_COMMON | HAPD_CFG_PERBSS), "Broadcom", wpa_supp_ssid_conv_fn},
	{"akm", "key_mgmt", (HAPD_CFG_COMMON | HAPD_CFG_PERBSS), "NONE", wpa_supp_key_mgmt_conv_fn},
	{"proto", "proto", (HAPD_CFG_COMMON | HAPD_CFG_PERBSS), "RSN", wpa_supp_key_mgmt_conv_fn},
	{"crypto", "pairwise", (HAPD_CFG_COMMON | HAPD_CFG_PERBSS), "CCMP TKIP",
		wpa_supp_key_mgmt_conv_fn},
	{"wpa_psk", "psk", (HAPD_CFG_COMMON | HAPD_CFG_PERBSS), "1234567890",
		wpa_supp_key_mgmt_conv_fn},
	{"map_onboarded", "scan_ssid", (HAPD_CFG_COMMON | HAPD_CFG_PERBSS),
		"0", wpa_supp_scan_ssid_conv_fn},
	{"bssid", "bssid", (HAPD_CFG_COMMON | HAPD_CFG_PERBSS), "0", NULL},
	{0, 0, 0, 0, 0}
};
#endif	/* MULTIAP */

/* Helper fn to get security info */
static void
hapd_wpasupp_get_security_details(char *nvi_ifname, int *out_akm)
{
	char nv_name[HAPD_MAX_BUF] = {0}, tmp[HAPD_MIN_BUF] = {0}, *akm_val, *next;
	int akm = 0;

	// Check for wep security
	snprintf(nv_name, sizeof(nv_name), "%s_wep", nvi_ifname);
	if (!strcmp(nvram_safe_get(nv_name), "enabled")) {
		akm = 0;
	} else {
		snprintf(nv_name, sizeof(nv_name), "%s_akm", nvi_ifname);
		akm_val = nvram_safe_get(nv_name);

		foreach(tmp, akm_val, next) {
			if (akm < HAPD_AKM_WPA && !strcmp(tmp, "psk")) { /* WPA-PSK */
				akm |= HAPD_AKM_PSK;
			}

			if (akm < HAPD_AKM_WPA && !strcmp(tmp, "psk2")) { /* WPA2-PSK */
				akm |= HAPD_AKM_PSK2;
			}

			/* WPA-EAP/1x */
			if (!strcmp(tmp, "wpa") && (!akm || akm == HAPD_AKM_WPA2)) {
				akm |= HAPD_AKM_WPA;
			}

			/* WPA2-EAP/1x */
			if (!strcmp(tmp, "wpa2") && (!akm || akm == HAPD_AKM_WPA)) {
				akm |= HAPD_AKM_WPA2;
			}
			if (!strcmp(tmp, "psk2ft")) {
				akm |= HAPD_AKM_PSK2_FT;
			}
			if (!strcmp(tmp, "sae")) {
				akm |= HAPD_AKM_WPA3_SAE;
			}
			if (!strcmp(tmp, "saeft")) {
				akm = HAPD_AKM_WPA3_SAE_FT;
			}
			if (!strcmp(tmp, "osen")) {
				akm = HAPD_AKM_WPA2_OSEN;
			}
		}
	}

	*out_akm = akm;
}

/* Fn to identify the bridge corresponding to wireless interface *
 */
static int
hapd_br_conv_fn(char *nvi_ifname, char *name, char *out_val, int out_sz)
{
	char tmp[HAPD_MAX_BUF] = {0};
	char *wl_name = nvram_safe_get(strcat_r(nvi_ifname, "_ifname", tmp));
	char *lan_ifnames = nvram_safe_get("lan_ifnames");
	char *lan1_ifnames = nvram_safe_get("lan1_ifnames");
	char *lan_ifname = NULL;

	if (lan_ifnames[0] != '\0' && find_in_list(lan_ifnames, wl_name)) {
		lan_ifname = nvram_safe_get("lan_ifname");
		snprintf(out_val, out_sz, "%s", (lan_ifname[0] != '\0') ? lan_ifname : "br0");
	} else if (lan1_ifnames[0] != '\0' && find_in_list(lan1_ifnames, wl_name)) {
		lan_ifname = nvram_safe_get("lan1_ifname");
		snprintf(out_val, out_sz, "%s", (lan_ifname[0] != '\0') ? lan_ifname : "br1");
	} else  {
		return -1;
	}

	return 0;
}

/* Callback fn to get the hw_mode from phytype nvram and band info. */
static int
hapd_phytype_conv_fn(char *nvi_ifname, char *name, char *out_val, int out_sz)
{
	char nv_name[HAPD_MAX_BUF] = {0}, os_ifname[IFNAMSIZ] = {0}, *phy_type;
	uint16 chanspec = 0;

	if (nvifname_to_osifname(nvi_ifname, os_ifname, sizeof(os_ifname))) {
		dprintf("Err: rc: %s in converting %s to os name \n", __FUNCTION__, name);
		return -1;
	}

	if (wl_iovar_get(os_ifname, "chanspec", &chanspec, sizeof(chanspec)) < 0) {
		dprintf("Err: rc: %s chanspec iovar cmd failed for %s\n", __FUNCTION__, os_ifname);
		return -1;
	}

	if (!wf_chspec_valid(chanspec)) {
		dprintf("Err: rc: %s invalid chanspec %x for %s\n", __FUNCTION__,
			chanspec, os_ifname);
		return -1;
	}

	snprintf(nv_name, sizeof(nv_name), "%s_%s", nvi_ifname, name);
	phy_type = nvram_safe_get(nv_name);

	if (CHSPEC_IS2G(chanspec)) {
		snprintf(out_val, out_sz, HAPD_HW_MODE_80211_G);
	} else if (CHSPEC_IS5G(chanspec) || strchr(phy_type, 'v') || strchr(phy_type, 'h')) {
		snprintf(out_val, out_sz, HAPD_HW_MODE_80211_A);
	}

	return 0;
}

/* Callback funtion to get the channel info */
static int
hapd_channel_conv_fn(char *nvi_ifname, char *name, char *out_val, int out_sz)
{
	uint16 chanspec = 0x0;
	char os_ifname[IFNAMSIZ] = {0};

	if (nvifname_to_osifname(nvi_ifname, os_ifname, sizeof(os_ifname))) {
		dprintf("Err: rc: %s in converting %s to os name \n", __FUNCTION__, name);
		return -1;
	}

	if (wl_iovar_get(os_ifname, name, &chanspec, sizeof(chanspec)) < 0) {
		dprintf("Err: rc: %s %s iovar cmd failed for %s\n", __FUNCTION__, name, os_ifname);
		return -1;
	}

	if (!wf_chspec_valid(chanspec)) {
		dprintf("Err: rc: %s invalid chanspec %x for %s\n", __FUNCTION__,
			chanspec, os_ifname);
		return -1;
	}

	snprintf(out_val, out_sz, "%d", wf_chspec_ctlchan(chanspec));

	return 0;
}

/* Callback fn to get the auth_algs info. */
static int
hapd_auth_conv_fn(char *nvi_ifname, char *name, char *out_val, int out_sz)
{
	int auth_val;
	char nv_name[HAPD_MAX_BUF] = {0};

	snprintf(nv_name, sizeof(nv_name), "%s_%s", nvi_ifname, name);
	auth_val = atoi(nvram_safe_get(nv_name));

	if (auth_val == 0) {
		snprintf(out_val, out_sz, "%d", HAPD_AUTH_OPEN_SYSTEM);
	} else if (auth_val == 1) {
		snprintf(out_val, out_sz, "%d", HAPD_AUTH_SHARED_KEY);
	}

	return 0;
}

/* Callback funtion to get the wep security info */
static int
hapd_wep_conv_fn(char *nvi_ifname, char *name, char *out_val, int out_sz)
{
	char nv_name[HAPD_MAX_BUF] = {0}, *wep = NULL;
	int ret = -1, def_key_index = 0;

	snprintf(nv_name, sizeof(nv_name), "%s_wep", nvi_ifname);
	wep = nvram_safe_get(nv_name);

	if (!strcmp(wep, "enabled")) {
		snprintf(nv_name, sizeof(nv_name), "%s_%s", nvi_ifname, name);
		def_key_index = atoi(nvram_safe_get(nv_name));
		if (def_key_index > 0) {
			snprintf(out_val, out_sz, "%d", def_key_index - 1);
		}
		ret = 0;
	} else {
			/* skip next 4 indexes key1 key2 key3 and key4 */
			ret = 4;
	}

	return ret;
}

/* Fn to check if the string (without 0x prefix) represents a
 * hexadecimal number. Returns TRUE for valid hex number.
 */
int hapd_hex_validate(char *s)
{
	int i;

	for  (i = 0; i < strlen(s); i++) {
		if (!isxdigit(s[i]))
			break;
	}

	return (i < strlen(s) ? FALSE : TRUE);
}

/* Callback fn to get WEP keys. The keys can be 5 or 13 ASCII characters in which case they should
 * be enclosed in quotes in conf file or they can be unquoted hexadecimal number of 10 or 26 digits.
 */
static int
hapd_wep_key_conv_fn(char *nvi_ifname, char *name, char *out_val, int out_sz)
{
	char nv_name[HAPD_MAX_BUF] = {0};
	char *wep_key = NULL;
	size_t key_length;

	snprintf(nv_name, sizeof(nv_name), "%s_%s", nvi_ifname, name); /* wl0_keyN , N = [1-4] */

	wep_key = nvram_safe_get(nv_name);

	if (wep_key[0] != '\0') {
		key_length = strlen(wep_key);

		switch (key_length)
		{
			case 5:
			case 13: /* ASCII characters -  Enclose in quotes */
				snprintf(out_val, out_sz, "\"%s\"", wep_key);
				break;
			case 10:
			case 26:
				if (hapd_hex_validate(wep_key)) {
					/* Unquoted hex digits */
					snprintf(out_val, out_sz, "%s", wep_key);
				}
				break;
			default:
				dprintf("Err: rc: %s, default case: key_length: %d\n",
						__FUNCTION__, key_length);
				break;
		}
	}

	return 0;
}

static int
hapd_fbt_mdid_conv_fn(char *nvi_ifname, char *name, char *out_val, int out_sz)
{
	char nv_name[HAPD_MAX_BUF] = {0};
	char *mdid;
	uint16 mdie, swp_mdie;
	int fbt_enabled = 0;

	fbt_enabled = hapd_fbt_enabled(nvi_ifname);

	if (!fbt_enabled) {
		//skip next 8 nvrams since mdid is accounted for
		return FBT_FIELDS_NUM - 1;
	}

	snprintf(nv_name, sizeof(nv_name), "%s_fbt_mdid", nvi_ifname);
	mdid = nvram_safe_get(nv_name);

	mdie = (uint16)atoi(mdid);

	/* swap the bytes in 2-octet mdie */
	swp_mdie = swap16(mdie);

	/* 2 octet MDID as hex string */
	snprintf(out_val, out_sz, "%04x", swp_mdie);

	return 0;
}

static int
hapd_r0kh_id_conv_fn(char *nvi_ifname, char *name, char *out_val, int out_sz)
{
	char nv_name[HAPD_MAX_BUF] = {0};
	char *nas_identifier;

	snprintf(nv_name, sizeof(nv_name), "%s_r0kh_id", nvi_ifname);
	nas_identifier = nvram_safe_get(nv_name);
	snprintf(out_val, out_sz, "%s", nas_identifier);

	return 0;
}

static int
hapd_r1kh_id_conv_fn(char *nvi_ifname, char *name, char *out_val, int out_sz)
{
	char nv_name[HAPD_MAX_BUF] = {0};
	char *r1_key_holder;
	unsigned char ea[ETHER_ADDR_LEN];
	int err = 0;

	snprintf(nv_name, sizeof(nv_name), "%s_r1kh_id", nvi_ifname);
	r1_key_holder = nvram_safe_get(nv_name);

	if (r1_key_holder != NULL) {
		err = ether_atoe(r1_key_holder, ea);
		if (err < 0)
			return err;
	}
	snprintf(out_val, out_sz, "%02x%02x%02x%02x%02x%02x",
		ea[0], ea[1], ea[2], ea[3], ea[4], ea[5]);

	return 0;
}

static int
hapd_fbt_reassoc_time_conv_fn(char *nvi_ifname, char *name, char *out_val, int out_sz)
{
	char nv_name[HAPD_MAX_BUF] = {0};
	char *fbt_reassoc_time;

	snprintf(nv_name, sizeof(nv_name), "%s_fbt_reassoc_time", nvi_ifname);
	fbt_reassoc_time = nvram_safe_get(nv_name);
	snprintf(out_val, out_sz, "%s", fbt_reassoc_time);

	return 0;
}

static int
hapd_fbtap_r0kh_conv_fn(char *nvi_ifname, char *name, char *out_val, int out_sz)
{
	char nv_name[HAPD_MAX_BUF] = {0};
	char fbt_name[HAPD_MIN_BUF], tar_name[HAPD_MIN_BUF];
	char *fbt_aps, *next;
	char r0kh[HAPD_MAX_BUF * 4];
	int repeat = 0;
	char *fbt_r0_key, fbt_r0_hex_key[33];
	char *addr, *r0kh_id;

	/*
	 * Preparing the r0kh string
	 */
	snprintf(nv_name, sizeof(nv_name), "%s_fbt_aps", nvi_ifname);
	fbt_aps = nvram_safe_get(nv_name);
	memset(fbt_name, 0, sizeof(fbt_name));
	memset(r0kh, 0, sizeof(r0kh));
	foreach(fbt_name, fbt_aps, next)
	{
		memset(tar_name, 0, sizeof(tar_name));
		memcpy(tar_name, fbt_name, sizeof(tar_name));
		memset(nv_name, 0, sizeof(nv_name));
		memset(fbt_r0_hex_key, 0, sizeof(fbt_r0_hex_key));
		/*
		 * r0kh
		 */
		snprintf(nv_name, sizeof(nv_name), "%s_addr", tar_name);
		if (nvram_safe_get(nv_name) == NULL) {
			continue;
		}

		snprintf(nv_name, sizeof(nv_name), "%s_addr", tar_name);
		addr = nvram_safe_get(nv_name);
		snprintf(nv_name, sizeof(nv_name), "%s_r0kh_id", tar_name);
		r0kh_id = nvram_safe_get(nv_name);
		snprintf(nv_name, sizeof(nv_name), "%s_r0kh_key", tar_name);
		fbt_r0_key = nvram_safe_get(nv_name);
		if (addr == NULL || r0kh_id == NULL || fbt_r0_key == NULL) {
			continue;
		}

		if (repeat != 0) {
			strcat(r0kh, "\nr0kh=");
		}

		strcat(r0kh, addr);
		strcat(r0kh, " ");
		strcat(r0kh, r0kh_id);
		strcat(r0kh, " ");
		snprintf(nv_name, sizeof(nv_name), "%s_r0kh_key", tar_name);
		if ((fbt_r0_key != NULL) && (strlen(fbt_r0_key) == 16)) {
			bytes_to_hex(fbt_r0_key, strlen(fbt_r0_key), fbt_r0_hex_key,
					sizeof(fbt_r0_hex_key));
			strcat(r0kh, fbt_r0_hex_key);
		}
		repeat++;
	}
	snprintf(out_val, out_sz, "%s", r0kh);
	return 0;
}

static int
hapd_fbtap_r1kh_conv_fn(char *nvi_ifname, char *name, char *out_val, int out_sz)
{
	char nv_name[HAPD_MAX_BUF] = {0};
	char fbt_name[HAPD_MIN_BUF], tar_name[HAPD_MIN_BUF];
	char *fbt_aps, *next;
	char r1kh[HAPD_MAX_BUF * 4];
	int repeat = 0;
	char *fbt_r1_key, fbt_r1_hex_key[33];
	char *addr, *r1kh_id;

	/*
	 * Preparing the r1kh string
	 */
	snprintf(nv_name, sizeof(nv_name), "%s_fbt_aps", nvi_ifname);
	fbt_aps = nvram_safe_get(nv_name);
	memset(fbt_name, 0, sizeof(fbt_name));
	memset(r1kh, 0, sizeof(r1kh));
	foreach(fbt_name, fbt_aps, next)
	{
		memset(tar_name, 0, sizeof(tar_name));
		memcpy(tar_name, fbt_name, sizeof(tar_name));
		memset(nv_name, 0, sizeof(nv_name));
		memset(fbt_r1_hex_key, 0, sizeof(fbt_r1_hex_key));
		/*
		 * r1kh
		 */
		snprintf(nv_name, sizeof(nv_name), "%s_addr", tar_name);
		addr = nvram_safe_get(nv_name);
		snprintf(nv_name, sizeof(nv_name), "%s_r1kh_id", tar_name);
		r1kh_id = nvram_safe_get(nv_name);
		snprintf(nv_name, sizeof(nv_name), "%s_r1kh_key", tar_name);
		fbt_r1_key = nvram_safe_get(nv_name);
		if (addr == NULL || r1kh_id == NULL || fbt_r1_key == NULL)
			continue;

		if (repeat) {
			strcat(r1kh, "\nr1kh=");
		}

		strcat(r1kh, addr);
		strcat(r1kh, " ");
		strcat(r1kh, r1kh_id);
		strcat(r1kh, " ");

		if ((fbt_r1_key != NULL) && (strlen(fbt_r1_key) == 16)) {
			bytes_to_hex(fbt_r1_key, strlen(fbt_r1_key), fbt_r1_hex_key,
					sizeof(fbt_r1_hex_key));
			strcat(r1kh, fbt_r1_hex_key);
		}
		repeat++;
	}
	snprintf(out_val, out_sz, "%s", r1kh);

	return 0;
}

static int
hapd_fbtap_psk_generate_local(char *nvi_ifname, char *name, char *out_val, int out_sz)
{
	char nv_name[HAPD_MAX_BUF] = {0};
	char *fbt_generate_local;

	snprintf(nv_name, sizeof(nv_name), "%s_fbt_generate_local", nvi_ifname);
	fbt_generate_local = nvram_safe_get(nv_name);
	snprintf(out_val, out_sz, "%s", fbt_generate_local);

	return 0;
}

static int
hapd_fbtoverds_conv_fn(char *nvi_ifname, char *name, char *out_val, int out_sz)
{
	char nv_name[HAPD_MAX_BUF] = {0};
	char *fbtoverds;

	snprintf(nv_name, sizeof(nv_name), "%s_fbtoverds", nvi_ifname);
	fbtoverds = nvram_safe_get(nv_name);
	snprintf(out_val, out_sz, "%s", fbtoverds);

	return 0;
}

static int hapd_fbt_enabled(char *nvi_ifname)
{
	char nv_name[HAPD_MAX_BUF] = {0};
	int fbt = 0, fbt_ap = 0, wbd_fbt = 0;
	int fbt_enabled = 0;
	int map_mode = 0;
	char *ptr = NULL;

	map_mode = (int)strtol(nvram_safe_get("multiap_mode"), NULL, 0);

	if (map_mode > 0) {
		/* WBD enabled. FBT by WBD */
		snprintf(nv_name, sizeof(nv_name), "%s_wbd_fbt", nvi_ifname);
		ptr  = nvram_safe_get(nv_name);
		if (ptr != NULL) {
			wbd_fbt = atoi(ptr);
		}
	}

	snprintf(nv_name, sizeof(nv_name), "%s_fbt", nvi_ifname);
	ptr  = nvram_safe_get(nv_name);
	if (ptr != NULL) {
		fbt = atoi(ptr);
	}

	snprintf(nv_name, sizeof(nv_name), "%s_fbt_ap", nvi_ifname);
	ptr = nvram_safe_get(nv_name);
	if (ptr != NULL) {
		fbt_ap = atoi(ptr);
	}

	if ((wbd_fbt || fbt) && fbt_ap) {
		fbt_enabled = 1;
	}

	return fbt_enabled;
}

/* Helper fn to get key mgmt info */
static void
hapd_get_key_mgmt(char *nvi_ifname, int fbt_enabled, int akm, char *key_mgmt)
{
	char nv_name[HAPD_MAX_BUF] = {0};

	snprintf(nv_name, sizeof(nv_name), "%s_mfp", nvi_ifname);
	bool mfp_required = nvram_match(nv_name, "2");

	if (mfp_required) { /* MFP is REQUIRED */
		if (akm == HAPD_AKM_WPA || akm == HAPD_AKM_WPA2 ||
				(akm == (HAPD_AKM_WPA | HAPD_AKM_WPA2))) {
			/* wpa, wpa2 and mixed mode enterprise security */
			snprintf(key_mgmt, HAPD_MAX_BUF, "%s",
				(fbt_enabled ? HAPD_KEY_MGMT_EAP_FT : HAPD_KEY_MGMT_EAP_SHA256));
		} else if (akm == HAPD_AKM_PSK || akm == HAPD_AKM_PSK2 ||
				akm == (HAPD_AKM_PSK | HAPD_AKM_PSK2)) {
			/* wpa, wpa2 and mixed mode psk security */
			snprintf(key_mgmt, HAPD_MAX_BUF, "%s", HAPD_KEY_MGMT_WPA_SHA256);
		}
	} else {
		if (akm == HAPD_AKM_WPA || akm == HAPD_AKM_WPA2 ||
				(akm == (HAPD_AKM_WPA | HAPD_AKM_WPA2))) {
			/* wpa, wpa2 and mixed mode enterprise security */
			snprintf(key_mgmt, HAPD_MAX_BUF, "%s",
				(fbt_enabled ? HAPD_KEY_MGMT_EAP_FT : HAPD_KEY_MGMT_EAP));
		} else if (akm == HAPD_AKM_PSK || akm == HAPD_AKM_PSK2 ||
				akm == (HAPD_AKM_PSK | HAPD_AKM_PSK2)) {
			/* wpa, wpa2 and mixed mode psk security */
			snprintf(key_mgmt, HAPD_MAX_BUF, "%s", HAPD_KEY_MGMT_WPA);
		} else if (akm == HAPD_AKM_WPA2_OSEN) {
			snprintf(key_mgmt, HAPD_MAX_BUF, "%s", HAPD_KEY_MGMT_OSEN);
		}
	}
	if (akm & HAPD_AKM_WPA3_SAE) {
		/* WPA3SAE */
		snprintf(key_mgmt, HAPD_MAX_BUF, "%s",
			((akm & HAPD_AKM_PSK2) ?
			(mfp_required ? HAPD_KEY_MGMT_WPA_SHA256_SAE : HAPD_KEY_MGMT_WPA_PSK_SAE) :
			HAPD_KEY_MGMT_SAE));
	}
	if (akm == HAPD_AKM_WPA3_SAE_FT) {
		/* WPA3FTSAE */
		snprintf(key_mgmt, HAPD_MAX_BUF, "%s", HAPD_KEY_MGMT_FT_SAE);
	}
}

/* Callback fn to fill key mgmt details */
static int
hapd_key_mgmt_conv_fn(char *nvi_ifname, char *name, char *out_val, int out_sz)
{
	char nv_name[HAPD_MAX_BUF] = {0}, key_mgmt[HAPD_MAX_BUF] = {0};
	int sae_require_mfp = 0;
	int akm = 0, ret = 0;
	bool psk_required;
	int fbt_enabled = 0;

	hapd_wpasupp_get_security_details(nvi_ifname, &akm);

	psk_required = ((akm & HAPD_AKM_PSK2) || (akm & HAPD_AKM_PSK) ||
			(akm & HAPD_AKM_WPA3_SAE)) ? TRUE : FALSE;

	fbt_enabled = hapd_fbt_enabled(nvi_ifname);

	hapd_get_key_mgmt(nvi_ifname, fbt_enabled, akm, key_mgmt);

	switch (akm) {
		/* open | wep security in both the cases wpa should be 0 */
		case HAPD_AKM_OPEN_WEP:
			if (!strcmp(name, "wpa")) {
				snprintf(out_val, out_sz, "%d", HAPD_SEC_OPEN);
			}
			// skip akm, crypto, wpa_psk, mfp plus SAE, FBT and RADIUS fields
			ret = WPAX_FIELDS_NUM + SAE_FIELDS_NUM + FBT_FIELDS_NUM +
				RADIUS_FIELDS_NUM;
			break;

		/* HAPD_SEC_WPA */
		case HAPD_AKM_PSK:
			if (!strcmp(name, "wpa")) {
				snprintf(out_val, out_sz, "%d", HAPD_SEC_WPA);
			} else if (!strcmp(name, "crypto")) {
				// Set wpa_pairwise based on crypto
				snprintf(nv_name, sizeof(nv_name), "%s_%s", nvi_ifname, name);
				if (!strcmp(nvram_safe_get(nv_name), "tkip")) {
					snprintf(out_val, out_sz, HAPD_CIPHER_SUITE_TKIP);
				} else if (nvram_match(nv_name, "tkip+aes")) {
					snprintf(out_val, out_sz,
						HAPD_CIPHER_SUITE_TKIP" "HAPD_CIPHER_SUITE_CCMP);
				}
			} else if (!strcmp(name, "akm")) {
				snprintf(out_val, out_sz, key_mgmt);
			} else if (psk_required && !strcmp(name, "wpa_psk")) {
				snprintf(nv_name, sizeof(nv_name), "%s_%s", nvi_ifname, name);
				snprintf(out_val, out_sz, nvram_safe_get(nv_name));
				// skip SAE fields
				ret = SAE_FIELDS_NUM;
			}
			break;

		/* HAPD_SEC_WPA | WPA2 | FT */
		case HAPD_AKM_PSK2:
		case (HAPD_AKM_PSK2 | HAPD_AKM_PSK2_FT):
			if (!strcmp(name, "wpa")) {
				snprintf(nv_name, sizeof(nv_name), "%s_crypto", nvi_ifname);
				// Set wpa based on crypto
				if (nvram_match(nv_name, "tkip+aes")) {
					snprintf(out_val, out_sz, "%d", (HAPD_SEC_WPA | HAPD_SEC_WPA2));
				} else if (nvram_match(nv_name, "aes")) {
					snprintf(out_val, out_sz, "%d", HAPD_SEC_WPA2);
				}
			} else if (!strcmp(name, "crypto")) {
				// Set wpa_pairwise based on crypto
				snprintf(nv_name, sizeof(nv_name), "%s_%s", nvi_ifname, name);
				if (!strcmp(nvram_safe_get(nv_name), "aes")) {
					snprintf(out_val, out_sz, HAPD_CIPHER_SUITE_CCMP);
				} else if (nvram_match(nv_name, "tkip+aes")) {
					snprintf(out_val, out_sz,
						HAPD_CIPHER_SUITE_TKIP" "HAPD_CIPHER_SUITE_CCMP);
				}
			} else if (!strcmp(name, "akm")) {
				if (fbt_enabled) {
					strncpy_n(key_mgmt, HAPD_KEY_MGMT_WPA_PSK_FT,
						sizeof(HAPD_KEY_MGMT_WPA_PSK_FT));
					snprintf(out_val, out_sz, key_mgmt);
				} else
					snprintf(out_val, out_sz, key_mgmt);
			} else if (psk_required && !strcmp(name, "wpa_psk")) {
				snprintf(nv_name, sizeof(nv_name), "%s_%s", nvi_ifname, name);
				snprintf(out_val, out_sz, nvram_safe_get(nv_name));
				// skip SAE fields
				ret = SAE_FIELDS_NUM;
			}
			break;

		/* HAPD_SEC_WPA-HAPD_SEC_WPA2 */
		case (HAPD_AKM_PSK | HAPD_AKM_PSK2):
		case (HAPD_AKM_WPA | HAPD_AKM_WPA2):
			if (!strcmp(name, "wpa")) {
				snprintf(out_val, out_sz, "%d", (HAPD_SEC_WPA | HAPD_SEC_WPA2));
			} else if (!strcmp(name, "crypto")) {
				snprintf(nv_name, sizeof(nv_name), "%s_%s", nvi_ifname, name);
				if (nvram_match(nv_name, "aes")) {
					snprintf(out_val, out_sz, HAPD_CIPHER_SUITE_CCMP);
				} else if (nvram_match(nv_name, "tkip+aes")) {
					snprintf(out_val, out_sz,
						HAPD_CIPHER_SUITE_TKIP" "HAPD_CIPHER_SUITE_CCMP);
				}
			} else if (!strcmp(name, "akm")) {
				snprintf(out_val, out_sz, key_mgmt);
			} else if (psk_required && !strcmp(name, "wpa_psk")) {
				snprintf(nv_name, sizeof(nv_name), "%s_%s", nvi_ifname, name);
				snprintf(out_val, out_sz, nvram_safe_get(nv_name));
				// skip SAE fields
				ret = SAE_FIELDS_NUM;
			}
			break;

		case HAPD_AKM_WPA:
			if (!strcmp(name, "wpa")) {
				snprintf(out_val, out_sz, "%d", HAPD_SEC_WPA2);
			} else if (!strcmp(name, "crypto")) {
				snprintf(nv_name, sizeof(nv_name), "%s_%s", nvi_ifname, name);
				if (!strcmp(nvram_safe_get(nv_name), "aes")) {
					snprintf(out_val, out_sz, HAPD_CIPHER_SUITE_CCMP);
				}
			} else if (!strcmp(name, "akm")) {
				snprintf(out_val, out_sz, key_mgmt);
			} else if (psk_required && !strcmp(name, "wpa_psk")) {
				snprintf(nv_name, sizeof(nv_name), "%s_%s", nvi_ifname, name);
				snprintf(out_val, out_sz, nvram_safe_get(nv_name));
				// skip SAE fields
				ret = SAE_FIELDS_NUM;
			}
			break;

		case HAPD_AKM_WPA2:
			if (!strcmp(name, "wpa")) {
				snprintf(out_val, out_sz, "%d", HAPD_SEC_WPA2);
			} else if (!strcmp(name, "crypto")) {
				snprintf(nv_name, sizeof(nv_name), "%s_%s", nvi_ifname, name);
				if (!strcmp(nvram_safe_get(nv_name), "aes")) {
					snprintf(out_val, out_sz, HAPD_CIPHER_SUITE_CCMP);
				}
			} else if (!strcmp(name, "akm")) {
				snprintf(out_val, out_sz, key_mgmt);
			} else if (psk_required && !strcmp(name, "wpa_psk")) {
				snprintf(nv_name, sizeof(nv_name), "%s_%s", nvi_ifname, name);
				snprintf(out_val, out_sz, nvram_safe_get(nv_name));
				// skip SAE fields
				ret = SAE_FIELDS_NUM;
			}
			break;
		case HAPD_AKM_WPA3_SAE:
		case (HAPD_AKM_WPA3_SAE | HAPD_AKM_PSK2):
			if (!strcmp(name, "wpa")) {
				snprintf(out_val, out_sz, "%d", HAPD_SEC_WPA2);
			} else if (!strcmp(name, "crypto")) {
				snprintf(nv_name, sizeof(nv_name), "%s_%s", nvi_ifname, name);
				if (!strcmp(nvram_safe_get(nv_name), "aes")) {
					snprintf(out_val, out_sz, HAPD_CIPHER_SUITE_CCMP);
				}
			} else if (!strcmp(name, "akm")) {
				snprintf(out_val, out_sz, key_mgmt);
			} else if (psk_required && !strcmp(name, "wpa_psk")) {
				snprintf(nv_name, sizeof(nv_name), "%s_%s", nvi_ifname, name);
				snprintf(out_val, out_sz, nvram_safe_get(nv_name));
			} else if (!strcmp(name, "mfp")) {
				snprintf(nv_name, sizeof(nv_name), "%s_%s", nvi_ifname, name);
				if (nvram_match(nv_name, "0")) {
					sae_require_mfp = 0;
				} else { /* MFP is capable/required */
					sae_require_mfp = 1;
				}
				snprintf(out_val, out_sz, "%d", sae_require_mfp);
			}
			break;
		case HAPD_AKM_WPA2_OSEN:
			/* MFP  FBT not valid for OSEN */
			if (!strcmp(name, "wpa")) {
				snprintf(out_val, out_sz, "%d", HAPD_SEC_OPEN);
			} else if (!strcmp(name, "crypto")) {
				snprintf(nv_name, sizeof(nv_name), "%s_%s", nvi_ifname, name);
				if (!strcmp(nvram_safe_get(nv_name), "aes")) {
					snprintf(out_val, out_sz, HAPD_CIPHER_SUITE_CCMP);
				}
			} else  if (!strcmp(name, "akm")) {
				snprintf(out_val, out_sz, key_mgmt);
			}
			break;
		default:
			return -1;
	};

	if (out_val[0] == '\0') {
		return -1;
	}

	return ret;
}

/* Callback fn to get fill pre-authentication capability */
static int
hapd_preauth_conv_fn(char *nvi_ifname, char *name, char *out_val, int out_sz)
{
	char nv_name[HAPD_MAX_BUF] = {0};
	char *ptr;

	snprintf(nv_name, sizeof(nv_name), "%s_%s", nvi_ifname, name);
	ptr = nvram_get(nv_name);

	/* Set preauth to disable, if it is explicitly disabled */
	if (ptr != NULL && (atoi(ptr) == 0)) {
		snprintf(out_val, out_sz, "%d", HAPD_DISABLE);
		return PREAUTH_FIELDS_NUM;
	} else {
		snprintf(out_val, out_sz, "%d", HAPD_ENABLE);
	}
	return 0;
}

static int
hapd_preauth_interfaces_conv_fn(char *nvi_ifname, char *name, char *out_val, int out_sz)
{
	char nv_name[HAPD_MAX_BUF] = {0};
	char *ptr;

	snprintf(nv_name, sizeof(nv_name), "%s_%s", nvi_ifname, name);
	ptr = nvram_get(nv_name);

	/* Set preauth_interfaces to bridge interface if enabled */
	if (ptr != NULL && (atoi(ptr) == 1)) {
		snprintf(out_val, out_sz, "br0 br1");
	} else {
		snprintf(out_val, out_sz, "");
	}

	return 0;
}

/* Callback fn to get fill RADIUS details */
static int
hapd_radius_akm_conv_fn(char *nvi_ifname, char *name, char *out_val, int out_sz)
{
	char nv_name[HAPD_MAX_BUF] = {0};

	snprintf(nv_name, sizeof(nv_name), "%s_%s", nvi_ifname, name);

	/* string match successfull only with trailing space */
	if (nvram_match(nv_name, "wpa2 ") || nvram_match(nv_name, "wpa2") ||
		nvram_match(nv_name, "wpa2 wpa") || nvram_match(nv_name, "wpa wpa2") ||
		nvram_match(nv_name, "wpa2 wpa ") || nvram_match(nv_name, "wpa wpa2 ") ||
		nvram_match(nv_name, "osen")) {
		snprintf(out_val, out_sz, HAPD_IEEE8021X_SUPPORTED);
	} else {
		/* skip RADIUS settings */
		return RADIUS_FIELDS_NUM - 1;
	}

	return 0;
}

/* Callback funtion to get the wps state setting
 * lan_wps_oob/lan1_wps_oob = disabled => Configured
 * lan_wps_oob/lan1_wps_oob = enabled => Unconfigured
 * default nvram value is enabled
 */
static int
hapd_wpsstate_conv_fn(char *nvi_ifname, char *name, char *out_val, int out_sz)
{
	char nv_name[HAPD_MAX_BUF] = {0}, tmp[HAPD_MAX_BUF] = {0};
	uint8 wps_state = 0;
	int akm = 0;

#if defined(MULTIAP)
	{
		uint16 map = 0, map_mode = 0;
		char *ptr = NULL;

		ptr = nvram_safe_get("multiap_mode");
		if (ptr[0] != '\0') {
			map_mode = (uint16)strtoul(ptr, NULL, 0);
		}

		ptr = nvram_safe_get(strcat_r(nvi_ifname, "_map", tmp));
		if (ptr[0] != '\0') {
			map = (uint16)strtoul(ptr, NULL, 0);
		}
		/* wps should be disabled for multiap backhaul bss */
		if (map_mode != 0 && map == 2) {
			/* Skip remaining wps and multiap settings. */
			return (WPS_FIELDS_NUM + MAP_FIELDS_NUM - 1);
		}
	}
#endif	/* MULTIAP */
	hapd_wpasupp_get_security_details(nvi_ifname, &akm);

	/* Enterprise enabled skip WPS. Update condition when WPA3 AKM is added */
	if (akm == HAPD_AKM_WPA || akm == HAPD_AKM_WPA2 ||
		akm == (HAPD_AKM_WPA | HAPD_AKM_WPA2) ||
		akm ==  HAPD_AKM_WPA2_OSEN) {
		// Skip remaining wps settings.
		return (WPS_FIELDS_NUM - 1);
	}

	snprintf(nv_name, sizeof(nv_name), "%s_%s", nvi_ifname, name);

	if (nvram_match(nv_name, "enabled")) {
		char *mode = nvram_safe_get(strcat_r(nvi_ifname, "_mode", tmp));

		if (!strcmp(mode, "ap")) {
			char *lan_ifnames = nvram_safe_get("lan_ifnames");
			char *lan1_ifnames = nvram_safe_get("lan1_ifnames");
			char *wl_name = nvram_safe_get(strcat_r(nvi_ifname, "_ifname", tmp));

			if (find_in_list(lan_ifnames, wl_name)) {
				if (nvram_match("lan_wps_oob", "disabled")) {
					wps_state = HAPD_WPS_CONFIGURED;
				} else {
					wps_state = HAPD_WPS_UNCONFIGURED;
				}

			} else if (find_in_list(lan1_ifnames, wl_name)) {
				if (nvram_match("lan1_wps_oob", "disabled")) {
					wps_state = HAPD_WPS_CONFIGURED;
				} else {
					wps_state = HAPD_WPS_UNCONFIGURED;
				}

			}
		} else {
			snprintf(tmp, sizeof(tmp), "%s_wps_oob", nvi_ifname);
			if (nvram_match(tmp, "disabled")) {
				wps_state = HAPD_WPS_CONFIGURED;
			} else {
				wps_state = HAPD_WPS_UNCONFIGURED;
			}
		}
	} else {
		// Skip remaining wps settings.
		return (WPS_FIELDS_NUM - 1);
	}

	snprintf(out_val, out_sz, "%d", wps_state);

	return 0;
}

/* Callback fn to fill wps ap lockdown field */
static int
hapd_wps_aplockdown_conv_fn(char *nvi_ifname, char *name, char *out_val, int out_sz)
{
	// If nvram wps_aplockdown is set to 1 or wps_wer_mode is deny than enable aplockdown
	if (nvram_match(name, "1") || nvram_match("wps_wer_mode", "deny")) {
		snprintf(out_val, out_sz, "%d", HAPD_ENABLE);
	}

	return 0;
}

/* Callback fn to fill wps uuid field */
static int
hapd_wpsuuid_conv_fn(char *nvi_ifname, char *name, char *out_val, int out_sz)
{
	char dev_type[] = "urn:schemas-wifialliance-org:device:WFADevice";
	char *mac_str;
	unsigned char mac[6] = {0}, uuid[16] = {0};
	MD5_CTX md_context;

	mac_str = nvram_safe_get("lan_hwaddr");
	if (!ether_atoe(mac_str, mac)) {
		cprintf("Info: rc: %s ether_atoe failed for mac (%s) \n", __FUNCTION__, mac_str);
		return -1;
	}

	/* Generate hash */
	MD5Init(&md_context);
	MD5Update(&md_context, mac, sizeof(mac));
	MD5Update(&md_context, dev_type, strlen(dev_type));
	MD5Final(uuid, &md_context);

	snprintf(out_val, out_sz, "%02x%02x%02x%02x-%02x%02x-%02x%02x-"
		"%02x%02x-%02x%02x%02x%02x%02x%02x",
		uuid[0], uuid[1], uuid[2], uuid[3], uuid[4], uuid[5], uuid[6], uuid[7],
		uuid[8], uuid[9], uuid[10], uuid[11], uuid[12], uuid[13], uuid[14], uuid[15]);

	return 0;
}

/* Function to generate the wps device pin */
static int
hapd_wps_generate_device_pin(char *device_pin, int device_pin_len)
{
	unsigned long PIN;
	unsigned long int accum = 0;
	unsigned char rand_bytes[HAPD_WPS_DEVICE_PIN_LEN];
	int digit;
	char temp_pin[HAPD_WPS_DEVICE_PIN_LEN + 1];

	/* Generate random bytes and compute the checksum */
	RAND_bytes(rand_bytes, sizeof(rand_bytes));

	snprintf(temp_pin, sizeof(temp_pin), "%08u", *(uint32 *)rand_bytes);
	temp_pin[HAPD_WPS_DEVICE_PIN_LEN - 1] = '\0';
	PIN = strtoul(temp_pin, NULL, 10);

	PIN *= 10;
	accum += 3 * ((PIN / 10000000) % 10);
	accum += 1 * ((PIN / 1000000) % 10);
	accum += 3 * ((PIN / 100000) % 10);
	accum += 1 * ((PIN / 10000) % 10);
	accum += 3 * ((PIN / 1000) % 10);
	accum += 1 * ((PIN / 100) % 10);
	accum += 3 * ((PIN / 10) % 10);

	digit = (accum % 10);
	accum = (10 - digit) % 10;

	PIN += accum;
	snprintf(temp_pin, sizeof(temp_pin), "%08u", (unsigned int)PIN);
	temp_pin[HAPD_WPS_DEVICE_PIN_LEN] = '\0';

	/* Output result */
	WLIF_STRNCPY(device_pin, temp_pin, device_pin_len);

	return 0;
}

/* Callback fn to fill wps pin method field */
static int
hapd_wps_device_pin_conv_fn(char *nvi_ifname, char *name, char *out_val, int out_sz)
{
	char *ptr;
	char wps_device_pin[HAPD_WPS_DEVICE_PIN_LEN + /* \0 */ 1] = {0};

	ptr = nvram_safe_get(name);

	// If wps_device_pin nvram is not present or it contains default value
	// Than generate the pin and update the same in nvram.
	if (ptr[0] == '\0' || !strcmp(ptr, HAPD_WPS_DEVICE_PIN_DEFAULT_VAL)) {
		hapd_wps_generate_device_pin(wps_device_pin, sizeof(wps_device_pin));
		snprintf(out_val, out_sz, "%s", wps_device_pin);
		nvram_set(name, wps_device_pin);
		nvram_commit();
		return 0;
	}

	snprintf(out_val, out_sz, "%s", ptr);

	return 0;
}

// Helper function to get the band type for wireless interface
static int
hapd_wps_get_band_type(char *ifname)
{
	int band_type = 0;

	wl_ioctl(ifname, WLC_GET_BAND, &band_type, sizeof(band_type));

	// If band type is auto than update band type from band list
	if (band_type == WLC_BAND_AUTO) {
		int list[3];	// list[0] is the count, values at index 1, 2 contains band type.
		int j;

		wl_ioctl(ifname, WLC_GET_BANDLIST, list, sizeof(list));
		if (list[0] > 2) {
			list[0] = 2;
		}
		band_type = 0;
		for (j = 0; j < list[0]; j++) {
			if (list[j] == WLC_BAND_5G || list[j] == WLC_BAND_2G) {
				band_type |= list[j];
			}
		}
	}

	dprintf("Info rc: %s ifname %s band type %d\n", __func__, ifname, band_type);

	return band_type;
}

/* Callback fn to fill the wps rf bands field */
static int
hapd_wps_rf_bands_conv_fn(char *nvi_ifname, char *name, char *out_val, int out_sz)
{
	char *lan_ifnames = nvram_safe_get("lan_ifnames");
	char *lan1_ifnames = nvram_safe_get("lan1_ifnames");
	char *ifnames, ifname[IFNAMSIZ] = {0}, *next;
	int len;
	int band_type = 0, device_band_type = 0;

	if ((lan_ifnames[0] == '\0') && (lan1_ifnames[0] == '\0')) {
		cprintf("Info: rc %s %d both lan and lan1 ifnames are empty\n",
			__func__, __LINE__);
		return -1;
	}

	len = strlen(lan_ifnames) + strlen(lan1_ifnames) + 2 /* space + '\0' */;

	ifnames = (char *)calloc(1, len);
	if (!ifnames) {
		cprintf("Info: rc %s %d calloc failed for %d bytes\n", __func__, __LINE__, len);
		return -1;
	}

	// Ifnames contains combined list of lan_ifnames and lan1 ifnames separated by space
	snprintf(ifnames, len, "%s %s", lan_ifnames, lan1_ifnames);

	// Traverse the list of interfaces in lan and lan1 ifnames
	foreach(ifname, ifnames, next) {
		// Skip non wireless interfaces
		if (wl_probe(ifname)) {
			dprintf("Info rc: %s skipping non wireless interface %s\n",
				__func__, ifname);
			continue;
		}

		// Get the band type for interface
		band_type = hapd_wps_get_band_type(ifname);

		// Aggregate the valid band type to get the device band type
		if (band_type != 0) {
			device_band_type |= band_type;
		}
	}

	switch (device_band_type) {
	case WLC_BAND_ALL:	// Dual band case
		snprintf(out_val, out_sz, "%s%s", HAPD_HW_MODE_80211_A, HAPD_HW_MODE_80211_G);
		break;
	case WLC_BAND_5G:	// 5G only
		snprintf(out_val, out_sz, "%s", HAPD_HW_MODE_80211_A);
		break;
	case WLC_BAND_2G:	// 2.4 G only
		snprintf(out_val, out_sz, "%s", HAPD_HW_MODE_80211_G);
		break;
	default:
		cprintf("Info rc: %s unknown device band type %d\n", __func__, device_band_type);
		break;

	}

	free(ifnames);

	return 0;
}

/* Callback fn to fill wps config method field */
static int
hapd_wpsconfig_method_conv_fn(char *nvi_ifname, char *name, char *out_val, int out_sz)
{
	uint16 config_methods = 0;
	char *ptr;

	ptr = nvram_safe_get(name);

	if (ptr[0] == '\0') {
		if (nvram_match("wps_version2", "enabled")) {
			snprintf(out_val, out_sz, "display virtual_push_button "
				"physical_push_button push_button virtual_display");
		} else {
			snprintf(out_val, out_sz, "display push_button");
		}
	} else {
		config_methods = (uint16)strtoul(ptr, NULL, 16);

		if (config_methods & HAPD_WPS_CONFIG_LABEL) {
			strncat(out_val, "label ", out_sz);
			out_sz -= strlen(out_val);
		}
		if (config_methods & HAPD_WPS_CONFIG_DISPLAY) {
			strncat(out_val, "display ", out_sz);
			out_sz -= strlen(out_val);
		}
		if (config_methods & HAPD_WPS_CONFIG_PBC) {
			strncat(out_val, "push_button ", out_sz);
			out_sz -= strlen(out_val);
		}
		if (config_methods & HAPD_WPS_CONFIG_KEYPAD) {
			strncat(out_val, "keypad ", out_sz);
			out_sz -= strlen(out_val);
		}
		if (config_methods & HAPD_WPS_CONFIG_VPBC) {
			strncat(out_val, "virtual_push_button ", out_sz);
			out_sz -= strlen(out_val);
		}
		if (config_methods & HAPD_WPS_CONFIG_PHYPBC) {
			strncat(out_val, "physical_push_button ", out_sz);
			out_sz -= strlen(out_val);
		}
		if (config_methods & HAPD_WPS_CONFIG_VPIN) {
			strncat(out_val, "virtual_display ", out_sz);
			out_sz -= strlen(out_val);
		}
		if (config_methods & HAPD_WPS_CONFIG_PHYPIN) {
			strncat(out_val, "physical_display ", out_sz);
			out_sz -= strlen(out_val);
		}
	}

	return 0;
}

#if defined(MULTIAP)
#define HAPD_MAP_FH_BSS			0x20
#define HAPD_MAP_BH_BSS			0x40
#define HAPD_MAP_BH_STA			0x80

// Nvram values for fronthaul, backhaul bss and backhaul sta
#define HAPD_MAP_FH_BSS_NVVAL		0x1	// Fronthaul BSS
#define HAPD_MAP_BH_BSS_NVVAL		0x2	// Backhaul BSS
#define HAPD_MAP_BH_STA_NVVAL		0x4	// Backhaul STA

#define IS_HAPD_MAP_FH_BSS(val)		(val & HAPD_MAP_FH_BSS_NVVAL)
#define IS_HAPD_MAP_BH_BSS(val)		(val & HAPD_MAP_BH_BSS_NVVAL)
#define IS_HAPD_MAP_BH_STA(val)		(val & HAPD_MAP_BH_STA_NVVAL)

/* Callback fn to fill the multiap attribute */
static int
hapd_map_conv_fn(char *nvi_ifname, char *name, char *out_val, int out_sz)
{
	char tmp[HAPD_MIN_BUF] = {0}, *mode, *ptr = NULL;
	uint16 map_mode = 0, map = 0;
	uint16 val = 0;

	ptr = nvram_safe_get("multiap_mode");
	if (ptr[0] != '\0') {
		map_mode = (uint16)strtoul(ptr, NULL, 0);
	}

	snprintf(tmp, sizeof(tmp), "%s_%s", nvi_ifname, name);
	ptr = nvram_safe_get(tmp);
	if (ptr[0] != '\0') {
		map = (uint16)strtoul(ptr, NULL, 0);
	}

	if (!map_mode || !map) {
		return (MAP_FIELDS_NUM - 1);	/* Skip remaining multiap settings */
	}

	snprintf(tmp, sizeof(tmp), "%s_mode", nvi_ifname);
	mode = nvram_safe_get(tmp);

	if ((IS_HAPD_MAP_FH_BSS(map) || IS_HAPD_MAP_BH_BSS(map)) && strcmp(mode, "ap")) {
		cprintf("Err: rc: %s Multi-AP Fronthaul or Backhaul AP setting (%d) "
			"on a non AP interface\n", __FUNCTION__, map);
		return (MAP_FIELDS_NUM - 1);	/* Skip remaining multiap settings */
	}
	if (IS_HAPD_MAP_FH_BSS(map)) {
		val |= HAPD_MAP_FH_BSS;
	}
	if (IS_HAPD_MAP_BH_BSS(map)) {
		val |= HAPD_MAP_BH_BSS;
	}

	if (val) {
		snprintf(out_val, out_sz, "%d", val);
	} else {
		return (MAP_FIELDS_NUM - 1);	/* Unsupported multiap setting */
	}

	return 0;
}

/* Callback fn to fill multiap backhaul settings to be used by wps in hostapd */
static int
hapd_wpsmap_conv_fn(char *nvi_ifname, char *name, char *out_val, int out_sz)
{
	uint16 map = 0;
	char *ptr, *next, tmp[HAPD_MIN_BUF] = {0}, prefix[IFNAMSIZ] = {0};
	char *val = NULL, ifname[IFNAMSIZ];
	int akm = 0, auth = 0, encr;

	ptr = nvram_safe_get("wbd_ifnames");
	foreach(ifname, ptr, next) {
		if (osifname_to_nvifname(ifname, prefix, sizeof(prefix))) {
			continue;
		}
		snprintf(tmp, sizeof(tmp), "%s_map", prefix);
		val = nvram_safe_get(tmp);
		if (val[0] != '\0') {
			map = (uint16)strtoul(val, NULL, 0);
		}
		if (IS_HAPD_MAP_BH_BSS(map)) {
			break;
		}
	}

	if (prefix[0] == '\0') {
		return 3;	// skip map related backhaul settings.
	}
	hapd_wpasupp_get_security_details(prefix, &akm);
	switch (akm) {
		case HAPD_AKM_PSK:
			auth = HAPD_WPS_AUTH_WPAPSK;
		break;

		case HAPD_AKM_PSK2:
			auth = HAPD_WPS_AUTH_WPA2PSK;
		break;

		case HAPD_AKM_PSK| HAPD_AKM_PSK2:
			auth = HAPD_WPS_AUTH_WPAPSK | HAPD_WPS_AUTH_WPA2PSK;
		break;

		case HAPD_AKM_OPEN_WEP:
			auth = HAPD_WPS_AUTH_OPEN;
		break;

		default:
			cprintf("Err: rc: %s Multi-AP unsupported auth value %d "
				"for backhaul settings \n", __FUNCTION__, akm);
		return 3;	/* Skip remaining multiap settings */
		break;
	}

	if (!strcmp(name, "bh_ssid")) {
		snprintf(tmp, sizeof(tmp), "%s_ssid", prefix);
		snprintf(out_val, out_sz, nvram_safe_get(tmp));
	} else if (!strcmp(name, "bh_akm")) {
		snprintf(out_val, out_sz, "%d", auth);
	} else if (!strcmp(name, "bh_crypto")) {
		char *crypto;
		snprintf(tmp, sizeof(tmp), "%s_crypto", prefix);
		crypto = nvram_safe_get(tmp);
		if (auth == 0x01) {
			encr = HAPD_WPS_ENCR_NONE;
		} else {
			if (!strcmp(crypto, "aes")) {
				encr = HAPD_WPS_ENCR_AES;
			} else if (!strcmp(crypto, "tkip")) {
				encr = HAPD_WPS_ENCR_TKIP;
			} else if (!strcmp(crypto, "tkip+aes")) {
				encr = HAPD_WPS_ENCR_TKIP | HAPD_WPS_ENCR_AES;
			}
		}
		snprintf(out_val, out_sz, "%d", encr);
	} else if (!strcmp(name, "bh_psk")) {
		char *psk;
		snprintf(tmp, sizeof(tmp), "%s_wpa_psk", prefix);
		psk = nvram_safe_get(tmp);
		if (psk[0] != '\0') {
			snprintf(out_val, out_sz, psk);
		} else {
			return -1;
		}
	} else {
		return -1;
	}

	return 0;
}

/* Callback fn to fill multiap backhaul sta attribute to be used by wps in wpa supplicant */
static int
wpa_supp_map_conv_fv(char *nvi_ifname, char *name, char *out_val, int out_sz)
{
	char tmp[HAPD_MIN_BUF] = {0}, *ptr = NULL;
	uint16 map_mode = 0, map = 0;

	ptr = nvram_safe_get("multiap_mode");
	if (ptr[0] != '\0') {
		map_mode = (uint16)strtoul(ptr, NULL, 0);
	}

	snprintf(tmp, sizeof(tmp), "%s_%s", nvi_ifname, name);
	ptr = nvram_safe_get(tmp);
	if (ptr[0] != '\0') {
		map = (uint16)strtoul(ptr, NULL, 0);
	}

	if (!map_mode || !map) {
		return -1;
	}

	if (IS_HAPD_MAP_BH_STA(map)) {
		snprintf(out_val, out_sz, "1");
	} else {
		return -1;
	}

	return 0;
}

/* Callback fn to fill scan_ssid attribute to be used by multiap backhaul sta */
static int
wpa_supp_scan_ssid_conv_fn(char *nvi_ifname, char *name, char *out_val, int out_sz)
{

	char tmp[HAPD_MIN_BUF] = {0}, *ptr = NULL;
	uint16 map_mode = 0, map = 0;

	ptr = nvram_safe_get("multiap_mode");
	if (ptr[0] != '\0') {
		map_mode = (uint16)strtoul(ptr, NULL, 0);
	}

	snprintf(tmp, sizeof(tmp), "%s_map", nvi_ifname);
	ptr = nvram_safe_get(tmp);
	if (ptr[0] != '\0') {
		map = (uint16)strtoul(ptr, NULL, 0);
	}

	if (!map_mode || !map) {
		return -1;
	}

	ptr = nvram_safe_get(name);
	if (atoi(ptr) == 1 && IS_HAPD_MAP_BH_STA(map)) { // map_onboarded == 1 && Multiap BSTA
		// Since the Backhaul bss might have hidden ssid enabled so the sta needs to send
		// directed probe requests hence scan_ssid is set to 1.
		snprintf(out_val, out_sz, "1");
	} else {
		return -1;
	}

	return 0;
}

/* Check if the interface is map backhaul STA and the BSSID is provided for that STA */
static int
wpa_supp_map_is_bssid_join_possible(char *nvi_ifname)
{
	char tmp[HAPD_MIN_BUF] = {0}, *ptr = NULL;
	uint16 map_mode = 0, map = 0;
	struct ether_addr bssid;

	ptr = nvram_safe_get("multiap_mode");
	if (ptr[0] != '\0') {
		map_mode = (uint16)strtoul(ptr, NULL, 0);
	}

	snprintf(tmp, sizeof(tmp), "%s_map", nvi_ifname);
	ptr = nvram_safe_get(tmp);
	if (ptr[0] != '\0') {
		map = (uint16)strtoul(ptr, NULL, 0);
	}

	/* If multi ap disabled or the map is not STA return */
	if (!map_mode || !IS_HAPD_MAP_BH_STA(map)) {
		dprintf("Err: rc: %s ifname[%s] map_mode[%d] map[%d]\n", __FUNCTION__, nvi_ifname,
			map_mode, map);
		return 0;
	}

	snprintf(tmp, sizeof(tmp), "%s_bssid", nvi_ifname);
	ptr = nvram_safe_get(tmp);
	if (strlen(ptr) < 17 || !ether_atoe(ptr, &bssid)) {
		dprintf("Err: rc: %s ifname[%s] NVRAM[%s=%s]. No valid BSSID\n", __FUNCTION__,
			nvi_ifname, tmp, ptr);
		return 0;
	}

	return 1;
}
#endif	/* MULTIAP */

/* Callback fn to fill osen authentication */
static int
hapd_osen_conv_fn(char *nvi_ifname, char *name, char *out_val, int out_sz)
{
	char nv_name[HAPD_MAX_BUF] = {0};

	snprintf(nv_name, sizeof(nv_name), "%s_%s", nvi_ifname, name);

	if (nvram_match(nv_name, "osen")) {
		snprintf(out_val, out_sz, "%d", HAPD_ENABLE);
	} else {
		snprintf(out_val, out_sz, "%d", HAPD_DISABLE);
	}

	return 0;
}

static int
hapd_dgaf_conv_fn(char *nvi_ifname, char *name, char *out_val, int out_sz)
{
	char nv_name[HAPD_MAX_BUF] = {0};
	char *ptr;

	snprintf(nv_name, sizeof(nv_name), "%s_%s", nvi_ifname, name);

	ptr = nvram_get_bitflag(nv_name, HSFLG_DGAF_DS);

	if (ptr != NULL && atoi(ptr)) {
		snprintf(out_val, out_sz, "%d", HAPD_ENABLE);
	} else {
		snprintf(out_val, out_sz, "%d", HAPD_DISABLE);
	}
	return 0;
}

/* Callback fn to fill mfp parameters */
static int
hapd_mfp_conv_fn(char *nvi_ifname, char *name, char *out_val, int out_sz)
{
	char nv_name[HAPD_MAX_BUF] = {0};
	int akm = 0;

	hapd_wpasupp_get_security_details(nvi_ifname, &akm);

	/* In case of pure SAE, MFP should be required */
	if (akm == HAPD_AKM_WPA3_SAE) {
		snprintf(out_val, out_sz, "%d", HAPD_MFP_REQ);
		return 0;
	}
	/* In case of pure WPA-PSK, MFP should be disabled */
	if (akm == HAPD_AKM_PSK) {
		snprintf(out_val, out_sz, "%d", HAPD_MFP_OFF);
		return 0;
	}

	snprintf(nv_name, sizeof(nv_name), "%s_%s", nvi_ifname, name);

	if (nvram_match(nv_name, "0")) {
		snprintf(out_val, out_sz, "%d", HAPD_MFP_OFF);
	} else if (nvram_match(nv_name, "2")) {
		snprintf(out_val, out_sz, "%d", HAPD_MFP_REQ);
	} else { /* PMF Capable by default */
		snprintf(out_val, out_sz, "%d", HAPD_MFP_CAP);
	}

	return 0;
}

/* Callback fn to fill wmm parameters */
static int
hapd_wmmconfig_conv_fn(char *nvi_ifname, char *name, char *out_val, int out_sz)
{
	char nv_name[HAPD_MAX_BUF] = {0};

	snprintf(nv_name, sizeof(nv_name), "%s_%s", nvi_ifname, name);

	if (nvram_match(nv_name, "1")) {
		snprintf(out_val, out_sz, HAPD_UNSUPPORTED);
	} else {
		snprintf(out_val, out_sz, HAPD_SUPPORTED);
	}

	return 0;
}

/* Callback fn to fill dtim period */
static int
hapd_dtim_conv_fn(char *nvi_ifname, char *name, char *out_val, int out_sz)
{
	char nv_name[HAPD_MAX_BUF] = {0}, wl_pri_ifname[IFNAMSIZ] = {0};

	/* dtim is per radio setting.Get primary ifname from nvi_ifname to build nvram */
	if (hapd_wpasupp_get_primary_ifce(nvi_ifname, wl_pri_ifname, sizeof(wl_pri_ifname)) < 0) {
		dprintf("Err: rc: %s fetching primary interface name\n", __FUNCTION__);
		return -1;
	}

	snprintf(nv_name, sizeof(nv_name), "%s_%s", wl_pri_ifname, name);

	snprintf(out_val, out_sz, nvram_safe_get(nv_name));

	return 0;
}

/* Callback fn to fill beacon interval */
static int
hapd_bi_conv_fn(char *nvi_ifname, char *name, char *out_val, int out_sz)
{

	char nv_name[HAPD_MAX_BUF] = {0}, wl_pri_ifname[IFNAMSIZ] = {0};

	/* Bcn interval is per radio setting. Get primary ifname from nvi_ifname to build nvram */
	if (hapd_wpasupp_get_primary_ifce(nvi_ifname, wl_pri_ifname, sizeof(wl_pri_ifname)) < 0) {
		dprintf("Err: rc: %s fetching primary interface name\n", __FUNCTION__);
		return -1;
	}

	snprintf(nv_name, sizeof(nv_name), "%s_%s", wl_pri_ifname, name);

	snprintf(out_val, out_sz, nvram_safe_get(nv_name));

	return 0;
}

/* Callback fn to fill country code - mandatory for 11d */
static int
hapd_cc_conv_fn(char *nvi_ifname, char *name, char *out_val, int out_sz)
{
	char nv_name[HAPD_MAX_BUF] = {0}, wl_pri_ifname[IFNAMSIZ] = {0};

	/* Country code is per radio setting. Get primary ifname from nvi_ifname to build nvram */
	if (hapd_wpasupp_get_primary_ifce(nvi_ifname, wl_pri_ifname, sizeof(wl_pri_ifname)) < 0) {
		dprintf("Err: rc: %s fetching primary interface name\n", __FUNCTION__);
		return -1;
	}

	snprintf(nv_name, sizeof(nv_name), "%s_%s", wl_pri_ifname, name);

	snprintf(out_val, out_sz, nvram_safe_get(nv_name));

	return 0;
}

/* Callback fn to fill 11d parameters */
static int
hapd_11d_conv_fn(char *nvi_ifname, char *name, char *out_val, int out_sz)
{
	char nv_name[HAPD_MAX_BUF] = {0}, wl_pri_ifname[IFNAMSIZ] = {0};

	/* 11d is per radio setting. Get primary ifname from nvi_ifname to build nvram */
	if (hapd_wpasupp_get_primary_ifce(nvi_ifname, wl_pri_ifname, sizeof(wl_pri_ifname)) < 0) {
		dprintf("Err: rc: %s fetching primary interface name\n", __FUNCTION__);
		return -1;
	}

	snprintf(nv_name, sizeof(nv_name), "%s_%s", wl_pri_ifname, name);

	snprintf(out_val, out_sz, HAPD_SUPPORTED);

	return 0;
}

/* Callback fn to fill 11h parameters */
static int
hapd_11h_conv_fn(char *nvi_ifname, char *name, char *out_val, int out_sz)
{
	char nv_name[HAPD_MAX_BUF] = {0}, wl_pri_ifname[IFNAMSIZ] = {0};

	/* 11h is per radio setting. Get primary ifname from nvi_ifname to build nvram */
	if (hapd_wpasupp_get_primary_ifce(nvi_ifname, wl_pri_ifname, sizeof(wl_pri_ifname)) < 0) {
		dprintf(" Err: rc: %s fetching primary interface name\n", __FUNCTION__);
		return -1;
	}

	snprintf(nv_name, sizeof(nv_name), "%s_%s", wl_pri_ifname, name);

	snprintf(out_val, out_sz, HAPD_SUPPORTED);

	return 0;
}

/* Settings deferred flag skips particular setting to config file */
static void
hapd_wpasupp_set_deferred(int cur_pos, int count)
{
	int idx = 0;
	while (cfg_arr[cur_pos + idx].nv_name != NULL && idx < count) {
		cfg_arr[cur_pos + idx].flags |= HAPD_CFG_DEFER;
		idx++;
	}
}

/* Fills the nvram settings to the config file */
static void
hapd_fill_nvrams_to_config_file(FILE *fp, char *name, uint32 flags)
{
	int idx;
	char tmp[HAPD_MAX_BUF] = {0};
	char nvram_name[HAPD_MAX_BUF] = {0};

	for (idx = 0; cfg_arr[idx].nv_name != NULL; idx++) {
		char *val = NULL, *val2 = NULL, cb_out[HAPD_MAX_BUF] = {0};
		if (!(cfg_arr[idx].flags & flags)) {
			continue;
		}
		// If defer flag is set, first clear it than continue.
		if (cfg_arr[idx].flags & HAPD_CFG_DEFER) {
			cfg_arr[idx].flags &= (~HAPD_CFG_DEFER);
			continue;
		}
		if (cfg_arr[idx].flags & HAPD_CFG_USEDEFAULT) {
			val = cfg_arr[idx].def_val;
		} else {
			int len = 0;
			 // If large buf is set allocate dynamic memory.
			if (cfg_arr[idx].flags & HAPD_CFG_LRG_BUF) {
				val2 = (char *) malloc((8 * (sizeof(char)) * HAPD_MAX_BUF));
				if (val2 != NULL) {
					memset(val2, 0, 8 * HAPD_MAX_BUF);
					len = 8 * HAPD_MAX_BUF;
					val = val2;
				}
			} else {
				val = cb_out;
				len = sizeof(cb_out);
			}
			cfg_arr[idx].flags &= (~HAPD_CFG_DEFER);
			if (cfg_arr[idx].cb_fn != NULL) {
				int ret = cfg_arr[idx].cb_fn(name, cfg_arr[idx].nv_name,
						val, len);
			       if (ret < 0) {
					dprintf("Err: rc: %s conversion fn returned err"
						" for %s in ifr %s\n", __FUNCTION__,
						cfg_arr[idx].nv_name, name);
					continue;
				}
				// A +ve means we need to set the defered flags for next ret vals
				if (ret > 0) {
					hapd_wpasupp_set_deferred(idx + 1, ret);
				}
			} else {
				if (cfg_arr[idx].flags & HAPD_CFG_PERBSS) {
					snprintf(nvram_name, sizeof(nvram_name), "%s_%s",
						name, cfg_arr[idx].nv_name);
				} else {
					snprintf(nvram_name, sizeof(nvram_name),
						cfg_arr[idx].nv_name);
				}
				val = nvram_safe_get(nvram_name);
			}
		}

		if (val[0] == '\0') {
			continue;
		}

		if ((cfg_arr[idx].flags & WPA_CFG_PSK_CHECK) &&
				strlen(val) == WPA2_PSK_HEXLEN && hapd_hex_validate(val))
			snprintf(tmp, sizeof(tmp), "wpa_psk=%s\n", val);
		else
			snprintf(tmp, sizeof(tmp), "%s=%s\n", cfg_arr[idx].placeholder, val);
		fprintf(fp, tmp);
		if (val2) {
			free(val2);
		}
	}
}

/* Append the fixed settings to the config file */
static void
hapd_fixed_strs_to_config_file(FILE *fp)
{
	fprintf(fp, "## General configurations\n");
	fprintf(fp, "driver=nl80211\n");
	fprintf(fp, "ctrl_interface_group=0\n");
}

/* File filename and flags based on the interface name */
static int
hapd_get_config_filename(char *ifname, char *out_filename, int size, uint32 *out_flags, int mode)
{
	char prefix[IFNAMSIZ] = {0};
	int unit = -1, subunit = -1;
	uint32 flags = HAPD_CFG_IFR;

	if (osifname_to_nvifname(ifname, prefix, sizeof(prefix))) {
		dprintf("Err: rc: %s invalid interface name  %s \n", __FUNCTION__, ifname);
		return -1;
	}

	if (get_ifname_unit(prefix, &unit, &subunit)) {
		dprintf("Err: rc: %s invalid interface name prefix %s \n", __FUNCTION__, prefix);
		return -1;
	}

	if (unit >= MAX_RADIO_NUM) {
		dprintf("Err: rc: %s invalid radio unit %s, MAX_RADIO_NUM is %d \n", __FUNCTION__,
			unit, MAX_RADIO_NUM);
		return -1;
	}

	if (subunit > 0 && mode == HAPD_ON_PRIMARY_IFACE) {
		flags = HAPD_CFG_BSS;
	}

	if (subunit > 0 && mode != HAPD_ON_PRIMARY_IFACE) { /* repeater mode */
		if (radio_params_set[unit] == FALSE) { /* radio params not set initially */
			/*  hostapd.conf file of the form - wlX.Y_hapd.conf */
			snprintf(out_filename, size, "%s/wl%d.%d_%s",
				HAPD_FILE_DIR, unit, subunit, HAPD_FILENAME_SUFFIX);

			/* Set flags to fill the radio params - interface, hw_mode, channel,
			 * only for the first virtual interface in the virtual iflist while
			 * populating hostapd.conf file.
			 */
			flags = HAPD_CFG_IFR;
			radio_params_set[unit] = TRUE;
		} else {
			flags = HAPD_CFG_BSS;
		}

	} else {
		snprintf(out_filename, size, "%s/wl%d_%s",
			HAPD_FILE_DIR, unit, HAPD_FILENAME_SUFFIX);
	}

	if (out_flags != NULL) {
		*out_flags = flags;
	}

	return 0;
}

/* Creates the hostapd config file for ifname */
static int
hapd_create_config_file(char *ifname, char *filename, uint32 flags)
{
	char *mode = "w", prefix[IFNAMSIZ] = {0};
	FILE *fp = NULL;

	if (osifname_to_nvifname(ifname, prefix, sizeof(prefix))) {
		dprintf("Err: rc: %s invalid interface name  %s \n", __FUNCTION__, ifname);
		return -1;
	}

	if (flags == HAPD_CFG_BSS) {
		mode = "a";
	}

	fp = fopen(filename, mode);
	if (fp == NULL) {
		dprintf("Err: rc: %s failed to open the file %s \n", __FUNCTION__, filename);
		return -1;
	}

	if (flags & HAPD_CFG_IFR) {
		hapd_fixed_strs_to_config_file(fp);
	}

	if (flags & HAPD_CFG_IFR) {
		fprintf(fp, "## Interface configurations\n");
	} else if (flags & HAPD_CFG_BSS) {
		fprintf(fp, "## BSS configurations\n");
	}

	hapd_fill_nvrams_to_config_file(fp, prefix, flags);

	fclose(fp);

	return 0;
}

/* Pass an ifce (primary/virtual) of the form wlX or wlX.Y to get corresponding
 * primary ifce wlX.
 */
static int
hapd_wpasupp_get_primary_ifce(char *nvi_ifname, char *wl_pri_ifname, int sz)
{
	int unit = -1;
	char wl_ifname[IFNAMSIZ] = {0};

	if (!nvi_ifname || !wl_pri_ifname) {
		dprintf("Err: rc: %s Empty buffer\n", __FUNCTION__);
		return -1;
	}

	if (get_ifname_unit(nvi_ifname, &unit, NULL) < 0) {
		dprintf("Err: rc: %s\n", __FUNCTION__);
		return -1;
	}

	if (*ifname_arr[unit] == '\0') {
		snprintf(wl_ifname, sizeof(wl_ifname), "wl%d", unit);
		strncpy(ifname_arr[unit], wl_ifname, sizeof(wl_ifname));
		ifname_arr[unit][sizeof(wl_ifname) - 1] = '\0';
	}

	memset(wl_pri_ifname, 0, sz);

	strncpy(wl_pri_ifname, ifname_arr[unit], MIN(IFNAMSIZ, sz) -  1);
	wl_pri_ifname[MIN(IFNAMSIZ, sz) - 1] = '\0';

	return 0;
}

/* Check if interface is primary or not */
static bool
hapd_wpasupp_is_primary_ifce(const char *ifname)
{
	int unit = -1, subunit = -1;
	char prefix[IFNAMSIZ] = {0};

	if (osifname_to_nvifname(ifname, prefix, sizeof(prefix))) {
		dprintf("Err: rc: %s invalid interface name\n", __FUNCTION__);
		return FALSE;
	}

	if (get_ifname_unit(prefix, &unit, &subunit) < 0) {
		dprintf("Err: rc: %s invalid interface name prefix\n", __FUNCTION__);
		return FALSE;
	}

	if (subunit > 0) {
		dprintf("Err: rc: %s Non primary interface\n", __FUNCTION__);
		return FALSE;
	}

	return TRUE;
}

static bool
hapd_wpasupp_is_bss_enabled(const char* ifname)
{
	char wl_prefix[IFNAMSIZ] = {0};
	char nv_name[HAPD_MAX_BUF] = {0};
	bool ret = FALSE;

	if (osifname_to_nvifname(ifname, wl_prefix, sizeof(wl_prefix))) {
		dprintf("Err: rc: %s invalid interface name  %s \n", __FUNCTION__, ifname);
		return -1;
	}

	snprintf(nv_name, sizeof(nv_name),  "%s_bss_enabled", wl_prefix);

	ret =  nvram_match(nv_name, "1");

	return ret;
}

/* Given a list of ifnames, the fn returns two lists - one containing primary radio ifces
 * and other containing secondary (virtual) ifces.
 */
static void
hapd_wpasupp_get_primary_virtual_iflist(char *filtered_list,
	char *pr_iflist, char *sec_iflist, int listsz)
{
	char ifname[IFNAMSIZ] = {0}, *next;

	foreach(ifname, filtered_list, next) {
		((hapd_wpasupp_is_primary_ifce(ifname) == TRUE) ?
			add_to_list(ifname, pr_iflist, listsz) :
			add_to_list(ifname, sec_iflist, listsz));
	}
}

/* Given a list of ifnames, the fn returns a filtered list of wireless interfaces on
 * which BSS is enabled.
 */
static void
hapd_wpasupp_get_filtered_ifnames_list(char *ifnames_list, char *filtered_list, int flist_sz)
{
	char pr_ifname[IFNAMSIZ] = {0}; /* primary ifname */
	char ifname[IFNAMSIZ] = {0}, tmp_list[HAPD_MAX_BUF] = {0};
	char *next;

	foreach(ifname, ifnames_list, next) {
		if (!wl_probe(ifname)) { /* only wireless interfaces */
			if (hapd_wpasupp_is_primary_ifce(ifname) == TRUE) {
				/* copy primary BSS ifname */
				strncpy(pr_ifname, ifname, IFNAMSIZ);
			} else { /* add all vritual BSS' to temporary list */
				add_to_list(ifname, tmp_list, HAPD_MAX_BUF);
			}
		}
	}

	/* if BSS on primary ifce is enabled, add it to filtered list */
	if (hapd_wpasupp_is_bss_enabled(pr_ifname)) {
		add_to_list(pr_ifname, filtered_list, flist_sz);
	}

	/* Check virtual BSS'. If any of the virtual BSS' enabled, then, blindly
	 * add the primary BSS to the filtered list even if it is not enabled.
	 */
	foreach(ifname, tmp_list, next) {
		if (hapd_wpasupp_is_bss_enabled(ifname)) {
			add_to_list(ifname, filtered_list, flist_sz);
		}
	}
}

static bool
hapd_wpasupp_is_ifce_ap(const char *ifname)
{
	char prefix[IFNAMSIZ] = {0}, nv_name[HAPD_MAX_BUF] = {0};
	bool ret = FALSE;

	if (osifname_to_nvifname(ifname, prefix, sizeof(prefix))) {
		dprintf("Err: rc: %s invalid interface name  %s \n", __FUNCTION__, ifname);
		return -1;
	}

	snprintf(nv_name, sizeof(nv_name),  "%s_mode", prefix);

	ret = nvram_match(nv_name, "ap");

	return ret;
}

/* Starts the hostapd.
 * If AP, run hostapd on all fileterd interfaces (ethX, wlX.Y...)
 * If non-AP/repeater, run hostapd on secondary/virtual ifce (wlX.Y).
 */
static int
start_hostapd(char *filtered_iflist, char *primary_iflist, char *virtual_iflist)
{
	uint32 flags = 0;
	int mode = HAPD_ON_PRIMARY_IFACE;
	char filename[HAPD_MAX_BUF] = {0}, cmd[HAPD_MAX_BUF * 2] = {0};
	char ifname[IFNAMSIZ] = {0}, prefix[IFNAMSIZ] = {0};
	char *next, *filelist;
	char *ifnames_list = filtered_iflist; /* if AP, run hostapd on primary ifces */
	int listsz = HAPD_MAX_BUF * 4, usedsz = 0, num_primary = 0;

	if (!filtered_iflist) {
		dprintf("Err: rc: %s empty filtered iface list\n", __FUNCTION__);
		return -1;
	}

	filelist = (char *)calloc(1, listsz);
	if (!filelist) {
		dprintf("Err: rc: %s calloc error\n", __FUNCTION__, __LINE__);
		return -1;
	}

	foreach(ifname, primary_iflist, next) {
		if (hapd_wpasupp_is_ifce_ap(ifname) == FALSE) { /* non AP mode */
			if (!virtual_iflist) {
				dprintf("Err: rc: %s empty secondary ifce list\n", __FUNCTION__);
				free(filelist);
				return -1;
			}
			ifnames_list = virtual_iflist; /* run hostapd on virtual ifces */
			mode = HAPD_ON_VIRTUAL_IFACE;
		}
		num_primary++;
	}

	if (num_primary == 0) {
		/* If primary is not enabled, run hostapd on virtual ifces */
		ifnames_list = virtual_iflist;
		mode = HAPD_ON_VIRTUAL_IFACE;
	}

#ifdef HAPD_WPASUPP_DBG
	PRINT_IFLIST(ifnames_list);
#endif /* HAPD_WPASUPP_DBG */

	foreach(ifname, ifnames_list, next) {
		if (osifname_to_nvifname(ifname, prefix, sizeof(prefix))) {
			dprintf("Err: rc: %s invalid interface name\n", __FUNCTION__);
			free(filelist);
			return -1;
		}

		if (hapd_get_config_filename(prefix, filename,
				sizeof(filename), &flags, mode) < 0) {
			continue;
		}

		if (hapd_create_config_file(prefix, filename, flags) < 0) {
			dprintf("Err: rc: %s hostapd config file %s creation for %s failed\n",
				__FUNCTION__, filename, ifname);
			continue;
		}

		// Store all the filenames into the list.
		if ((usedsz + strlen(filename) + 1 /* Space */ + 1 /* NULL */) > listsz) {
			char *tmp = (char *)calloc(2, listsz);
			if (!tmp) {
				dprintf("Err: rc: %s calloc error at %d \n",
					__FUNCTION__, __LINE__);
				goto end;
			}
			memcpy(tmp, filelist, usedsz);
			free(filelist);
			filelist = tmp;
			listsz *= 2;
		}
		add_to_list(filename, filelist, listsz);
		usedsz = strlen(filelist);
	}

end:
	foreach(filename, filelist, next) {
		cprintf("Running hostapd instance using %s configurations\n", filename);
		snprintf(cmd, sizeof(cmd), "hostapd %s %s &",
				(nvram_match("hapd_dbg", "1") ? "-ddt" : "-B"), filename);
		system(cmd);
	}

	free(filelist);

	return 0;
}

/* Stops the hostapd */
int
stop_hostapd()
{
	int ret = 0, i;
	char cmd[HAPD_MAX_BUF * 2] = {0};

	ret = eval("killall", "-9", "hostapd");

	snprintf(cmd, sizeof(cmd), "rm -f /tmp/*_hapd.conf");
	system(cmd);

	memset(cmd, 0, sizeof(cmd));

	snprintf(cmd, sizeof(cmd), "rm -rf /var/run/hostapd");
	system(cmd);

	for (i = 0; i < MAX_RADIO_NUM; i++)
		radio_params_set[i] = FALSE;

	return ret;
}

static void
stop_hostapd_per_radio(int radio_idx)
{
	DIR *dir;
	struct dirent *ent;
	char path[PATH_MAX];
	FILE *fp = NULL;
	bool bFound = FALSE;
	char nv_name[HAPD_MAX_BUF] = {0};
	char *nvi_ifname = NULL, *pStrTmp = NULL;
	char cmd[HAPD_MAX_BUF * 2] = {0};

	snprintf(nv_name, sizeof(nv_name), "wl%d_ifname", radio_idx);
	nvi_ifname = nvram_get(nv_name);

	if ((dir = opendir("/proc")) == NULL) {
		dprintf("%s fail to open /proc\n", __FUNCTION__);
	} else {
		while ((ent = readdir(dir)) != NULL) {
			if (!strspn(ent->d_name, "0123456789"))
				continue;

			path[0] = '\0';
			strcat(path, "/proc/");
			strcat(path, ent->d_name);
			strcat(path, "/");
			strcat(path, "cmdline");
			fp = fopen(path, "r");
			if (!fp)
				continue;

			if (fgets(cmd, sizeof(cmd), fp) != NULL) {
			/*
			* hexdump -c /proc/4361/cmdline
			* 0000000   h   o   s   t   a   p   d  \0   -   B  \0   /   t   m   p   /
			* 0000010   w   l   0   _   h   a   p   d   .   c   o   n   f  \0
			*/
				if (!strstr(cmd, "hostapd")) {
					fclose(fp);
					continue;
				}

				/* to skip "hostapd" */
				pStrTmp = cmd;
				pStrTmp += strlen(pStrTmp);
				++pStrTmp;

				/* to skip "-B" */
				pStrTmp += strlen(pStrTmp);
				++pStrTmp;

				/* now we come to /tmp/wlx_hapd.conf */
				pStrTmp = strstr(pStrTmp, nvi_ifname);
				if (pStrTmp) {
					pStrTmp += strlen(nvi_ifname);
					/* config file name is wlx_hapd.conf or wlx.y_hapd.conf */
					if (*pStrTmp == '.' || *pStrTmp == '_') {
						bFound = TRUE;
						dprintf("%s kill hostapd instance %s for %s\n",
							__FUNCTION__, ent->d_name, nvi_ifname);
						eval("kill", ent->d_name);
					}
				}
			}
			fclose(fp);
			if (bFound) {
				break;
			}
		}
		closedir(dir);
	}

	if (!bFound) {
		dprintf("Err: %s hostapd instance for %s does not exist\n",
			__FUNCTION__, nvi_ifname);
	}

	memset(cmd, 0, sizeof(cmd));
	snprintf(cmd, sizeof(cmd), "rm -f /tmp/%s_hapd.conf", nvi_ifname);
	system(cmd);

	memset(cmd, 0, sizeof(cmd));
	snprintf(cmd, sizeof(cmd), "rm -f /tmp/%s.*_hapd.conf", nvi_ifname);
	system(cmd);

	memset(cmd, 0, sizeof(cmd));
	snprintf(cmd, sizeof(cmd), "rm -rf /var/run/hostapd/%s", nvi_ifname);
	system(cmd);

	memset(cmd, 0, sizeof(cmd));
	snprintf(cmd, sizeof(cmd), "rm -rf /var/run/hostapd/%s.*", nvi_ifname);
	system(cmd);

	/* /var/run/hostapd to be removed when it is empty */
	memset(cmd, 0, sizeof(cmd));
	snprintf(cmd, sizeof(cmd), "rmdir /var/run/hostapd");
	system(cmd);

	radio_params_set[radio_idx] = FALSE;

	return;
}

/* Callback fn to fill wps cred processing field */
static int
wpa_supp_wps_cred_processing_conv_fn(char *nvi_ifname, char *name, char *out_val, int out_sz)
{
	/*
	 * When multiap is enabled and the enrollee is backhaul sta WPS should not process the
	 * received credentials instead it has to pass them over to the control interface.
	 * In all other cases wps should process the credentials as well as pass them over to the
	 * control interface for processing to external programs.
	 */
#ifdef MULTIAP
	char tmp[HAPD_MIN_BUF] = {0}, *ptr = NULL;
	uint16 map_mode = 0, map = 0;

	ptr = nvram_safe_get("multiap_mode");
	if (ptr[0] != '\0') {
		map_mode = (uint16)strtoul(ptr, NULL, 0);
	}

	if (map_mode > 0) {
		snprintf(tmp, sizeof(tmp), "%s_%s", nvi_ifname, name);
		ptr = nvram_safe_get(tmp);
		if (ptr[0] != '\0') {
			map = (uint16)strtoul(ptr, NULL, 0);
		}
	}

	if (IS_HAPD_MAP_BH_STA(map)) {
		snprintf(out_val, out_sz, HAPD_WPS_DO_NOT_PROCESS_CREDS_PASS_TO_CTRL_INTERFACE);
	} else
#endif /* MULTIAP */
	{
		snprintf(out_val, out_sz, HAPD_WPS_PROCESS_CREDS_AND_PASS_TO_CTRL_INTERFACE);
	}

	return 0;
}

static int
wpa_supp_key_mgmt_conv_fn(char *nvi_ifname, char *name, char *out_val, int out_sz)
{
	char nv_name[HAPD_MAX_BUF] = {0}, *key_mgmt = HAPD_KEY_MGMT_WPA;
	int akm = 0, ret = 0;
	bool psk_required;

	hapd_wpasupp_get_security_details(nvi_ifname, &akm);

	psk_required = ((akm & HAPD_AKM_PSK2) || (akm & HAPD_AKM_PSK)) ? TRUE : FALSE;

	switch (akm) {
		/* open | wep security in both the cases wpa should be 0 */
		case HAPD_AKM_OPEN_WEP:
			if (!strcmp(name, "akm")) {
				snprintf(out_val, out_sz, "%s", WPA_SUPP_SEC_OPEN_OR_WEP);
			}
			// skip akm, crypto, wpa_psk and mfp
			ret = 3;
			break;

			/* HAPD_SEC_WPA */
		case HAPD_AKM_PSK:
			if (!strcmp(name, "wpa")) {
				snprintf(out_val, out_sz, "%d", HAPD_SEC_WPA);
			} else if (!strcmp(name, "crypto")) {
				snprintf(nv_name, sizeof(nv_name), "%s_%s", nvi_ifname, name);
				if (!strcmp(nvram_safe_get(nv_name), "tkip")) {
					snprintf(out_val, out_sz, HAPD_CIPHER_SUITE_TKIP);
				}
			} else if (!strcmp(name, "akm")) {
				snprintf(out_val, out_sz, key_mgmt);
			} else if (psk_required && !strcmp(name, "wpa_psk")) {
				snprintf(nv_name, sizeof(nv_name), "%s_%s", nvi_ifname, name);
				snprintf(out_val, out_sz, nvram_safe_get(nv_name));
			}
			break;

			/* HAPD_SEC_WPA2 */
		case HAPD_AKM_PSK2:
			if (!strcmp(name, "akm")) {
				snprintf(out_val, out_sz, "%s", "WPA-PSK");
			} else if (!strcmp(name, "proto")) {
				snprintf(out_val, out_sz, "%s", "RSN");
			} else if (!strcmp(name, "crypto")) {
				snprintf(nv_name, sizeof(nv_name), "%s_%s", nvi_ifname, name);
				if (!strcmp(nvram_safe_get(nv_name), "aes")) {
					snprintf(out_val, out_sz, "CCMP TKIP");
				}
			} else if (psk_required && !strcmp(name, "wpa_psk")) {
				snprintf(nv_name, sizeof(nv_name), "%s_%s", nvi_ifname, name);
				snprintf(out_val, out_sz, "\"%s\"", nvram_safe_get(nv_name));
			}
			break;

			/* HAPD_SEC_WPA-HAPD_SEC_WPA2 */
		case HAPD_AKM_WPA:
			if (!strcmp(name, "wpa")) {
				snprintf(out_val, out_sz, "%d", (HAPD_SEC_WPA | HAPD_SEC_WPA2));
			} else if (!strcmp(name, "crypto")) {
				snprintf(nv_name, sizeof(nv_name), "%s_%s", nvi_ifname, name);
				if (!strstr(nvram_safe_get(nv_name), "aes")) {
					snprintf(out_val, out_sz, HAPD_CIPHER_SUITE_CCMP);
				}
			} else if (!strcmp(name, "akm")) {
				snprintf(out_val, out_sz, key_mgmt);
			} else if (psk_required && !strcmp(name, "wpa_psk")) {
				snprintf(nv_name, sizeof(nv_name), "%s_%s", nvi_ifname, name);
				snprintf(out_val, out_sz, nvram_safe_get(nv_name));
			}
			break;

		default:
			return -1;
	};

	if (out_val[0] == '\0') {
		return -1;
	}

	return ret;
}

static int
wpa_supp_ssid_conv_fn(char *nvi_ifname, char *nv_name, char *out_val, int out_sz)
{
	char nvram_name[HAPD_MAX_BUF] = {0};
	char *val = NULL;

	snprintf(nvram_name, sizeof(nvram_name), "%s_%s", nvi_ifname, nv_name);
	val = nvram_safe_get(nvram_name);

	snprintf(out_val, out_sz, "\"%s\"", val);

	return 0;
}

/* Settings deferred flag skips particular setting to config file */
static void
wpa_supp_set_deferred(int cur_pos, int count)
{
	int idx = 0;
	while (wpa_supp_cfg_arr[cur_pos + idx].nv_name != NULL && idx < count) {
		wpa_supp_cfg_arr[cur_pos + idx].flags |= HAPD_CFG_DEFER;
		idx++;
	}
}

static void
wpa_supp_fill_nvrams_to_config_file(FILE *fp, char *ifname, uint32 flags,
	hapd_nvram_config_t *cfg_arr)
{
	int idx;
	char tmp[HAPD_MAX_BUF] = {0};
	char nvram_name[HAPD_MAX_BUF] = {0};

	for (idx = 0; cfg_arr[idx].nv_name != NULL; idx++) {
		char *val = NULL, cb_out[HAPD_MAX_BUF] = {0};

		if (!(cfg_arr[idx].flags & flags)) {
			continue;
		}

		// If defer flag is set, first clear it than continue.
		if (cfg_arr[idx].flags & HAPD_CFG_DEFER) {
			cfg_arr[idx].flags &= (~HAPD_CFG_DEFER);
			continue;
		}

		if (cfg_arr[idx].flags & HAPD_CFG_USEDEFAULT) {
			val = cfg_arr[idx].def_val;
		} else {
			if (cfg_arr[idx].cb_fn != NULL) {
				/* do something */
				int ret = cfg_arr[idx].cb_fn(
						ifname,
						cfg_arr[idx].nv_name,
						cb_out,
						sizeof(cb_out));
			       if (ret < 0) {
					dprintf("Err: rc: %s conversion fn returned err"
						" for %s in ifr %s\n", __FUNCTION__,
						cfg_arr[idx].nv_name, ifname);
					continue;
				}
				val = cb_out;
				// A +ve means we need to set the defered flags for next ret vals
				if (ret > 0) {
					wpa_supp_set_deferred(idx + 1, ret);
				}
			} else {
				if (cfg_arr[idx].flags & HAPD_CFG_PERBSS) {
					snprintf(nvram_name, sizeof(nvram_name), "%s_%s",
						ifname, cfg_arr[idx].nv_name);
				} else {
					snprintf(nvram_name, sizeof(nvram_name),
						cfg_arr[idx].nv_name);
				}
				val = nvram_safe_get(nvram_name);
			}
		}

		if (val[0] == '\0') {
			val = cfg_arr[idx].def_val;
			//continue;
		}

		if (flags == WPA_SUP_CFG_GBL) {
			snprintf(tmp, sizeof(tmp), "%s=%s\n",
				cfg_arr[idx].placeholder, val);
		} else {
			snprintf(tmp, sizeof(tmp), "\t%s=%s\n",
					cfg_arr[idx].placeholder, val);
		}
		fprintf(fp, tmp);
	}
}

static int
wpa_supp_get_config_filename(char *prefix_ifname, char *out_filename, int size, uint32 *out_flags)
{
	uint32 flags = HAPD_CFG_IFR; /* per radio */

	snprintf(out_filename, size, "%s/%s_%s",
		WPA_SUPP_FILE_DIR, prefix_ifname, WPA_SUPP_FILENAME_SUFFIX);

	if (out_flags != NULL) {
		*out_flags = flags;
	}

	return 0;
}

static int
wpa_supp_create_config_file(char *prefix, char *filename, uint32 flags)
{
	char *mode = "w";
	FILE *fp = NULL;
	char nv_name[HAPD_MAX_BUF] = {0};

	fp = fopen(filename, mode);
	if (fp == NULL) {
		fprintf(stderr, "Failed to open file: %s\n", filename);
		return -1;
	}

	fprintf(fp, "update_config=1\n");
	fprintf(fp, "ctrl_interface=/var/run/%s_wpa_supplicant\n", prefix);
	fprintf(fp, "ctrl_interface_group=0\n");
	/* Fills the global config values eg: wps config */
	wpa_supp_fill_nvrams_to_config_file(fp, prefix, WPA_SUP_CFG_GBL, wpa_supp_cfg_arr);

	snprintf(nv_name, sizeof(nv_name), "%s_ap_scan", prefix);
	if (nvram_match(nv_name, "0")) {
		/* driver performs ap scanning/selection */
		fprintf(fp, "\nap_scan=0\n");
	}

#ifdef MULTIAP
	/* Add Multi-Ap specific network block configuration which has some extra parameters.
	 * Let it be the first network block so that, it gets highest priority
	 */
	if (wpa_supp_map_is_bssid_join_possible(prefix)) {
		fprintf(fp, "\nnetwork={\n");

		wpa_supp_fill_nvrams_to_config_file(fp, prefix, flags, wpa_supp_map_nw_cfg_arr);

		fprintf(fp, "}\n");
	}
#endif /* MULTIAP */

	/* Add common network block configuration. This network block will be selected if the
	 * above network block fails to connect
	 */
	fprintf(fp, "\nnetwork={\n");

	wpa_supp_fill_nvrams_to_config_file(fp, prefix, flags, wpa_supp_nw_cfg_arr);

	fprintf(fp, "}\n");

	fclose(fp);

	return 0;
}

/* run wpa_supplicant on primary interfaces (ethX, ethY...) which are in non-AP/repeater mode.
 */
static int
start_wpa_supplicant(char *primary_iflist)
{
	uint32 flags = 0;
	char ifname[IFNAMSIZ] = {0}, prefix[IFNAMSIZ] = {0};
	char filename[HAPD_MAX_BUF] = {0}, cmd[HAPD_MAX_BUF * 2] = {0};
	char *next;
	char *lan_ifname = NULL;
	int ret = -1;

	if (!primary_iflist) {
		dprintf("Err: rc: %s empry primary ifaces list\n", __FUNCTION__);
		return -1;
	}
#ifdef HAPD_WPASUPP_DBG
	printf("primary_iflist: ");
	PRINT_IFLIST(primary_iflist);
#endif /* HAPD_WPASUPP_DBG */

	foreach(ifname, primary_iflist, next) {
		/* skip if interface is in AP mode */
		if (hapd_wpasupp_is_ifce_ap(ifname) == TRUE)  {
			dprintf("Err: rc: %s: %s is in AP mode. skip.\n", __FUNCTION__, ifname);
			continue;
		}

		if (osifname_to_nvifname(ifname, prefix, sizeof(prefix))) {
			dprintf("Err: rc: %s invalid interface name\n", __FUNCTION__);
			continue;
		}

		if (wpa_supp_get_config_filename(prefix, filename, sizeof(filename), &flags) < 0) {
			continue;
		}

		if (wpa_supp_create_config_file(prefix, filename, flags) < 0) {
			dprintf("Err: rc: %s wpa_supplicant conf file %s creation for %s failed\n",
					__FUNCTION__, filename, prefix);
			continue;
		}

		lan_ifname = nvram_safe_get("lan_ifname");
		if (lan_ifname[0] == '\0') {
			lan_ifname = "br0";
		}

		cprintf("Running wpa_supplicant instance on ifce %s using %s configuration\n",
				ifname, filename);
		snprintf(cmd, sizeof(cmd), "wpa_supplicant %s -i %s -Dnl80211 -c %s -b %s &",
				(nvram_match("wpasupp_dbg", "1") ? "-ddK" : ""),
				ifname, filename, lan_ifname);
		ret = system(cmd);
	} /* foreach */
end:

	return ret;
}

/* Stops the wpa_supplicant */
int
stop_wpa_supplicant()
{
	int ret = 0;
	char cmd[HAPD_MAX_BUF * 2] = {0};

	ret = eval("killall", "-9", "wpa_supplicant");

	snprintf(cmd, sizeof(cmd), "rm -f /tmp/wl*_wpa_supplicant.conf");
	system(cmd);

	memset(cmd, 0, sizeof(cmd));

	snprintf(cmd, sizeof(cmd), "rm -rf /var/run/wl*_wpa_supplicant");
	system(cmd);

	return ret;
}

static void
stop_wpa_supplicant_per_radio(int radio_idx)
{
	DIR *dir;
	struct dirent *ent;
	char path[PATH_MAX];
	FILE *fp = NULL;
	bool bFound = FALSE;
	char nv_name[HAPD_MAX_BUF] = {0};
	char *nvi_ifname = NULL, *pStrTmp = NULL;
	char cmd[HAPD_MAX_BUF * 2] = {0};
	int size = 0;

	snprintf(nv_name, sizeof(nv_name), "wl%d_ifname", radio_idx);
	nvi_ifname = nvram_get(nv_name);

	if ((dir = opendir("/proc")) == NULL) {
		dprintf("%s fail to open /proc\n", __FUNCTION__);
	}
	else {
		while ((ent = readdir(dir)) != NULL) {
			if (!strspn(ent->d_name, "0123456789"))
				continue;

			path[0] = '\0';
			strcat(path, "/proc/");
			strcat(path, ent->d_name);
			strcat(path, "/");
			strcat(path, "cmdline");
			fp = fopen(path, "r");
			if (!fp)
				continue;

			if (fgets(cmd, sizeof(cmd), fp) != NULL) {
			/*
			* hexdump -c /proc/4361/cmdline
			* 0000000   w   p   a   _   s   u   p   p   l   i   c   a   n   t  \0  /
			* 0000010   -   i  \0   w   l   0  \0
			*/
				if (!strstr(cmd, "wpa_supplicant")) {
					fclose(fp);
					continue;
				}

				size = ftell(fp);
				pStrTmp = cmd;

				while (pStrTmp < cmd + size) {
					if (!strcmp(pStrTmp, "-i")) {
						break;
					}
					else {
						pStrTmp += strlen(pStrTmp);
						++pStrTmp;
					}
				}

				if (pStrTmp < cmd + size) {
					pStrTmp += strlen(pStrTmp);
					++pStrTmp;

					/* pStrTmp now points to interface name */
					if (!strcmp(pStrTmp, nvi_ifname)) {
						bFound = TRUE;
						dprintf("%s kill wpa_supplicant instance",
							__FUNCTION__);
						dprintf(" %s for %s \n", ent->d_name, nvi_ifname);
						eval("kill", ent->d_name);
					}
				}
			}
			fclose(fp);

			if (bFound) {
				break;
			}
		}
		closedir(dir);
	}

	if (!bFound) {
		dprintf("Err: %s hostapd instance for %s does not exist\n",
				__FUNCTION__, nvi_ifname);
	}

	memset(cmd, 0, sizeof(cmd));
	snprintf(cmd, sizeof(cmd), "rm -f /tmp/%s_wpa_supplicant.conf", nvi_ifname);
	system(cmd);

	memset(cmd, 0, sizeof(cmd));
	snprintf(cmd, sizeof(cmd), "rm -rf /var/run/%s_wpa_supplicant", nvi_ifname);
	system(cmd);

	return;
}
/* Fn to fetch the list of primary and virtual interfaces for a radio */
static void
hapd_wpasupp_get_radio_list(char *ifnames_list, char *radio_iflist, int rlistsz, int idx)
{
	char ifname[IFNAMSIZ] = {0}, *next;
	char wlprefix[IFNAMSIZ] = {0}, nv_name[HAPD_MAX_BUF] = {0};
	int unit = -1;

	snprintf(nv_name, sizeof(nv_name),  "wl%d_radio", idx);

	if ((nvram_match(nv_name, "0")) == TRUE) {
		dprintf("radio %d disabled\n", idx);
		return;
	}

	foreach(ifname, ifnames_list, next) {
		if (wl_probe(ifname)) {
			dprintf("%s: Skipping %s - non wireless interface\n", __FUNCTION__, ifname);
			continue;
		}
		if (osifname_to_nvifname(ifname, wlprefix, IFNAMSIZ) < 0) {
			dprintf("%s, %d: Error\n", __FUNCTION__, __LINE__);
			return;
		}
		if (get_ifname_unit(wlprefix, &unit, NULL) < 0) {
			dprintf("%s, %d: Error\n", __FUNCTION__, __LINE__);
			return;
		}

		if (unit == idx) {
			add_to_list(ifname, radio_iflist, rlistsz);
		} else {
			dprintf("Skipped adding %s to radio_iflist[%d]\n", ifname, idx);
		}
	} /* foreach */

#ifdef HAPD_WPASUPP_DBG
	printf("radio_iflist[%d]: ", idx);
	PRINT_IFLIST(radio_iflist);
#endif /* HAPD_WPASUPP_DBG */

	return;
}

int
start_hapd_wpasupp_per_radio(int radio_idx)
{
	char *br0_ifnames_list, *br1_ifnames_list; /* interfaces on br0 and  br1 */
	char *ifnames_list;  /* concatenated ifnames on br0 and br1 with space in between */
	char *primary_iflist; /* primary radio interfaces on the board - ethX, ethY... */
	char *filtered_iflist; /* interfaces on which BSS' are enabled - ethX, wlX.Y... */
	char *virtual_iflist; /* virtual interfaces  - wlX.Y... */
	char *radio_iflist; /* primay and virtual ifces on a radio */
	int ifnames_listsz = 0; /* concatenated list size */
	int ret = -1;

	br0_ifnames_list = nvram_safe_get("lan_ifnames");
	br1_ifnames_list = nvram_safe_get("lan1_ifnames");

	if (br0_ifnames_list[0] == '\0' && br1_ifnames_list[0] == '\0') {
		cprintf("Err: rc: %s, %d:  No interfaces in LAN and GUEST bridges\n",
			__FUNCTION__, __LINE__);
		return -1;
	}

	/* concatenated list size */
	ifnames_listsz = (strlen(br0_ifnames_list) + strlen(br1_ifnames_list)
				+ 1 /* space */ + 1 /* '\0' */);

	ifnames_list = calloc(1, ifnames_listsz);
	if (!ifnames_list) {
		dprintf("Err: rc: %s,%d: calloc() error\n", __FUNCTION__, __LINE__);
		goto end;
	}
	filtered_iflist = calloc(1, ifnames_listsz);
	if (!filtered_iflist) {
		dprintf("Err: rc: %s,%d: calloc() error\n", __FUNCTION__, __LINE__);
		goto end;
	}
	primary_iflist = calloc(1, ifnames_listsz);
	if (!primary_iflist) {
		dprintf("Err: rc: %s,%d: calloc() error\n", __FUNCTION__, __LINE__);
		goto end;
	}
	virtual_iflist = calloc(1, ifnames_listsz);
	if (!virtual_iflist) {
		dprintf("Err: rc: %s,%d: calloc() error\n", __FUNCTION__, __LINE__);
		goto end;
	}

	radio_iflist = calloc(1, ifnames_listsz);
	if (!radio_iflist) {
		dprintf("Err: rc: %s,%d: calloc() error\n", __FUNCTION__, __LINE__);
		goto end;
	}

	/* ifnames_list = br0_ifnames_list + space */
	strcat_r(br0_ifnames_list, " ", ifnames_list);

	/* ifnames_list = br0_ifnames_list + space + br1_ifnames_list */
	strcat_r(ifnames_list, br1_ifnames_list, ifnames_list);

	hapd_wpasupp_get_radio_list(ifnames_list, radio_iflist, ifnames_listsz, radio_idx);

	/* Iterate over each radio. Fetch the radio's secondary BSS' */
	if (*radio_iflist == '\0') {
		dprintf("radio_iflist[%d] is empty. continue\n", radio_idx);
		goto end;
	}

	memset(filtered_iflist, 0, ifnames_listsz);
	memset(primary_iflist, 0, ifnames_listsz);
	memset(virtual_iflist, 0, ifnames_listsz);

	hapd_wpasupp_get_filtered_ifnames_list(radio_iflist,
		filtered_iflist, ifnames_listsz);
	hapd_wpasupp_get_primary_virtual_iflist(filtered_iflist, primary_iflist,
		virtual_iflist, ifnames_listsz);

#ifdef HAPD_WPASUPP_DBG
	printf("radio_iflist: ", radio_idx);
	PRINT_IFLIST(radio_iflist);

	printf("filtered_iflist: ");
	PRINT_IFLIST(filtered_iflist);

	printf("primary_iflist: ");
	PRINT_IFLIST(primary_iflist);

	printf("virtual_iflist: ");
	PRINT_IFLIST(virtual_iflist);
#endif /* HAPD_WPASUPP_DBG */

	ret = start_wpa_supplicant(primary_iflist);
	if (ret == 0) {
		/* if wpa_supp successfully launched, wait before starting hostapd */
		sleep(HAPD_START_DEFER);
	}

	ret = start_hostapd(filtered_iflist, primary_iflist, virtual_iflist);

end:
	if (ifnames_list) {
		free(ifnames_list);
	}
	if (filtered_iflist) {
		free(filtered_iflist);
	}
	if (primary_iflist) {
		free(primary_iflist);
	}
	if (virtual_iflist) {
		free(virtual_iflist);
	}

	if (radio_iflist) {
		free(radio_iflist);
	}

	return ret;
}

int
start_hapd_wpasupp()
{
	char *br0_ifnames_list, *br1_ifnames_list; /* interfaces on br0 and  br1 */
	char *ifnames_list;  /* concatenated ifnames on br0 and br1 with space in between */
	char *primary_iflist; /* primary radio interfaces on the board - ethX, ethY... */
	char *filtered_iflist; /* interfaces on which BSS' are enabled - ethX, wlX.Y... */
	char *virtual_iflist; /* virtual interfaces  - wlX.Y... */
	char *radio_iflist[MAX_RADIO_NUM]; /* primay and virtual ifces on a radio */
	int ifnames_listsz = 0; /* concatenated list size */
	int idx, ret = -1;

	br0_ifnames_list = nvram_safe_get("lan_ifnames");
	br1_ifnames_list = nvram_safe_get("lan1_ifnames");

	if (br0_ifnames_list == '\0' && br1_ifnames_list == '\0') {
		cprintf("Err: rc: %s, %d:  No interfaces in LAN and GUEST bridges\n",
			__FUNCTION__, __LINE__);
		return -1;
	}

	/* concatenated list size */
	ifnames_listsz = (strlen(br0_ifnames_list) + strlen(br1_ifnames_list)
				+ 1 /* space */ + 1 /* '\0' */);

	ifnames_list = calloc(1, ifnames_listsz);
	if (!ifnames_list) {
		dprintf("Err: rc: %s,%d: calloc() error\n", __FUNCTION__, __LINE__);
		goto end;
	}
	filtered_iflist = calloc(1, ifnames_listsz);
	if (!filtered_iflist) {
		dprintf("Err: rc: %s,%d: calloc() error\n", __FUNCTION__, __LINE__);
		goto end;
	}
	primary_iflist = calloc(1, ifnames_listsz);
	if (!primary_iflist) {
		dprintf("Err: rc: %s,%d: calloc() error\n", __FUNCTION__, __LINE__);
		goto end;
	}
	virtual_iflist = calloc(1, ifnames_listsz);
	if (!virtual_iflist) {
		dprintf("Err: rc: %s,%d: calloc() error\n", __FUNCTION__, __LINE__);
		goto end;
	}

	for (idx = 0; idx < MAX_RADIO_NUM; idx++) {
		radio_iflist[idx] = calloc(1, ifnames_listsz);

		if (!radio_iflist[idx]) {
			dprintf("Err: rc: %s,%d: calloc() error\n", __FUNCTION__, __LINE__);
			goto end;
		}
	}

	/* ifnames_list = br0_ifnames_list + space */
	strcat_r(br0_ifnames_list, " ", ifnames_list);

	/* ifnames_list = br0_ifnames_list + space + br1_ifnames_list */
	strcat_r(ifnames_list, br1_ifnames_list, ifnames_list);

	for (idx = 0; idx < MAX_RADIO_NUM; idx++) {
		hapd_wpasupp_get_radio_list(ifnames_list, radio_iflist[idx], ifnames_listsz, idx);
	}

	/* Iterate over each radio. Fetch the radio's secondary BSS' */
	for (idx = 0; idx < MAX_RADIO_NUM; idx++) {
		memset(filtered_iflist, 0, ifnames_listsz);
		memset(primary_iflist, 0, ifnames_listsz);
		memset(virtual_iflist, 0, ifnames_listsz);

		if (*radio_iflist[idx] == '\0') {
			dprintf("radio_iflist[%d] is empty. continue\n", idx);
			continue;
		}

		hapd_wpasupp_get_filtered_ifnames_list(radio_iflist[idx],
			filtered_iflist, ifnames_listsz);
		hapd_wpasupp_get_primary_virtual_iflist(filtered_iflist, primary_iflist,
			virtual_iflist, ifnames_listsz);

#ifdef HAPD_WPASUPP_DBG
		printf("radio_iflist[%d]: ", idx);
		PRINT_IFLIST(radio_iflist[idx]);

		printf("filtered_iflist: ");
		PRINT_IFLIST(filtered_iflist);

		printf("primary_iflist: ");
		PRINT_IFLIST(primary_iflist);

		printf("virtual_iflist: ");
		PRINT_IFLIST(virtual_iflist);
#endif /* HAPD_WPASUPP_DBG */

		ret = start_wpa_supplicant(primary_iflist);
		if (ret == 0) {
			/* if wpa_supp successfully launched, wait before starting hostapd */
			sleep(HAPD_START_DEFER);
		}

		ret = start_hostapd(filtered_iflist, primary_iflist, virtual_iflist);

		sleep(2);

	} /* for each radio */

end:
	if (ifnames_list) {
		free(ifnames_list);
	}
	if (filtered_iflist) {
		free(filtered_iflist);
	}
	if (primary_iflist) {
		free(primary_iflist);
	}
	if (virtual_iflist) {
		free(virtual_iflist);
	}

	for (idx = 0; idx < MAX_RADIO_NUM; idx++) {
		if (radio_iflist[idx]) {
			free(radio_iflist[idx]);
		}
	}

	return ret;
}

void
stop_hapd_wpasupp()
{
	stop_hostapd();
	stop_wpa_supplicant();
}

void
stop_hapd_wpasupp_per_radio(int radio_idx)
{
	stop_hostapd_per_radio(radio_idx);
	stop_wpa_supplicant_per_radio(radio_idx);
}
#endif	/* CONFIG_HOSTAPD */
