/*
 * iwinfo - Wireless Information Library - Linux Wireless Extension Backend
 *
 *   Copyright (C) 2009 Jo-Philipp Wich <xm@subsignal.org>
 *
 * The iwinfo library is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
 * The iwinfo library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with the iwinfo library. If not, see http://www.gnu.org/licenses/.
 *
 * Parts of this code are derived from the Linux wireless tools, iwlib.c,
 * iwlist.c and iwconfig.c in particular.
 */

#include "iwinfo/mtk.h"
#include "iwinfo/wext.h"

struct survey_table {
	long channel;
	long strength;
	char mode[12];
	char ssid[66];
	char bssid[18];
	char enc[16];
	char crypto[16];
};

char* wifidev_to_wifiname[2] = {
	"ra0",
	"rai0"
};

CH_FREQ_MAP CH_HZ_OF_EU_5G[] = 
{
	{36, 5180},
	{40, 5200},
	{44, 5220},
	{48, 5240},
};
CH_FREQ_MAP CH_HZ_OF_EU_2G[] = 
{
	{1, 2412},
	{2, 2417},
	{3, 2422},
	{4, 2427},
	{5, 2432},
	{6, 2437},
	{7, 2442},
	{8, 2447},
	{9, 2452},
	{10, 2457},
	{11, 2462},
	{12, 2467},
	{13, 2472},
};


const char* _devname_to_ifname(const char *ifname)
{
	if (!strncmp(ifname, "wifi0", 5))
	{
		//printf("call with ifname %s\n", wifidev_to_wifiname[0]);
		return wifidev_to_wifiname[0];
	}
	else if (!strncmp(ifname, "wifi1", 5))
	{
		//printf("call with ifname %s\n", wifidev_to_wifiname[1]);
		return wifidev_to_wifiname[1];
	}
	else
	{
		//printf("call with ifname %s\n", ifname);
		return ifname;
	}
}


static int mtk_wrq(struct iwreq *wrq, const char *ifname, int cmd, void *data, size_t len)
{
	strncpy(wrq->ifr_name, ifname, IFNAMSIZ);

	if( data != NULL )
	{
		if( len < IFNAMSIZ )
		{
			memcpy(wrq->u.name, data, len);
		}
		else
		{
			wrq->u.data.pointer = data;
			wrq->u.data.length = len;
		}
	}

	return iwinfo_ioctl(cmd, wrq);
}

static int mtk_get80211priv(const char *ifname, int op, void *data, size_t len)
{
	struct iwreq iwr;

	if( mtk_wrq(&iwr, ifname, op, data, len) < 0 )
		return -1;

	return iwr.u.data.length;
}

static int mtk_isap(const char *ifname)
{
	int ret=0;

	if( strlen(ifname) <= 7 )
	{
		static char wifiname[IFNAMSIZ];
		snprintf(wifiname, sizeof(wifiname), "%s", ifname);
		if( !strncmp(wifiname, "ra", 2) || !strncmp(wifiname, "rai", 3) || !strncmp(wifiname, "wifi", 4) ) ret=1;
	}

	return ret;
}

static int mtk_iscli(const char *ifname)
{
	int ret=0;
	char cmd[256];
	char buf[1024];
	FILE *fp = NULL;
	char *pstr;
	memset(cmd, 0, sizeof(cmd));
	
	if( strlen(ifname) <= 7 )
	{
		static char wifiname[IFNAMSIZ];
		snprintf(wifiname, sizeof(wifiname), "%s", ifname);
		if( !strncmp(wifiname, "apcli", 5) || !strncmp(wifiname, "apclii", 6) ) 
		{	
			sprintf(cmd, "ifconfig %s | grep UP", _devname_to_ifname(ifname));
			fp = popen(cmd, "r");
			fscanf(fp, "%s\n", buf);
			pclose(fp);
			pstr = strstr(buf, "UP");
			ret = pstr? 1 : 0;
		}
	}

	return ret;
}

