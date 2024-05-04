/*
   <:copyright-BRCM:2015:DUAL/GPL:standard
   
      Copyright (c) 2015 Broadcom 
      All Rights Reserved
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2, as published by
   the Free Software Foundation (the "GPL").
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   
   A copy of the GPL is available at http://www.broadcom.com/licenses/GPLv2.php, or by
   writing to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
   
   :>
 */

/*
 *  Created on: May/2018
 *      Author: ido@broadcom.com
 */

#include "runner_wifi.h"
#include "enet.h"
#include <rdpa_api.h>
#include "port.h"
#include "mux_index.h"
#ifdef CONFIG_BCM_FTTDP_G9991
#include "g9991.h"
#endif
#include "runner_common.h"
#ifdef CONFIG_BCM_PTP_1588
#include "ptp_1588.h"
#endif

/* Forward declerations */
port_ops_t port_runner_port_wan_gbe;
port_ops_t port_runner_port;

int runner_port_flooding_set(bdmf_object_handle port_obj, int enable)
{
    int rc = 0;
#if defined(CONFIG_BCM_RUNNER_FLOODING) && !defined(NETDEV_HW_SWITCH)
    rdpa_port_dp_cfg_t cfg;

    if ((rc = rdpa_port_cfg_get(port_obj , &cfg)))
        return -EFAULT;

    if (!cfg.dal_enable)
    {
        enet_err("DA lookup disabled on port, cannot configure flooding\n");
        return -EFAULT;
    }

    if (enable)
    {
        /* Enable flooding. */
        cfg.dal_miss_action = rdpa_forward_action_flood;
    }
    else
    {
        /* Disable flooding, assume action host. XXX: Are we required to store previous action and support drop? */
        cfg.dal_miss_action = rdpa_forward_action_host;
    }
    rc = rdpa_port_cfg_set(port_obj, &cfg);
#endif
    return rc;
}

static int tr_runner_hw_switching_set_single(enetx_port_t *port, void *_ctx)
{
    unsigned long state = (unsigned long)_ctx;

    if (port->n.port_netdev_role != PORT_NETDEV_ROLE_LAN || !port->dev)
        return 0;
    return runner_port_flooding_set(port->priv, state == HW_SWITCHING_ENABLED);
}

static int runner_hw_switching_state = HW_SWITCHING_DISABLED;

static int runner_hw_switching_set(enetx_port_t *sw, unsigned long state)
{
    int rc;

    rc = port_traverse_ports(root_sw, tr_runner_hw_switching_set_single, PORT_CLASS_PORT, (void *)state);
    if (!rc)
        runner_hw_switching_state = (int)state;
    return rc;
}

static int runner_hw_switching_get(enetx_port_t *sw)
{
    return runner_hw_switching_state;
}

#if defined(CONFIG_BCM_PTP_1588) && !defined(XRDP)
static int port_runner_1588_sw_demux(enetx_port_t *sw, enetx_rx_info_t *rx_info, FkBuff_t *fkb, enetx_port_t **out_port)
{
    uint32_t reason = rx_info->reason;

    if (unlikely(reason == rdpa_cpu_rx_reason_etype_ptp_1588))
        ptp_1588_rx_pkt_store_timestamp(fkb->data, fkb->len, rx_info->ptp_index);
    
    return mux_get_rx_index(sw, rx_info, fkb, out_port);
}
#endif

sw_ops_t port_runner_sw =
{
    .init = port_runner_sw_init,
    .uninit = port_runner_sw_uninit,
#if defined(CONFIG_BCM_PTP_1588) && !defined(XRDP)
    .mux_port_rx = port_runner_1588_sw_demux,
#else
    .mux_port_rx = mux_get_rx_index,
#endif
    .mux_free = mux_index_sw_free,
    .stats_get = port_generic_stats_get,
    .port_id_on_sw = port_runner_sw_port_id_on_sw,
    .hw_sw_state_set = runner_hw_switching_set,
    .hw_sw_state_get = runner_hw_switching_get,
};

