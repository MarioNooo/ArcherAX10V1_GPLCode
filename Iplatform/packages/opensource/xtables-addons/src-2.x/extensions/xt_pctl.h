/*!Copyright(c) 2013-2014 Shenzhen TP-LINK Technologies Co.Ltd.
 *
 *\file     xt_pctl.h
 *\brief    kernel/netfilter part for http host filter. 
 *
 *\author   Hu Luyao
 *\version  1.0.0
 *\date     23Dec13
 *
 *\history  \arg 1.0.0, creat this based on "multiurl" mod from soho.  
 *                  
 */
/***************************************************************************/
/*                      CONFIGURATIONS                   */
/***************************************************************************/
#ifndef _XT_PCTL_H_
#define _XT_PCTL_H_


/***************************************************************************/
/*                      INCLUDE_FILES                    */
/***************************************************************************/


/***************************************************************************/
/*                      DEFINES                      */
/***************************************************************************/
#define     PCTL_OWNER_NUM                    (4)
#define     PCTL_OWNER_ID_ALL                 0xffff

#define     PCTL_URL_NUM                        (32)
#define     PCTL_URL_LEN                        (33) //make the actual max length to 32;so the total len is 33;
#define     PCTL_MAX_DNS_SIZE                   (256)

#define     TRUE                                (1)
#define     FALSE                               (0)


/***************************************************************************/
/*                      TYPES                            */
/***************************************************************************/
typedef struct _xt_pctl_info
{
    int id; /* owner id */

    bool blocked;    /* paused */

    bool workday_limit;
    unsigned int workday_time;

    bool workday_bedtime;
    unsigned int workday_begin;
    unsigned int workday_end;

    bool weekend_limit;
    unsigned int weekend_time;

    bool weekend_bedtime;
    unsigned int weekend_begin;
    unsigned int weekend_end;

    int num; 
    char hosts[PCTL_URL_NUM][PCTL_URL_LEN]; /* hosts */

} xt_pctl_info;



#if defined(SUPPORT_SHORTCUT_FE)

struct sfe_ipv6_addr {
	__be32 addr[4];
};

typedef union {
	__be32			ip;
	struct sfe_ipv6_addr	ip6[1];
} sfe_ip_addr_t;

struct sfe_connection_destroy {
	int protocol;
	sfe_ip_addr_t src_ip;
	sfe_ip_addr_t dest_ip;
	__be16 src_port;
	__be16 dest_port;
};
#endif
/***************************************************************************/
/*                      VARIABLES                        */
/***************************************************************************/


/***************************************************************************/
/*                      FUNCTIONS                        */
/***************************************************************************/
#endif