int mtk_probe(const char *ifname)
{
	return ( mtk_isap(ifname) || mtk_iscli(ifname) );
}

void mtk_close(void)
{
	/* Nop */
}

char *GetAuthModeStr(UINT32 authMode)
{
	if (IS_AKM_OPEN(authMode))
		return "OPEN";
	else if (IS_AKM_SHARED(authMode))
		return "SHARED";
	else if (IS_AKM_AUTOSWITCH(authMode))
		return "WEPAUTO";
	else if (IS_AKM_WPANONE(authMode))
		return "WPANONE";
	else if (IS_AKM_WPA1(authMode) && IS_AKM_WPA2(authMode))
		return "WPA1WPA2";
	else if (IS_AKM_WPA1PSK(authMode) && IS_AKM_WPA2PSK(authMode))
		return "WPAPSKWPA2PSK";
#if 0
	else if (IS_AKM_WPA2PSK(authMode) && IS_AKM_WPA3PSK(authMode))
		return "WPA2PSKWPA3PSK";
	else if (IS_AKM_WPA3PSK(authMode))
		return "WPA3PSK";
#endif
	else if (IS_AKM_WPA1(authMode))
		return "WPA";
	else if (IS_AKM_WPA1PSK(authMode))
		return "WPAPSK";
	else if (IS_AKM_FT_WPA2(authMode))
		return "FT-WPA2";
	else if (IS_AKM_FT_WPA2PSK(authMode))
		return "FT-WPA2PSK";
	else if (IS_AKM_WPA3(authMode)) /* WPA3 will be always accompanied by WPA2, so it should put before the WPA2 */
		return "WPA3";
	else if (IS_AKM_WPA2(authMode))
		return "WPA2";
	else if (IS_AKM_WPA2PSK(authMode))
		return "WPA2PSK";
#if 0
	else if (IS_AKM_WPA3_192BIT(authMode))
		return "WPA3-192";
#endif
	else if (IS_AKM_OWE(authMode))
		return "OWE";
	else
		return "UNKNOW";
}

char *GetEncryModeStr(UINT32 encryMode)
{
	if (IS_CIPHER_NONE(encryMode))
		return "NONE";
	else if (IS_CIPHER_WEP(encryMode))
		return "WEP";
	else if (IS_CIPHER_TKIP(encryMode) && IS_CIPHER_CCMP128(encryMode))
		return "TKIPAES";
	else if (IS_CIPHER_TKIP(encryMode))
		return "TKIP";
	else if (IS_CIPHER_CCMP128(encryMode))
		return "AES";
	else if (IS_CIPHER_CCMP256(encryMode))
		return "CCMP256";
	else if (IS_CIPHER_GCMP128(encryMode))
		return "GCMP128";
	else if (IS_CIPHER_GCMP256(encryMode))
		return "GCMP256";
	else if (IS_CIPHER_BIP_CMAC128(encryMode))
		return "BIP-CMAC128";
	else if (IS_CIPHER_BIP_CMAC256(encryMode))
		return "BIP-CMAC256";
	else if (IS_CIPHER_BIP_GMAC128(encryMode))
		return "BIP-GMAC128";
	else if (IS_CIPHER_BIP_GMAC256(encryMode))
		return "BIP-GMAC256";
	else
		return "UNKNOW";
}



int mtk_get_mode(const char *ifname, int *buf)
{
	if( mtk_isap(_devname_to_ifname(ifname)) ) *buf = IWINFO_OPMODE_MASTER;
	else if( mtk_iscli(_devname_to_ifname(ifname)) ) *buf = IWINFO_OPMODE_CLIENT;
	else *buf = IWINFO_OPMODE_UNKNOWN;
	return 0;
}