int _demux_id_runner_port(enetx_port_t *self)
{
    rdpa_if demuxif = self->p.port_id;

#if defined(CONFIG_BCM_PON_RDP) || defined(CONFIG_BCM_PON_XRDP)
#ifndef CONFIG_BCM_FTTDP_G9991
    /* XXX non-G9991 FW wan gbe src port is lan0 + mac_id, not wan0 */
    if (rdpa_if_is_wan(demuxif))
    {
#ifndef XRDP
        demuxif = rdpa_if_lan0 + self->p.mac->mac_id;

        if (self->p.mac->mac_id == 5)
#endif
            demuxif = rdpa_wan_type_to_if(rdpa_wan_gbe); 
    }
#endif
#endif /* defined(CONFIG_BCM_PON_RDP) || defined(CONFIG_BCM_PON_XRDP) */
    return demuxif;
}

int link_pbit_tc_to_q_to_rdpa_lan_port(bdmf_object_handle port_obj)
{
    bdmf_error_t rc = 0;
    bdmf_object_handle rdpa_tc_to_queue_obj = NULL;
    bdmf_object_handle rdpa_pbit_to_queue_obj = NULL;
    rdpa_tc_to_queue_key_t t2q_key;

    t2q_key.dir = rdpa_dir_ds; 
    t2q_key.table = 0; 

    if ((rdpa_tc_to_queue_get(&t2q_key, &rdpa_tc_to_queue_obj)))
    {
        enet_err("Failed to get tc_to_q object. rc=%d\n", rc);
        goto Exit;
    }
   
    if ((rc = bdmf_link(rdpa_tc_to_queue_obj, port_obj, NULL)))
    {
        enet_err("Failed to link tc_to_q table to RDPA port rc=%d\n", rc);
        goto Exit;
    }
    
    if ((rdpa_pbit_to_queue_get(0, &rdpa_pbit_to_queue_obj)))
    {
        enet_err("Failed to get pbit_to_q object. rc=%d\n", rc);
        goto Exit;
    }
   
    if ((rc = bdmf_link(rdpa_pbit_to_queue_obj, port_obj, NULL)))
    {
        enet_err("Failed to link pbit_to_q table to RDPA port rc=%d\n", rc);
        goto Exit;
    }

Exit:
    if (rdpa_tc_to_queue_obj)
        bdmf_put(rdpa_tc_to_queue_obj);
    if (rdpa_pbit_to_queue_obj)
        bdmf_put(rdpa_pbit_to_queue_obj);

    return rc;
}

int port_runner_port_init(enetx_port_t *self)
{
    bdmf_error_t rc;
    rdpa_if rdpaif = self->p.port_id;
    rdpa_if control_sid = rdpa_if_none;
    rdpa_emac emac = rdpa_emac_none;

    if (mux_set_rx_index(self->p.parent_sw, _demux_id_runner_port(self), self))
        return -1;

    if (self->p.mac)
        emac = rdpa_emac0 + self->p.mac->mac_id;

#ifdef CONFIG_BLOG
    self->n.blog_phy = BLOG_ENETPHY;
    self->n.blog_chnl = self->n.blog_chnl_rx = emac;
#endif

#ifdef CONFIG_BCM_FTTDP_G9991
    /* Not set later when initializing g9991 SW because of FW limitation: must be set when creating LAG port object */
    if (self->p.child_sw)
        control_sid = get_first_high_priority_sid_from_sw(self->p.child_sw);
#endif

    if (!(self->priv = create_rdpa_port(rdpaif, emac, NULL, control_sid)))
    {
        enet_err("Failed to create RDPA port object for %s\n", self->obj_name);
        return -1;
    }

    /* Override bp_parser settings, since once a rdpa port object is created, port role cannot change */
    self->p.port_cap = rdpa_if_is_wan(rdpaif) ? PORT_CAP_WAN_ONLY : PORT_CAP_LAN_ONLY;
    self->n.port_netdev_role = rdpa_if_is_wan(rdpaif) ? PORT_NETDEV_ROLE_WAN : PORT_NETDEV_ROLE_LAN;

    if (rdpa_if_is_wan(rdpaif))
        self->p.ops = &port_runner_port_wan_gbe; /* override ops with correct dispatch_pkt */
#if !defined(CONFIG_BCM_FTTDP_G9991) && !defined(CONFIG_BCM963158)
    else
    {
        rc = link_pbit_tc_to_q_to_rdpa_lan_port(self->priv);
        if (rc)
            return rc;
    }
#endif

    if (rdpa_if_is_lag_and_switch(rdpaif) && (rc = link_switch_to_rdpa_port(self->priv)))
    {
        enet_err("Failed to link RDPA switch to port object %s. rc =%d\n", self->obj_name, rc);
        return rc;
    }

    enet_dbg("Initialized runner port %s\n", self->obj_name);

    return 0;
}

