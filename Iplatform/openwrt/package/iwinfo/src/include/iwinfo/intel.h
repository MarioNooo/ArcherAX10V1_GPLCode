#ifndef __IWINFO_INTEL_H_
#define __IWINFO_INTE_H_

#include <fcntl.h>

#include "iwinfo.h"
#include "iwinfo/utils.h"
#include "iwinfo/api/intel.h"

int intel_probe(const char *ifname);
int intel_get_mode(const char *ifname, int *buf);
int intel_get_ssid(const char *ifname, char *buf);
int intel_get_bssid(const char *ifname, char *buf);
int intel_get_country(const char *ifname, char *buf);
int intel_get_channel(const char *ifname, int *buf);
int intel_get_frequency(const char *ifname, int *buf);
int intel_get_frequency_offset(const char *ifname, int *buf);
int intel_get_txpower(const char *ifname, int *buf);
int intel_get_txpower_offset(const char *ifname, int *buf);
int intel_get_bitrate(const char *ifname, int *buf);
int intel_get_signal(const char *ifname, int *buf);
int intel_get_noise(const char *ifname, int *buf);
int intel_get_quality(const char *ifname, int *buf);
int intel_get_quality_max(const char *ifname, int *buf);
int intel_get_encryption(const char *ifname, char *buf);
int intel_get_assoclist(const char *ifname, char *buf, int *len);
int intel_get_txpwrlist(const char *ifname, char *buf, int *len);
int intel_get_scanlist(const char *ifname, char *buf, int *len);
int intel_get_freqlist(const char *ifname, char *buf, int *len);
int intel_get_countrylist(const char *ifname, char *buf, int *len);
int intel_get_hwmodelist(const char *ifname, int *buf);
int intel_get_mbssid_support(const char *ifname, int *buf);
int intel_get_hardware_id(const char *ifname, char *buf);
int intel_get_hardware_name(const char *ifname, char *buf);
int intel_get_beacon_int(const char *ifname, int *buf);
void intel_close(void);

static const struct iwinfo_ops intel_ops = {
	//.name             = "intel",
	//.probe            = intel_probe,
	.mbssid_support   = intel_get_mbssid_support,
	//.htmodelist       = intel_get_htmodelist,
	//.phyname          = intel_get_phyname,

	.channel          = intel_get_channel,
	.frequency        = intel_get_frequency,
	.frequency_offset = intel_get_frequency_offset,
	.txpower          = intel_get_txpower,
	.txpower_offset   = intel_get_txpower_offset,
	.bitrate          = intel_get_bitrate,
	.signal           = intel_get_signal,
	.noise            = intel_get_noise,
	.quality          = intel_get_quality,
	.quality_max      = intel_get_quality_max,
	.hwmodelist       = intel_get_hwmodelist,
	.mode             = intel_get_mode,
	.ssid             = intel_get_ssid,
	.bssid            = intel_get_bssid,
	.country          = intel_get_country, //c
	.hardware_id      = intel_get_hardware_id,
	.hardware_name    = intel_get_hardware_name,
	.encryption       = intel_get_encryption,
	.assoclist        = intel_get_assoclist, //a
	.txpwrlist        = intel_get_txpwrlist,
	.scanlist         = intel_get_scanlist,
	.freqlist         = intel_get_freqlist, //f
	.countrylist      = intel_get_countrylist,
	.beacon_int       = intel_get_beacon_int,

	.close            = intel_close,
};

#endif // __IWINFO_INTE_H_