int mtk_get_vapinfo(const char *ifname, WLAN_BSS_INFO *bss)
{
	if (mtk_get80211priv(ifname, RTPRIV_IOCTL_GBSSINFO, bss, sizeof(WLAN_BSS_INFO)) < 0)
	{
		/*can not get bss info when interface is down, so give defalut value*/
		printf("error: cannot get bssinfo from driver\n");

		bss->securityEnable = 0;
		if (!strncmp(ifname, "rai", 3))
		{
			bss->channel = 36;
			bss->phymode = 14;
		}
		else
		{
			bss->channel = 1;
			bss->phymode = 9;
		}
		bss->rssi = -1;
		bss->authMode = SEC_AKM_OPEN;
		bss->encrypType = SEC_CIPHER_NONE;
		return -1;	
	}
	return 0;
}

int mtk_get_ssid(const char *ifname, char *buf)
{
	return wext_ops.ssid(_devname_to_ifname(ifname), buf);
}

int mtk_get_bssid(const char *ifname, char *buf)
{
	char cmd[256];
	FILE *fp = NULL;

	memset(cmd, 0, sizeof(cmd));
	sprintf(cmd, "ifconfig %s | grep UP", _devname_to_ifname(ifname));
	fp = popen(cmd, "r");
	fscanf(fp, "%s\n", buf);
	pclose(fp);

	return wext_ops.bssid(ifname, buf);
}

int mtk_get_bitrate(const char *ifname, int *buf)
{
	return wext_ops.bitrate(ifname, buf);
}

int mtk_get_channel(const char *ifname, int *buf)
{
	WLAN_BSS_INFO bss;
	memset(&bss, 0, sizeof(WLAN_BSS_INFO));
	mtk_get_vapinfo(_devname_to_ifname(ifname), &bss);
	*buf = bss.channel;
	return 0;
	/*
	return wext_ops.channel(_devname_to_ifname(ifname), buf);
	*/
}

int mtk_get_frequency(const char *ifname, int *buf)
{
	return wext_ops.frequency(_devname_to_ifname(ifname), buf);
}

int mtk_get_txpower(const char *ifname, int *buf)
{
	return -1;
}

int mtk_get_signal(const char *ifname, int *buf)
{
	return -1;
}

int mtk_get_noise(const char *ifname, int *buf)
{
	return -1;
}

int mtk_get_quality(const char *ifname, int *buf)
{
	return -1;
}

int mtk_get_quality_max(const char *ifname, int *buf)
{
	return -1;
}

static int mtk_get_rate(MACHTTRANSMIT_SETTING HTSetting)

{
	int MCSMappingRateTable[] =
	{2,  4,   11,  22, /* CCK*/
	12, 18,   24,  36, 48, 72, 96, 108, /* OFDM*/
	13, 26,   39,  52,  78, 104, 117, 130, 26,  52,  78, 104, 156, 208, 234, 260, /* 20MHz, 800ns GI, MCS: 0 ~ 15*/
	39, 78,  117, 156, 234, 312, 351, 390,										  /* 20MHz, 800ns GI, MCS: 16 ~ 23*/
	27, 54,   81, 108, 162, 216, 243, 270, 54, 108, 162, 216, 324, 432, 486, 540, /* 40MHz, 800ns GI, MCS: 0 ~ 15*/
	81, 162, 243, 324, 486, 648, 729, 810,										  /* 40MHz, 800ns GI, MCS: 16 ~ 23*/
	14, 29,   43,  57,  87, 115, 130, 144, 29, 59,   87, 115, 173, 230, 260, 288, /* 20MHz, 400ns GI, MCS: 0 ~ 15*/
	43, 87,  130, 173, 260, 317, 390, 433,										  /* 20MHz, 400ns GI, MCS: 16 ~ 23*/
	30, 60,   90, 120, 180, 240, 270, 300, 60, 120, 180, 240, 360, 480, 540, 600, /* 40MHz, 400ns GI, MCS: 0 ~ 15*/
	90, 180, 270, 360, 540, 720, 810, 900};

	int rate_count = sizeof(MCSMappingRateTable)/sizeof(int);
	int rate_index = 0;
	int value = 0;

	if (HTSetting.field.MODE >= MODE_HTMIX)
	{
    		rate_index = 12 + ((UCHAR)HTSetting.field.BW *24) + ((UCHAR)HTSetting.field.ShortGI *48) + ((UCHAR)HTSetting.field.MCS);
	}
	else if (HTSetting.field.MODE == MODE_OFDM)
		rate_index = (UCHAR)(HTSetting.field.MCS) + 4;
	else if (HTSetting.field.MODE == MODE_CCK)   
		rate_index = (UCHAR)(HTSetting.field.MCS);

	if (rate_index < 0)
		rate_index = 0;
    
	if (rate_index >= rate_count)
		rate_index = rate_count-1;

	value = (MCSMappingRateTable[rate_index] * 5)/10;

	return value;
}