int port_runner_dispatch_pkt_lan(dispatch_info_t *dispatch_info)
{
    rdpa_cpu_tx_info_t info;
#ifdef CONFIG_BCM_PTP_1588
    char *ptp_offset;
#endif

    info.method = rdpa_cpu_tx_port;
    info.port = dispatch_info->port->p.port_id;
    info.cpu_port = rdpa_cpu_host;
    info.x.lan.queue_id = dispatch_info->egress_queue;
    info.drop_precedence = dispatch_info->drop_eligible;
    info.flags = 0;

    enet_dbg_tx("rdpa_cpu_send: port %d queue %d\n", info.port, dispatch_info->egress_queue);

#ifdef CONFIG_BCM_PTP_1588
    if (unlikely(is_pkt_ptp_1588(dispatch_info->pNBuff, &info, &ptp_offset)))
        return ptp_1588_cpu_send_sysb((bdmf_sysb)dispatch_info->pNBuff, &info, ptp_offset);
    else
#endif
    return _rdpa_cpu_send_sysb((bdmf_sysb)dispatch_info->pNBuff, &info);
}

static int dispatch_pkt_gbe_wan(dispatch_info_t *dispatch_info)
{
    rdpa_cpu_tx_info_t info = {};

    info.method = rdpa_cpu_tx_port;
    info.port = dispatch_info->port->p.port_id;
    info.cpu_port = rdpa_cpu_host;
    info.x.wan.queue_id = dispatch_info->egress_queue;
    info.drop_precedence = dispatch_info->drop_eligible;

    enet_dbg_tx("rdpa_cpu_send: port %d queue %d\n", info.port, dispatch_info->egress_queue);

    return _rdpa_cpu_send_sysb((bdmf_sysb)dispatch_info->pNBuff, &info);
}

int enetxapi_post_config(void)
{
#ifdef ENET_RUNNER_WIFI
    if (register_wifi_dev_forwarder())
        return -1;
#endif

    return 0;
}

port_ops_t port_runner_port =
{
    .init = port_runner_port_init,
    .uninit = port_runner_port_uninit,
    .dispatch_pkt = port_runner_dispatch_pkt_lan,
    .stats_get = port_runner_port_stats_get,
    .stats_clear = port_runner_port_stats_clear,
    .pause_get = port_generic_pause_get,
    .pause_set = port_generic_pause_set,
    .mtu_set = port_runner_mtu_set,
    .mib_dump = port_runner_mib_dump,
    .print_status = port_runner_print_status,
    .print_priv = port_runner_print_priv,
    .link_change = port_runner_link_change,
};

port_ops_t port_runner_port_wan_gbe =
{
    .init = port_runner_port_init,
    .uninit = port_runner_port_uninit,
    .dispatch_pkt = dispatch_pkt_gbe_wan,
    .stats_get = port_runner_port_stats_get,
    .stats_clear = port_runner_port_stats_clear,
    .pause_get = port_generic_pause_get,
    .pause_set = port_generic_pause_set,
    .mtu_set = port_runner_mtu_set,
    .mib_dump = port_runner_mib_dump,
    .print_status = port_runner_print_status,
    .print_priv = port_runner_print_priv,
    .link_change = port_runner_link_change,
};