int mtk_get_assoclist(const char *ifname, char *buf, int *len)
{
	struct iwinfo_assoclist_entry entry;
	WLAN_STA_INFO_TABLE staInfoTab;
	WLAN_STA_INFO *pStaInfo;
	int i, j;

	if (mtk_get80211priv(_devname_to_ifname(ifname), RTPRIV_IOCTL_GSTAINFO, &staInfoTab, sizeof(WLAN_STA_INFO_TABLE)) > 0)
	{
		j = 0;
		for (i = 0; i < staInfoTab.Num; i++)
		{
			memset(&entry, 0, sizeof(entry));
			memcpy(entry.mac, &staInfoTab.Entry[i].addr, 6);
			if(staInfoTab.Entry[i].avgRssi0 > staInfoTab.Entry[i].avgRssi1)
				entry.signal = staInfoTab.Entry[i].avgRssi0;
			else
				entry.signal = staInfoTab.Entry[i].avgRssi1;

			entry.noise  = -95;
			entry.inactive =staInfoTab.Entry[i].connectedTime * 1000;
			
			entry.tx_packets = staInfoTab.Entry[i].txPackets;
			entry.rx_packets = staInfoTab.Entry[i].rxPackets;

			entry.tx_rate.rate = staInfoTab.Entry[i].lastTxRate * 1000;
			entry.rx_rate.rate = staInfoTab.Entry[i].lastRxRate * 1000;	

			/*no need to set this filed*/
			entry.tx_rate.mcs = 0;
			entry.tx_rate.is_40mhz = 1;
			entry.tx_rate.is_short_gi = 1;
			
			entry.rx_rate.mcs = 0;
			entry.rx_rate.is_40mhz = 1;
			entry.rx_rate.is_short_gi = 1;
			
			memcpy(&buf[j], &entry, sizeof(struct iwinfo_assoclist_entry));
			j += sizeof(struct iwinfo_assoclist_entry);
		}
		*len = j;
		return 0;
	}
	
	return -1;
}


#if 0
int mtk_get_assoclist(const char *ifname, char *buf, int *len)
{
	struct iwinfo_assoclist_entry entry;
	RT_802_11_MAC_TABLE *mt;
	MACHTTRANSMIT_SETTING rxrate;
	char raname[IFNAMSIZ],raidx[IFNAMSIZ],raiidx[IFNAMSIZ];
	int mtlen=sizeof(RT_802_11_MAC_TABLE);
	int i,j;

	if ((mt = (RT_802_11_MAC_TABLE *) malloc(mtlen)) == NULL)
	{
		return -1;
	}

	memset(mt, 0, mtlen);
	snprintf(raname, sizeof(raname), "%s", _devname_to_ifname(ifname));

	if (mtk_get80211priv(_devname_to_ifname(ifname), RTPRIV_IOCTL_GET_MAC_TABLE_STRUCT, mt, mtlen) > 0)
	{
		j = 0;

		for (i = 0; i < mt->Num && i < MAX_NUMBER_OF_MAC; i++)
		{
			snprintf(raidx, sizeof(raidx), "ra%d", mt->Entry[i].ApIdx);
			snprintf(raiidx, sizeof(raiidx), "rai%d", mt->Entry[i].ApIdx);

			if( strncmp(raname, raidx, IFNAMSIZ) && strncmp(raname, raiidx, IFNAMSIZ) ) continue;

			memset(&entry, 0, sizeof(entry));

			memcpy(entry.mac, &mt->Entry[i].Addr, 6);

			if(mt->Entry[i].AvgRssi0 > mt->Entry[i].AvgRssi1)
				entry.signal = mt->Entry[i].AvgRssi0;
			else
				entry.signal = mt->Entry[i].AvgRssi1;

			entry.noise  = -95;

			entry.inactive = mt->Entry[i].ConnectedTime * 1000;

			rxrate.word = mt->Entry[i].LastRxRate;
			entry.tx_rate.rate = mtk_get_rate(mt->Entry[i].TxRate) * 1000;
			entry.rx_rate.rate = mtk_get_rate(rxrate) * 1000;

			entry.tx_rate.mcs = mt->Entry[i].TxRate.field.MCS;
			entry.rx_rate.mcs = rxrate.field.MCS;

			entry.tx_packets = 0;
			entry.rx_packets = 0;

			if(mt->Entry[i].TxRate.field.BW) entry.tx_rate.is_40mhz = 1;
			if(mt->Entry[i].TxRate.field.ShortGI) entry.tx_rate.is_short_gi = 1;
			if(rxrate.field.BW) entry.rx_rate.is_40mhz = 1;
			if(rxrate.field.ShortGI) entry.rx_rate.is_short_gi = 1;

			memcpy(&buf[j], &entry, sizeof(struct iwinfo_assoclist_entry));
			j += sizeof(struct iwinfo_assoclist_entry);
		}
		*len = j;
		free(mt);
		return 0;
	}

	free(mt);
	return -1;
}
#endif 

int mtk_get_txpwrlist(const char *ifname, char *buf, int *len)
{
	return -1;
}

static int ascii2num(char ascii)
{
	int num;
	if ((ascii >= '0') && (ascii <= '9'))
		num=ascii - 48;
	else if ((ascii >= 'a') && (ascii <= 'f'))
		num=ascii - 'a' + 10;
        else if ((ascii >= 'A') && (ascii <= 'F'))
		num=ascii - 'A' + 10;
	else
		num = 0;
	return num;
}

static void next_field(char **line, char *output, int n, int m) {
	while (**line == ' ') (*line)++;
	char *sep = strchr(*line, ' ');

	while (m-- >0) {
		if (*(*line+m) != ' ') {
			sep= *line+m+1;
			break;
		}
	}

	if (sep) {
		*sep = '\0';
		sep++;
	}

	strncpy(output, *line, n);

	/* Update the line token for the next call */
	*line = sep;
}

static int mtk_get_scan(const char *ifname, struct survey_table *st)
{
	int survey_count = 0;
	char ss[MAX_NUM_OF_SURVEY_CNT] = "SiteSurvey=1";
	char *p;
	SITE_SURVEY_LIST SurveyList[MAX_NUM_OF_SURVEY_CNT];    /* Modified by JiangZheyu 19/7/31: Change form of scan results into struct */
	                                                       /* To fix 'spaces in SSID' BUGs, and more clear at the same time. */

	if( mtk_get80211priv(ifname, RTPRIV_IOCTL_SET, ss, sizeof(ss)) < 0 )
		return -1;

	sleep(1);
	
	if( mtk_get80211priv(ifname, RTPRIV_IOCTL_GSITESURVEY, SurveyList, MAX_NUM_OF_SURVEY_CNT * sizeof(SITE_SURVEY_LIST)) < 1 )
		return -1;

	while((survey_count < MAX_NUM_OF_SURVEY_CNT))
	{
		char tmp[MAX_LEN_OF_SECURITY];

		if(!SurveyList[survey_count].Channel)
			break;

		/* channel */
		st[survey_count].channel = SurveyList[survey_count].Channel;

		/* ssid */
		strncpy(st[survey_count].ssid, SurveyList[survey_count].SSID, MAX_LEN_OF_SSID);

		/* bssid */
		strncpy(st[survey_count].bssid, SurveyList[survey_count].BSSID, MAX_LEN_OF_MAC_STR);

		/* crypto&enc */
		strncpy(tmp, SurveyList[survey_count].Security, MAX_LEN_OF_SECURITY);
		p = strchr(tmp, '/');
		if(p != NULL) {
			*p = '\0';
			strncpy(st[survey_count].crypto, p+1, sizeof(st->crypto));
		}
		strncpy(st[survey_count].enc, tmp, sizeof(st->enc));

		/* strength */
		st[survey_count].strength = SurveyList[survey_count].RssiQuality;

		/* wireless mode */
		strncpy(st[survey_count].mode, SurveyList[survey_count].WirelessMode, MAX_LEN_OF_WIRELESS_MODE);

		survey_count++;
	}
	return survey_count;
}