#ifdef EPON
static void port_runner_epon_stats(enetx_port_t *self, struct rtnl_link_stats64 *net_stats)
{
    port_generic_stats_get(self, net_stats);
}

int create_rdpa_egress_tm(bdmf_object_handle port_obj);

bdmf_object_handle create_epon_rdpa_port(mac_dev_t *mac_dev)
{
    BDMF_MATTR(port_attrs, rdpa_port_drv());
    bdmf_object_handle port_obj = NULL;
    bdmf_object_handle cpu_obj = NULL;
    rdpa_wan_type wan_type = rdpa_wan_xepon;
    rdpa_port_dp_cfg_t port_cfg = {0};
    mac_cfg_t mac_cfg = {};
    bdmf_error_t rc = 0;

    if (!mac_dev || !mac_dev->mac_drv)
        return NULL;
    
#if defined(CONFIG_BCM_PON_XRDP) && !defined(CONFIG_GPON_SFU) && !defined(CONFIG_EPON_SFU)
    /* XRDP: For WAN port, SAL and DAL should be disabled by default */
    port_cfg.sal_enable = 0;
    port_cfg.dal_enable = 0;
#else
    port_cfg.sal_enable = 1;
    port_cfg.dal_enable = 1;
#endif
    port_cfg.sal_miss_action = rdpa_forward_action_host;
    port_cfg.dal_miss_action = rdpa_forward_action_host;
    port_cfg.control_sid = rdpa_if_none;
    port_cfg.emac = rdpa_emac_none;
    port_cfg.physical_port = rdpa_physical_none;
    port_cfg.ls_fc_enable = 0;
    port_cfg.ae_enable = (mac_dev->mac_drv->mac_type == MAC_TYPE_EPON_AE);
        
    mac_dev_cfg_get(mac_dev, &mac_cfg);
    if ((!port_cfg.ae_enable) && (mac_cfg.speed != MAC_SPEED_10000))
    {
        wan_type = rdpa_wan_epon;
    }
    rdpa_port_index_set(port_attrs, rdpa_wan_type_to_if(wan_type));
    rdpa_port_wan_type_set(port_attrs, wan_type);
    rdpa_port_cfg_set(port_attrs, &port_cfg);
    if (port_cfg.ae_enable)
    {
        if (mac_cfg.speed == MAC_SPEED_100)
            rdpa_port_speed_set(port_attrs,rdpa_speed_100m);
        else if (mac_cfg.speed == MAC_SPEED_1000)
            rdpa_port_speed_set(port_attrs,rdpa_speed_1g);
        else if (mac_cfg.speed == MAC_SPEED_2500)
            rdpa_port_speed_set(port_attrs,rdpa_speed_2_5g);
        else if (mac_cfg.speed == MAC_SPEED_5000)
            rdpa_port_speed_set(port_attrs,rdpa_speed_5g);
        else if (mac_cfg.speed == MAC_SPEED_10000)
            rdpa_port_speed_set(port_attrs,rdpa_speed_10g);
        else
        {
            enet_err("AE speed is not supported");
            goto Exit;
        }
    }

    rc = bdmf_new_and_set(rdpa_port_drv(), NULL, port_attrs, &port_obj);
    if (rc < 0)
    {
        enet_err("Failed to create wan port");
        goto Exit;
    }

    rc = rdpa_cpu_get(rdpa_cpu_host, &cpu_obj);
    if (rc)
    {
        enet_err("Failed to get CPU object, rc(%d)", rc);
        goto Exit;
    }
#ifdef CONFIG_BCM_PON_XRDP
    rc = rdpa_port_cpu_obj_set(port_obj, cpu_obj);
    if (rc)
    {
        enet_err("Failed to set CPU object to WAN port, rc(%d)", rc);
        goto Exit;
    }
#endif

Exit:
    if (cpu_obj)
        bdmf_put(cpu_obj);
    
    if (rc && port_obj)
    {
        bdmf_destroy(port_obj);
        port_obj = NULL;
    }

    return port_obj;
}