int mtk_get_scanlist(const char *ifname, char *buf, int *len)
{
	int sc,i,j=0,h;
	char *p;
	struct survey_table stl[MAX_NUM_OF_SURVEY_CNT];
	struct iwinfo_scanlist_entry sce;

	sc = mtk_get_scan(_devname_to_ifname(ifname), stl);
	if ( sc < 1)
		return -1;
	
	for (i = 0; i < sc; i++)
	{
		memset(&sce, 0, sizeof(sce));

		for (h = 0; h < 6; h++)
			sce.mac[h] = (uint8_t)(ascii2num(stl[i].bssid[h*3]) * 16 + ascii2num(stl[i].bssid[h*3+1]));

		memcpy(sce.ssid, stl[i].ssid, sizeof(sce.ssid));
		sce.ssid[32] = '\0';
		sce.channel=(uint8_t) stl[i].channel;
		sce.quality = (uint8_t) stl[i].strength;
		sce.quality_max = 100;
		sce.signal = 155 + sce.quality;
		sce.mode=1;

		if (strcmp(stl[i].enc,"OPEN"))
		{
			sce.crypto.enabled=1;
			if (!strcmp(stl[i].enc,"WPAPSKWPA2PSK"))
			{
				sce.crypto.wpa_version = 3;
				sce.crypto.auth_suites = IWINFO_KMGMT_PSK;
			}
			else if (!strcmp(stl[i].enc,"WPA2PSK"))
			{
				sce.crypto.wpa_version = 2;
				sce.crypto.auth_suites = IWINFO_KMGMT_PSK;
			}
			else if (!strcmp(stl[i].enc,"WPAPSK"))
			{
				sce.crypto.wpa_version = 1;
				sce.crypto.auth_suites = IWINFO_KMGMT_PSK;
			}

			if (!strcmp(stl[i].crypto,"AES"))
				sce.crypto.pair_ciphers = IWINFO_CIPHER_CCMP;
			else if (!strcmp(stl[i].crypto,"TKIP"))
				sce.crypto.pair_ciphers = IWINFO_CIPHER_TKIP;
		}
		else
		{
			if (!strcmp(stl[i].crypto,"WEP"))
			{
				sce.crypto.enabled=1;
				sce.crypto.wpa_version = 0;
				sce.crypto.auth_algs = (IWINFO_AUTH_OPEN | IWINFO_AUTH_SHARED);
				sce.crypto.pair_ciphers = (IWINFO_CIPHER_WEP104 | IWINFO_CIPHER_WEP40);
				sce.crypto.auth_suites = IWINFO_KMGMT_NONE;
			}
			else
				sce.crypto.enabled=0;
		}
		
		memcpy(&buf[j], &sce, sizeof(struct iwinfo_scanlist_entry));
		j += sizeof(struct iwinfo_scanlist_entry);
	}
	
	*len = j;
	return 0;
}