static int port_runner_epon_init(enetx_port_t *self)
{
    rdpa_port_dp_cfg_t cfg;
    bdmf_error_t rc = 0;

    self->p.port_cap = PORT_CAP_WAN_ONLY;
    self->n.port_netdev_role = PORT_NETDEV_ROLE_WAN;
#ifdef CONFIG_BLOG
    self->n.blog_phy = BLOG_EPONPHY;
#endif
    if (mux_set_rx_index(self->p.parent_sw, rdpa_wan_type_to_if(rdpa_wan_epon), self))
        return -1;

    if (!(self->priv = create_epon_rdpa_port(self->p.mac)))
    {
        enet_err("Failed to create EPON rdpa port object for %s\n", self->obj_name);
        return -1;
    }
    
    rc = rdpa_port_get(rdpa_wan_type_to_if(rdpa_wan_epon), (bdmf_object_handle *)&self->priv);
    if (rc)
        goto Exit;

    rc = rdpa_port_cfg_get(self->priv, &cfg);
    if (rc)
        goto Exit;

    if (cfg.ae_enable && (rc = create_rdpa_egress_tm(self->priv)))
        goto Exit;
    
    if (!cfg.ae_enable)
    {
        self->n.set_channel_in_mark = 1; /*Epon mode: channel will be set to/from gem */
    }
#ifdef CONFIG_BLOG    
    else
    {
        self->n.blog_chnl = self->n.blog_chnl_rx = 0; /*AE mode: set blog_chnl=0 */
    }
#endif

Exit:
    if (self->priv)
        bdmf_put(self->priv);
    return rc;
}

static int port_runner_epon_uninit(enetx_port_t *self)
{
    bdmf_object_handle port_obj = self->priv;
    rdpa_port_tm_cfg_t port_tm_cfg;

    if (!port_obj)
        return 0;

    rdpa_port_tm_cfg_get(port_obj, &port_tm_cfg);
    if (port_tm_cfg.sched)
        bdmf_destroy(port_tm_cfg.sched);

    rdpa_port_uninit_set(port_obj, 1);
    bdmf_destroy(port_obj);
    self->priv = 0;

    mux_set_rx_index(self->p.parent_sw, rdpa_wan_type_to_if(rdpa_wan_epon), NULL);

    return 0;
}

static int dispatch_pkt_epon(dispatch_info_t *dispatch_info)
{
    int rc;
    rdpa_cpu_tx_info_t info = {};

    /* TODO: queue/channel mapping from EponFrame.c:EponTrafficSend() */
    info.method = rdpa_cpu_tx_port;
    info.port = rdpa_wan_type_to_if(rdpa_wan_epon);
    info.cpu_port = rdpa_cpu_host;
    info.x.wan.queue_id = dispatch_info->egress_queue;
    if (dispatch_info->port->n.set_channel_in_mark)
        info.x.wan.flow = dispatch_info->channel;
        
    info.drop_precedence = dispatch_info->drop_eligible;

    rc = _rdpa_cpu_send_sysb((bdmf_sysb)dispatch_info->pNBuff, &info);

    return rc;
}

port_ops_t port_runner_epon =
{
    .init = port_runner_epon_init,
    .uninit = port_runner_epon_uninit,
    .dispatch_pkt = dispatch_pkt_epon,
    .stats_get = port_runner_epon_stats,
    .mtu_set = port_runner_mtu_set,
    /* TODO: stats_clear */
    .mib_dump = port_runner_mib_dump,
    .print_status = port_runner_print_status,
    .print_priv = port_runner_print_priv,
    .link_change = port_runner_link_change,
};
#endif /* EPON */