int mtk_get_freqlist(const char *ifname, char *buf, int *len)
{
	struct MTK_CHANINFO chans;
	struct iwinfo_freqlist_entry entry;
	int i, bl = 0;
	int chan_num;
	CH_FREQ_MAP *pChHzMap = NULL;
	if (mtk_get80211priv(_devname_to_ifname(ifname), RTPRIV_IOCTL_GCHANLIST, &chans, sizeof(struct MTK_CHANINFO)) < 0)
	{
		printf("fail to get chanlist from driver, use default\n");
		if (!strncmp(ifname, "rai", 3))
		{
			pChHzMap = &CH_HZ_OF_EU_5G;
			chan_num = sizeof(CH_HZ_OF_EU_5G) / sizeof(CH_FREQ_MAP);
		}	
		else
		{
			pChHzMap = &CH_HZ_OF_EU_2G;
			chan_num = sizeof(CH_HZ_OF_EU_2G) / sizeof(CH_FREQ_MAP);
		}
		
		
		printf("tqj-->chan num is %d\n", chan_num);
		for (i = 0; i < chan_num; i++)
		{
			entry.mhz = (pChHzMap + i)->freqMHz;
			entry.channel = (pChHzMap + i)->channel;
			entry.restricted = entry.channel <= CH_MAX_2G_CHANNEL ? 0x80 : 0;
			memcpy(&buf[bl], &entry, sizeof(struct iwinfo_freqlist_entry));
			bl += sizeof(struct iwinfo_freqlist_entry);
		}
		
		*len = bl;

		return 0;
	}
	for (i = 0; i < chans.mtk_nchans; i++)
	{	
		entry.mhz        = chans.mtk_chans[i].ic_freq / 1000;
		entry.channel    = chans.mtk_chans[i].ic_ieee;
		entry.restricted = entry.channel <= CH_MAX_2G_CHANNEL ? 0x80 : 0;

		memcpy(&buf[bl], &entry, sizeof(struct iwinfo_freqlist_entry));
		bl += sizeof(struct iwinfo_freqlist_entry);
	}
	*len = bl;

	return 0;
}

int mtk_get_country(const char *ifname, char *buf)
{
	sprintf(buf, "00");
	return 0;
}

int mtk_get_countrylist(const char *ifname, char *buf, int *len)
{
	/* Stub */
	return -1;
}

int mtk_get_hwmodelist(const char *ifname, int *buf)
{

	WLAN_BSS_INFO *bss;
	if ((bss = (WLAN_BSS_INFO *) malloc(sizeof(WLAN_BSS_INFO))) == NULL)
	{
		return -1;
	}
	memset(bss, 0, sizeof(WLAN_BSS_INFO));
	mtk_get_vapinfo(_devname_to_ifname(ifname), bss);
	if (bss->phymode < 0 || bss->phymode >= PHY_MODE_MAX)
	{
		free(bss);
		return -1;
	}
	*buf = CFG_WMODE_MAP[2*(bss->phymode) + 1];
	free(bss);
	return 0;
}

int mtk_get_encryption(const char *ifname, char *buf)
{
	struct iwinfo_crypto_entry *crypto = (struct iwinfo_crypto_entry *)buf;
	WLAN_BSS_INFO bss;
	memset(&bss, 0, sizeof(WLAN_BSS_INFO));
	mtk_get_vapinfo(_devname_to_ifname(ifname), &bss);
/* to do: need to figure out the detail encrypt*/
	if (bss.securityEnable)
	{
		crypto->enabled=1;
	}
	else
	{
		crypto->enabled=0;
	}
	
	return 0;
}

int mtk_get_phyname(const char *ifname, char *buf)
{
	/* No suitable api in mtk */
	strcpy(buf, ifname);
	return 0;
}

int mtk_get_mbssid_support(const char *ifname, int *buf)
{
	return -1;
}

int mtk_get_hardware_id(const char *ifname, char *buf)
{
	return -1;
}

int mtk_get_hardware_name(const char *ifname, char *buf)
{
	sprintf(buf, "Generic Mediatek/Ralink");
	return 0;
}

int mtk_get_txpower_offset(const char *ifname, int *buf)
{
	/* Stub */
	*buf = 0;
	return -1;
}

int mtk_get_frequency_offset(const char *ifname, int *buf)
{
	/* Stub */
	*buf = 0;
	return -1;
}


