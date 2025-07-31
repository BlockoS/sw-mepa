// Copyright (c) 2004-2020 Microchip Technology Inc. and its subsidiaries.
// SPDX-License-Identifier: MIT


#include <microchip/ethernet/phy/api.h>
#include <mepa_driver.h>
#include <mepa_ts_driver.h>
#include "lan80xx_private.h"
#include "lan80xx_ts_private.h"
#include "lan80xx_types.h"
#include "regs_lan80xx.h"
#include <lan80xx_ts.h>

static mepa_ts_fifo_read_t fifo_cb = NULL;

static uint8_t lan80xx_get_vs_addr_type(mepa_ts_mac_match_mode_t mac_match)
{
    uint8_t ret = 0;
    switch (mac_match) {
    case MEPA_TS_ETH_ADDR_MATCH_48BIT:
        ret = LAN80XX_PHY_TS_ETH_ADDR_MATCH_48BIT;
        break;
    case MEPA_TS_ETH_ADDR_MATCH_ANY_UNICAST:
        ret = LAN80XX_PHY_TS_ETH_ADDR_MATCH_ANY_UNICAST;
        break;
    case MEPA_TS_ETH_ADDR_MATCH_ANY_MULTICAST:
        ret = LAN80XX_PHY_TS_ETH_ADDR_MATCH_ANY_MULTICAST;
        break;
    case MEPA_TS_ETH_ADDR_MATCH_ANY:
    default:
        ret = LAN80XX_PHY_TS_ETH_ADDR_MATCH_ANY_MULTICAST | MEPA_TS_ETH_ADDR_MATCH_ANY_UNICAST;
        break;
    }
    return ret;
}

static uint8_t lan80xx_get_vs_mac_type(mepa_ts_mac_match_select_t mac_type)
{
    uint8_t ret;
    switch (mac_type) {
    case MEPA_TS_ETH_MATCH_DEST_ADDR:
        ret = LAN80XX_PHY_TS_ETH_MATCH_DEST_ADDR;
        break;
    case MEPA_TS_ETH_MATCH_SRC_ADDR:
        ret = LAN80XX_PHY_TS_ETH_MATCH_SRC_ADDR;
        break;
    case MEPA_TS_ETH_MATCH_SRC_OR_DEST:
    default:
        ret = LAN80XX_PHY_TS_ETH_MATCH_SRC_OR_DEST;
        break;
    }
    return ret;
}


static uint8_t lan80xx_get_vs_ntw_type(mepa_ts_ip_match_select_t ntw_type)
{
    uint8_t ret;
    switch (ntw_type) {
    case MEPA_TS_IP_MATCH_DEST:
        ret = LAN80XX_PHY_TS_IP_MATCH_DEST;
        break;
    case MEPA_TS_IP_MATCH_SRC:
        ret = LAN80XX_PHY_TS_IP_MATCH_SRC;
        break;
    case MEPA_TS_IP_MATCH_SRC_OR_DEST:
    case MEPA_TS_IP_MATCH_NONE:
    default:
        ret = LAN80XX_PHY_TS_IP_MATCH_SRC_OR_DEST;
        break;
    }
    return ret;
}

static mepa_rc mepa_to_lan80xx_encap(mepa_ts_pkt_encap_t encap, phy25g_ts_encap_t *phy25g_encap)
{
    mepa_rc rc = MEPA_RC_OK;
    switch (encap) {
    case MEPA_TS_ENCAP_ETH_PTP:
        *phy25g_encap = LAN80XX_PHY_TS_ENCAP_ETH_PTP;
        break;

    case MEPA_TS_ENCAP_ETH_IP_PTP:
        *phy25g_encap = LAN80XX_PHY_TS_ENCAP_ETH_IP_PTP;
        break;

    case MEPA_TS_ENCAP_ETH_IP_IP_PTP:
        *phy25g_encap = LAN80XX_PHY_TS_ENCAP_ETH_IP_IP_PTP;
        break;

    case MEPA_TS_ENCAP_ETH_ETH_PTP:
        *phy25g_encap = LAN80XX_PHY_TS_ENCAP_ETH_ETH_PTP;
        break;

    case MEPA_TS_ENCAP_ETH_ETH_IP_PTP:
        *phy25g_encap = LAN80XX_PHY_TS_ENCAP_ETH_ETH_IP_PTP;
        break;

    case  MEPA_TS_ENCAP_ETH_MPLS_IP_PTP:
        *phy25g_encap = LAN80XX_PHY_TS_ENCAP_ETH_MPLS_IP_PTP;
        break;

    case MEPA_TS_ENCAP_ETH_MPLS_ETH_PTP:
        *phy25g_encap = LAN80XX_PHY_TS_ENCAP_ETH_MPLS_ETH_PTP;
        break;

    case MEPA_TS_ENCAP_ETH_MPLS_ETH_IP_PTP:
        *phy25g_encap = LAN80XX_PHY_TS_ENCAP_ETH_MPLS_ETH_IP_PTP;
        break;

    case MEPA_TS_ENCAP_NONE:
        *phy25g_encap = LAN80XX_PHY_TS_ENCAP_NONE;
        break;

    default:
        rc = MEPA_RC_ERROR;
        break;
    }
    return rc;
}

static phy25g_ptp_action_type_t mepa_to_lan80xx_ls_action(mepa_ts_ls_type_t ls_type)
{
    phy25g_ptp_action_type_t ls_action = LAN80XX_TS_CMD_IDLE;

    if (ls_type == MEPA_TS_CMD_LOAD) {
        return LAN80XX_TS_CMD_LOAD;
    } else if (ls_type == MEPA_TS_CMD_SAVE) {
        return LAN80XX_TS_CMD_STORE;
    } else if (ls_type == MEPA_TS_CMD_WAVEFORM) {
        return LAN80XX_TS_CMD_WAVEFORM;
    } else if (ls_type == MEPA_TS_CMD_DELTA) {
        return  LAN80XX_TS_CMD_DELTA;
    } else if (ls_type == MEPA_TS_CMD_TOD) {
        return LAN80XX_TS_CMD_TOD;
    }

    return ls_action;
}


static mepa_rc lan80xx_ts_reset(mepa_device_t *dev, const mepa_ts_reset_conf_t *const tsreset)
{
    mepa_rc rc = MEPA_RC_ERROR;

    MEPA_ENTER(dev);
    rc = lan80xx_ts_reset_priv(dev, tsreset);
    MEPA_EXIT(dev);

    return rc;
}

static mepa_rc lan80xx_ts_init_conf_get(mepa_device_t *dev, mepa_ts_init_conf_t *const ts_init_conf)
{
    mepa_rc rc = MEPA_RC_ERROR;
    mepa_bool_t                     ts_init_done = FALSE;
    phy25g_phy_ts_init_conf_t         init_conf;
    phy25g_phy_state_t *data = (phy25g_phy_state_t *)dev->data;

    MEPA_ENTER(dev);
    rc =  lan80xx_phy_ts_init_conf_get(dev, data->port_no, &ts_init_done, &init_conf);
    MEPA_EXIT(dev);
    if (rc != MEPA_RC_OK) {
        T_E(MEPA_TRACE_GRP_GEN, "ts init conf get failed\n");
        return rc;
    }
    if (!ts_init_done) {
        T_W(MEPA_TRACE_GRP_TS, "TS block is not initialized\n");
    }
    /*
     * MEPA-1147
     * The mepa_ts_demo and lan80xx clk src mappings are different
     * So, need to assign appropriate mepa_ts_demo clk src
     */
    switch (init_conf.clk_src) {
    case LAN80XX_PHY_TS_CLOCK_SRC_SYSREFCLK:
        ts_init_conf->clk_src = MEPA_TS_CLOCK_SRC_INTERNAL;
        break;
    case LAN80XX_PHY_TS_CLOCK_SRC_LINE0:
        ts_init_conf->clk_src = MEPA_TS_CLOCK_SRC_FROM_RX_PORT0;
        break;
    case LAN80XX_PHY_TS_CLOCK_SRC_LINE1:
        ts_init_conf->clk_src = MEPA_TS_CLOCK_SRC_FROM_RX_PORT1;
        break;
    case LAN80XX_PHY_TS_CLOCK_SRC_LINE2:
        ts_init_conf->clk_src = MEPA_TS_CLOCK_SRC_FROM_RX_PORT2;
        break;
    case LAN80XX_PHY_TS_CLOCK_SRC_LINE3:
        ts_init_conf->clk_src = MEPA_TS_CLOCK_SRC_FROM_RX_PORT3;
        break;
    case LAN80XX_PHY_TS_CLOCK_SRC_EXTERNAL_25MHZ:
        ts_init_conf->clk_src = MEPA_TS_CLOCK_SRC_EXTERNAL;
        break;

    default:
        T_E(MEPA_TRACE_GRP_GEN, "Not a valide clock src %s:  %d\n", __FUNCTION__, ts_init_conf->clk_src);
        /* We don't return error here, as get function just fetch existing config */
    }
    ts_init_conf->clk_freq          = init_conf.clk_freq;
    ts_init_conf->rx_ts_len         = init_conf.rx_ts_len == LAN80XX_PHY_TS_RX_TIMESTAMP_LEN_30BIT ? MEPA_TS_RX_TIMESTAMP_LEN_30BIT : MEPA_TS_RX_TIMESTAMP_LEN_32BIT;
    ts_init_conf->rx_ts_pos         = init_conf.rx_ts_pos == LAN80XX_PHY_TS_RX_TIMESTAMP_POS_AT_END ? MEPA_TS_RX_TIMESTAMP_POS_AT_END : MEPA_TS_RX_TIMESTAMP_POS_IN_PTP;
    ts_init_conf->tx_fifo_mode      = init_conf.tx_fifo_mode;
    ts_init_conf->tx_fifo_spi_conf  = init_conf.tx_fifo_spi_conf;
    ts_init_conf->tx_ts_len         = init_conf.tx_ts_len == LAN80XX_PHY_TS_FIFO_TIMESTAMP_LEN_10BYTE ? MEPA_TS_FIFO_TIMESTAMP_LEN_10BYTE : MEPA_TS_FIFO_TIMESTAMP_LEN_4BYTE;
    ts_init_conf->auto_clear_ls     = init_conf.auto_clear_ls;
    ts_init_conf->dly_req_recv_10byte_ts = data->ts.dly_req_recv_10byte_ts;

    return MEPA_RC_OK;
}

static mepa_rc lan80xx_ts_init_conf_set(mepa_device_t *dev, const mepa_ts_init_conf_t *const ts_init_conf)
{
    phy25g_phy_state_t *data = (phy25g_phy_state_t *)dev->data;
    mepa_device_t *base_dev;
    phy25g_phy_state_t *base_data;
    LAN80XX_BASE_DEV(data, base_dev, base_data);

    if (base_data->features.ptp_1588_disable != 0U) {
        T_E(MEPA_TRACE_GRP_GEN, "\n PHY SKU on port : %d doesn't support PTP", data->port_no);
        return MEPA_RC_ERROR;
    }

    phy25g_phy_ts_init_conf_t init_conf = {0};
    mepa_rc rc = MEPA_RC_ERROR;

    init_conf.clk_freq = ts_init_conf->clk_freq;
    switch (ts_init_conf->clk_src) {
    case MEPA_TS_CLOCK_SRC_INTERNAL:
        init_conf.clk_src =  LAN80XX_PHY_TS_CLOCK_SRC_SYSREFCLK;
        break;
    case MEPA_TS_CLOCK_SRC_FROM_RX_PORT0:
        init_conf.clk_src =  LAN80XX_PHY_TS_CLOCK_SRC_LINE0;
        break;
    case MEPA_TS_CLOCK_SRC_FROM_RX_PORT1:
        init_conf.clk_src =  LAN80XX_PHY_TS_CLOCK_SRC_LINE1;
        break;
    case MEPA_TS_CLOCK_SRC_FROM_RX_PORT2:
        init_conf.clk_src =  LAN80XX_PHY_TS_CLOCK_SRC_LINE2;
        break;
    case MEPA_TS_CLOCK_SRC_FROM_RX_PORT3:
        init_conf.clk_src =  LAN80XX_PHY_TS_CLOCK_SRC_LINE3;
        break;
    case MEPA_TS_CLOCK_SRC_EXT_1588_REF_CLOCK:
    case MEPA_TS_CLOCK_SRC_EXTERNAL:
        init_conf.clk_src = LAN80XX_PHY_TS_CLOCK_SRC_EXTERNAL_25MHZ;
        break;

    default:
        T_E(MEPA_TRACE_GRP_GEN, "Not a valide clock src %s:  %d\n", __FUNCTION__, ts_init_conf->clk_src);
        return MEPA_RC_ERROR;
    }
    init_conf.rx_ts_len  = ts_init_conf->rx_ts_len == MEPA_TS_RX_TIMESTAMP_LEN_30BIT ? LAN80XX_PHY_TS_RX_TIMESTAMP_LEN_30BIT : LAN80XX_PHY_TS_RX_TIMESTAMP_LEN_32BIT;
    init_conf.rx_ts_pos  = ts_init_conf->rx_ts_pos == MEPA_TS_RX_TIMESTAMP_POS_AT_END ? LAN80XX_PHY_TS_RX_TIMESTAMP_POS_AT_END : LAN80XX_PHY_TS_RX_TIMESTAMP_POS_IN_PTP;
    init_conf.tx_fifo_mode = ts_init_conf->tx_fifo_mode;
    init_conf.tx_ts_len = ts_init_conf->tx_ts_len == MEPA_TS_FIFO_TIMESTAMP_LEN_10BYTE ? LAN80XX_PHY_TS_FIFO_TIMESTAMP_LEN_10BYTE : LAN80XX_PHY_TS_FIFO_TIMESTAMP_LEN_4BYTE;
    init_conf.tx_fifo_spi_conf = ts_init_conf->tx_fifo_spi_conf;
    init_conf.auto_clear_ls = ts_init_conf->auto_clear_ls;
    init_conf.tc_op_mode = mepa_to_lan80xx_tc_opmode(ts_init_conf->tc_op_mode);
    init_conf.macsec_ena = FALSE;
    init_conf.chk_ing_modified = FALSE;
    init_conf.one_step_txfifo = FALSE;
    init_conf.mch_conf = ts_init_conf->mch_pch_conf;
    MEPA_ENTER(dev);
    rc = lan80xx_phy_ts_init(dev, data->port_no, &init_conf);
    if (data->phy_ts_port_conf.port_ts_init_done == TRUE) {
        data->ts.dly_req_recv_10byte_ts = ts_init_conf->dly_req_recv_10byte_ts;
    }
    MEPA_EXIT(dev);

    return rc;
}

static mepa_rc lan80xx_ts_tx_classifier_conf_set(struct mepa_device *dev,
                                                 uint16_t flow_index, const mepa_ts_classifier_t *const pkt_class_conf)
{
    mepa_rc rc;
    phy25g_phy_state_t *data = (phy25g_phy_state_t *)dev->data;
    phy25g_ts_eng_conf_t *eng_conf;
    phy25g_ts_engine_flow_conf_t flow_conf;
    const mepa_range_unit16_t *range_in;
    const mepa_value_unit16_t *val_in;
    const mepa_ts_classifier_eth_t *eth_in, *eth2_in;
    phy25g_ts_eth_flow_conf_t *eth_flow, *eth2_flow;
    const mepa_ts_classifier_ip_t *in_ip_conf, *in_ip2_conf;
    phy25g_ts_ip_flow_conf_t *ip_flow, *ip2_flow;
    u16 eng_flow;
    phy25g_ts_encap_t encap;
    phy25g_ts_engine_t eng_id;
    phy25g_ts_mpls_flow_conf_t *mpls_flow;

    if ((rc = lan80xx_get_eng_flow_info(flow_index, &eng_id, &eng_flow) != MEPA_RC_OK)) {
        T_E(MEPA_TRACE_GRP_TS, "Invalid engine ID: %d", eng_id);
        return MEPA_RC_ERROR;
    }

    if ((rc = mepa_to_lan80xx_encap(pkt_class_conf->pkt_encap_type, &encap) != MEPA_RC_OK)) {
        T_E(MEPA_TRACE_GRP_TS, "Invalid Encapsulation : %d", pkt_class_conf->pkt_encap_type);
        return MEPA_RC_ERROR;
    }

    eng_conf = &data->phy_ts_port_conf.egress_eng_conf[eng_id];
    eth_in = &pkt_class_conf->eth_class_conf;
    eth_flow = &flow_conf.flow_conf.ptp.eth1_opt.flow_opt[eng_flow];
    ip_flow = &flow_conf.flow_conf.ptp.ip1_opt.flow_opt[eng_flow];
    mpls_flow = &flow_conf.flow_conf.ptp.mpls_opt.flow_opt[eng_flow];

    if (eng_conf->eng_used && pkt_class_conf->pkt_encap_type != MEPA_TS_ENCAP_NONE && encap != eng_conf->encap_type) {
        T_E(MEPA_TRACE_GRP_TS, "engine encap error");
        T_E(MEPA_TRACE_GRP_TS, "eng_used %d eng_conf->encap_type %d\n", eng_conf->eng_used, eng_conf->encap_type);
        return MEPA_RC_ERR_TS_ENG_ENCAP_OVERWRITE;
    }

    if (pkt_class_conf->pkt_encap_type == MEPA_TS_ENCAP_NONE) {
        if (lan80xx_ts_egress_engine_clear(dev, data->port_no, eng_id) != MESA_RC_OK) {
            T_E(MEPA_TRACE_GRP_TS, "Not able to clear the egress engine %d port %d", eng_id, data->port_no);
            return MEPA_RC_ERR_TS_ENG_CLR;
        }
        T_I(MEPA_TRACE_GRP_TS, "engine conf cleared");
    }

    else {
        T_D(MEPA_TRACE_GRP_TS, "Initializing engine (%d)flow...\n", eng_id);
        memcpy(&flow_conf, &eng_conf->flow_conf, sizeof(phy25g_ts_engine_flow_conf_t));
        eng_conf->encap_type = encap;
        eng_conf->eng_used = TRUE;
        //Stict mode
        eng_conf->flow_match_mode = TRUE;
        flow_conf.flow_conf.ptp.eth1_opt.comm_opt.etype = eth_in->vlan_conf.etype;
        flow_conf.flow_conf.ptp.eth1_opt.comm_opt.tpid = eth_in->vlan_conf.tpid;
        flow_conf.flow_conf.ptp.eth1_opt.comm_opt.pbb_en =  eth_in->vlan_conf.pbb_en;
        //Engine enable
        flow_conf.eng_mode = TRUE;
        eth_flow->flow_en = TRUE;
        eth_flow->addr_match_mode = lan80xx_get_vs_addr_type(pkt_class_conf->eth_class_conf.mac_match_mode);
        eth_flow->addr_match_select = lan80xx_get_vs_mac_type(pkt_class_conf->eth_class_conf.mac_match_select);
        memcpy(eth_flow->mac_addr, eth_in->mac_addr, sizeof(eth_in->mac_addr));
        eth_flow->vlan_check = eth_in->vlan_check;
        eth_flow->num_tag = eth_in->vlan_conf.num_tag;
        eth_flow->tag_range_mode = LAN80XX_PHY_TS_TAG_RANGE_NONE;

        if ((flow_conf.flow_conf.ptp.eth1_opt.comm_opt.pbb_en) &&
            ((eth_in->vlan_conf.outer_tag.mode == MEPA_TS_MATCH_MODE_RANGE) ||
             (eth_in->vlan_conf.inner_tag.mode == MEPA_TS_MATCH_MODE_RANGE))) {
            T_E(MEPA_TRACE_GRP_TS, "For pbb enabled case, tag range cannot be configured");
            return MEPA_RC_ERROR;
        }

        if (eth_in->vlan_conf.pbb_en) {
            eth_flow->outer_tag_type = LAN80XX_PHY_TS_TAG_TYPE_B;
            eth_flow->inner_tag_type = LAN80XX_PHY_TS_TAG_TYPE_I;
        } else if (eth_flow->num_tag > 0) {
            eth_flow->outer_tag_type = eth_in->vlan_conf.tpid ? LAN80XX_PHY_TS_TAG_TYPE_S : LAN80XX_PHY_TS_TAG_TYPE_C;
            eth_flow->inner_tag_type = eth_in->vlan_conf.tpid ? LAN80XX_PHY_TS_TAG_TYPE_S : LAN80XX_PHY_TS_TAG_TYPE_C;
        } else if (eth_flow->num_tag > 2) {
            T_E(MEPA_TRACE_GRP_TS, "Tag count should not be greater than 2");
            return MEPA_RC_ERR_TS_ENG_CLR;
        }
        if (eth_in->vlan_conf.outer_tag.mode == MEPA_TS_MATCH_MODE_RANGE) {
            eth_flow->tag_range_mode = LAN80XX_PHY_TS_TAG_RANGE_OUTER;
            range_in =  &pkt_class_conf->eth_class_conf.vlan_conf.outer_tag.match.range;
            eth_flow->outer_tag.range.upper = range_in->upper;
            eth_flow->outer_tag.range.lower = range_in->lower;
        } else {//MEPA_TS_MATCH_MODE_VALUE
            val_in = &pkt_class_conf->eth_class_conf.vlan_conf.outer_tag.match.value;
            eth_flow->outer_tag.value.val = val_in->val;
            eth_flow->outer_tag.value.mask = val_in->mask;
        }
        if (eth_in->vlan_conf.inner_tag.mode == MEPA_TS_MATCH_MODE_RANGE) {
            eth_flow->tag_range_mode = LAN80XX_PHY_TS_TAG_RANGE_INNER;
            range_in = &pkt_class_conf->eth_class_conf.vlan_conf.inner_tag.match.range;
            eth_flow->inner_tag.range.upper = range_in->upper;
            eth_flow->inner_tag.range.lower = range_in->lower;
        } else {
            val_in = &pkt_class_conf->eth_class_conf.vlan_conf.inner_tag.match.value;
            eth_flow->inner_tag.value.val = val_in->val;
            eth_flow->inner_tag.value.mask = val_in->mask;
        }


        if (eng_conf->encap_type == LAN80XX_PHY_TS_ENCAP_ETH_IP_IP_PTP) {
            ip2_flow =  &flow_conf.flow_conf.ptp.ip2_opt.flow_opt[eng_flow];
            in_ip2_conf = &pkt_class_conf->ip2_class_conf;

            ip2_flow->flow_en = TRUE;
            ip2_flow->match_mode = lan80xx_get_vs_ntw_type(in_ip2_conf->ip_match_mode);
            if (in_ip2_conf->ip_ver == MEPA_TS_IP_VER_4) {
                flow_conf.flow_conf.ptp.ip2_opt.comm_opt.ip_mode = LAN80XX_PHY_TS_IP_VER_4;
                flow_conf.flow_conf.ptp.ip2_opt.comm_opt.dport_mask = in_ip2_conf->udp_dport_en ? 0xFFFF : 0;
                flow_conf.flow_conf.ptp.ip2_opt.comm_opt.dport_val =  in_ip2_conf->udp_dport;
                flow_conf.flow_conf.ptp.ip2_opt.comm_opt.sport_mask = in_ip2_conf->udp_sport_en ? 0xFFFF : 0;
                flow_conf.flow_conf.ptp.ip2_opt.comm_opt.sport_val =  in_ip2_conf->udp_sport;
                ip2_flow->ip_addr.ipv4.addr = in_ip2_conf->ip_addr.ipv4.addr;
                ip2_flow->ip_addr.ipv4.mask = in_ip2_conf->ip_addr.ipv4.mask;
            } else {
                flow_conf.flow_conf.ptp.ip2_opt.comm_opt.ip_mode = LAN80XX_PHY_TS_IP_VER_6;
                memcpy(&ip2_flow->ip_addr.ipv6.addr, &in_ip2_conf->ip_addr.ipv6.addr, sizeof(in_ip2_conf->ip_addr.ipv6.addr));
                memcpy(&ip2_flow->ip_addr.ipv6.mask, &in_ip2_conf->ip_addr.ipv6.mask, sizeof(in_ip2_conf->ip_addr.ipv6.mask));
            }
        }

        if ((eng_conf->encap_type == LAN80XX_PHY_TS_ENCAP_ETH_IP_PTP) ||
            (eng_conf->encap_type == LAN80XX_PHY_TS_ENCAP_ETH_ETH_IP_PTP) || (eng_conf->encap_type == LAN80XX_PHY_TS_ENCAP_ETH_IP_IP_PTP)
            || (eng_conf->encap_type == LAN80XX_PHY_TS_ENCAP_ETH_MPLS_IP_PTP) || (eng_conf->encap_type == LAN80XX_PHY_TS_ENCAP_ETH_MPLS_ETH_IP_PTP)) {
            in_ip_conf = &pkt_class_conf->ip_class_conf;
            ip_flow->flow_en = TRUE;
            ip_flow->match_mode = lan80xx_get_vs_ntw_type(in_ip_conf->ip_match_mode);
            if (in_ip_conf->ip_ver == MEPA_TS_IP_VER_4) {
                flow_conf.flow_conf.ptp.ip1_opt.comm_opt.ip_mode = LAN80XX_PHY_TS_IP_VER_4;
                flow_conf.flow_conf.ptp.ip1_opt.comm_opt.dport_mask = in_ip_conf->udp_dport_en ? 0xFFFF : 0;
                flow_conf.flow_conf.ptp.ip1_opt.comm_opt.dport_val = in_ip_conf->udp_dport;
                flow_conf.flow_conf.ptp.ip1_opt.comm_opt.sport_mask = in_ip_conf->udp_sport_en ? 0xFFFF : 0;
                flow_conf.flow_conf.ptp.ip1_opt.comm_opt.sport_val = in_ip_conf->udp_sport;
                ip_flow->ip_addr.ipv4.addr = in_ip_conf->ip_addr.ipv4.addr;
                ip_flow->ip_addr.ipv4.mask = in_ip_conf->ip_addr.ipv4.mask;
            } else {
                flow_conf.flow_conf.ptp.ip1_opt.comm_opt.ip_mode = LAN80XX_PHY_TS_IP_VER_6;
                memcpy(&ip_flow->ip_addr.ipv6.addr, &in_ip_conf->ip_addr.ipv6.addr, sizeof(in_ip_conf->ip_addr.ipv6.addr));
                memcpy(&ip_flow->ip_addr.ipv6.mask, &in_ip_conf->ip_addr.ipv6.mask, sizeof(in_ip_conf->ip_addr.ipv6.mask));
            }
        }


        if ((eng_conf->encap_type  == LAN80XX_PHY_TS_ENCAP_ETH_ETH_PTP) || (eng_conf->encap_type == LAN80XX_PHY_TS_ENCAP_ETH_ETH_IP_PTP)
            || (eng_conf->encap_type == LAN80XX_PHY_TS_ENCAP_ETH_MPLS_ETH_PTP) || (eng_conf->encap_type == LAN80XX_PHY_TS_ENCAP_ETH_MPLS_ETH_IP_PTP)) {
            eth2_flow = &flow_conf.flow_conf.ptp.eth2_opt.flow_opt[eng_flow];
            eth2_in =  &pkt_class_conf->eth2_class_conf;

            flow_conf.flow_conf.ptp.eth2_opt.comm_opt.etype = eth2_in->vlan_conf.etype;
            flow_conf.flow_conf.ptp.eth2_opt.comm_opt.tpid = eth2_in->vlan_conf.tpid;

            eth2_flow->flow_en = TRUE;
            eth2_flow->addr_match_mode = lan80xx_get_vs_addr_type(pkt_class_conf->eth2_class_conf.mac_match_mode);
            eth2_flow->addr_match_select = lan80xx_get_vs_mac_type(pkt_class_conf->eth2_class_conf.mac_match_select);
            eth2_flow->vlan_check = pkt_class_conf->eth2_class_conf.vlan_check;
            eth2_flow->num_tag = pkt_class_conf->eth2_class_conf.vlan_conf.num_tag;

            if (eth2_flow->num_tag > 0) {
                eth2_flow->outer_tag_type = eth2_in->vlan_conf.tpid ? LAN80XX_PHY_TS_TAG_TYPE_S : LAN80XX_PHY_TS_TAG_TYPE_C;
                eth2_flow->inner_tag_type =  eth2_in->vlan_conf.tpid ? LAN80XX_PHY_TS_TAG_TYPE_S : LAN80XX_PHY_TS_TAG_TYPE_C;
            } else if (eth2_flow->num_tag > 2) {
                T_E(MEPA_TRACE_GRP_TS, "Tag should not be greater than 2");
                return MEPA_RC_ERR_TS_ENG_CLR;
            }

            if (eth2_in->vlan_conf.outer_tag.mode == MEPA_TS_MATCH_MODE_RANGE) {
                eth2_flow->tag_range_mode = LAN80XX_PHY_TS_TAG_RANGE_OUTER;
                range_in =  &pkt_class_conf->eth2_class_conf.vlan_conf.outer_tag.match.range;
                eth2_flow->outer_tag.range.upper = range_in->upper;
                eth2_flow->outer_tag.range.lower = range_in->lower;
            } else {
                //MEPA_TS_MATCH_MODE_VALUE
                val_in = &pkt_class_conf->eth2_class_conf.vlan_conf.outer_tag.match.value;
                eth2_flow->outer_tag.value.val = val_in->val;
                eth2_flow->outer_tag.value.mask = val_in->mask;
            }

            if (eth2_in->vlan_conf.inner_tag.mode == MEPA_TS_MATCH_MODE_RANGE) {
                eth2_flow->tag_range_mode = LAN80XX_PHY_TS_TAG_RANGE_INNER;
                range_in = &pkt_class_conf->eth2_class_conf.vlan_conf.inner_tag.match.range;
                eth2_flow->inner_tag.range.upper = range_in->upper;
                eth2_flow->inner_tag.range.lower = range_in->lower;
            } else {
                val_in = &pkt_class_conf->eth2_class_conf.vlan_conf.inner_tag.match.value;
                eth2_flow->inner_tag.value.val = val_in->val;
                eth2_flow->inner_tag.value.mask = val_in->mask;
            }

            memcpy(eth2_flow->mac_addr, pkt_class_conf->eth2_class_conf.mac_addr, sizeof(pkt_class_conf->eth2_class_conf.mac_addr));
        }
        /**
         * MEPA-1163
         * Update the new flow config for mpls as per the pkt_class_config
         **/
        if ((eng_conf->encap_type == LAN80XX_PHY_TS_ENCAP_ETH_MPLS_ETH_PTP) ||
            (eng_conf->encap_type == LAN80XX_PHY_TS_ENCAP_ETH_MPLS_ETH_IP_PTP) ||
            (eng_conf->encap_type == LAN80XX_PHY_TS_ENCAP_ETH_MPLS_IP_PTP)) {
            void *Ptrmem = NULL;
            flow_conf.flow_conf.ptp.mpls_opt.comm_opt.cw_en = pkt_class_conf->mpls_class_conf.cw_en;
            mpls_flow->flow_en = pkt_class_conf->mpls_class_conf.flow_en;
            mpls_flow->stack_depth = pkt_class_conf->mpls_class_conf.stack_depth;
            mpls_flow->stack_ref_point = pkt_class_conf->mpls_class_conf.stack_ref_point;
            Ptrmem = memcpy(&mpls_flow->stack_level.top_down, &pkt_class_conf->mpls_class_conf.stack_level, sizeof(mpls_flow->stack_level.top_down));
            if (Ptrmem == NULL) {
                T_E (MEPA_TRACE_GRP_TS, "TS mpls flow update failed\n");
                return MEPA_RC_ERROR;
            }
        }

        if ((rc = lan80xx_ts_egress_engine_conf_set(dev, eng_id, eng_flow, &flow_conf)) != MEPA_RC_OK) {
            T_E(MEPA_TRACE_GRP_TS, "TS egress engine conf set failed (%d)\n", rc);
            return MEPA_RC_ERROR;
        } else {
            /* Save the flow config */
            memcpy(&eng_conf->flow_conf, &flow_conf,
                   sizeof(phy25g_ts_engine_flow_conf_t));
        }
    }
    return MEPA_RC_OK;
}

static mepa_rc lan80xx_ts_tx_classifier_conf_get(mepa_device_t *dev, uint16_t in_flow,
                                                 mepa_ts_classifier_t *const out_conf)
{
    phy25g_phy_state_t *data = (phy25g_phy_state_t *)dev->data;
    return lan80xx_phy_tx_classifier_conf_get(dev, data->port_no, out_conf);
}


static mepa_rc lan80xx_ts_rx_classifier_conf_set(struct mepa_device *dev,
                                                 uint16_t flow_index, const mepa_ts_classifier_t *const pkt_class_conf)
{
    mepa_rc rc;
    phy25g_phy_state_t *data = (phy25g_phy_state_t *)dev->data;
    phy25g_ts_eng_conf_t *eng_conf;
    const mepa_range_unit16_t *range_in;
    const mepa_value_unit16_t *val_in;
    const mepa_ts_classifier_eth_t *eth_in, *eth2_in;
    phy25g_ts_engine_flow_conf_t flow_conf;
    phy25g_ts_eth_flow_conf_t *eth_flow, *eth2_flow;
    const mepa_ts_classifier_ip_t *in_ip_conf, *in_ip2_conf;
    phy25g_ts_ip_flow_conf_t *ip_flow, *ip2_flow;
    uint16_t eng_flow;
    phy25g_ts_encap_t encap;
    phy25g_ts_engine_t eng_id;
    phy25g_ts_mpls_flow_conf_t *mpls_flow;


    if ((rc = lan80xx_get_eng_flow_info(flow_index, &eng_id, &eng_flow) != MEPA_RC_OK)) {
        T_E(MEPA_TRACE_GRP_TS, "Invalid engine ID: %d", eng_id);
        return MEPA_RC_ERROR;
    }

    if ((rc = mepa_to_lan80xx_encap(pkt_class_conf->pkt_encap_type, &encap) != MEPA_RC_OK)) {
        T_E(MEPA_TRACE_GRP_TS, "Invalid Encapsulation : %d", pkt_class_conf->pkt_encap_type);
        return MEPA_RC_ERROR;
    }

    eng_conf = &data->phy_ts_port_conf.ingress_eng_conf[eng_id];
    eth_in = &pkt_class_conf->eth_class_conf;
    eth_flow = &flow_conf.flow_conf.ptp.eth1_opt.flow_opt[eng_flow];
    ip_flow = &flow_conf.flow_conf.ptp.ip1_opt.flow_opt[eng_flow];
    mpls_flow = &flow_conf.flow_conf.ptp.mpls_opt.flow_opt[eng_flow];
    /* if engine is initialized */
    if (eng_conf->eng_used && pkt_class_conf->pkt_encap_type != MEPA_TS_ENCAP_NONE && encap != eng_conf->encap_type) {
        T_E(MEPA_TRACE_GRP_TS, "engine encap error");
        return MEPA_RC_ERR_TS_ENG_ENCAP_OVERWRITE;
    }
    if (pkt_class_conf->pkt_encap_type == MEPA_TS_ENCAP_NONE) {
        if (lan80xx_ts_egress_engine_clear(dev, data->port_no, eng_id) != MESA_RC_OK) {
            T_I(MEPA_TRACE_GRP_TS, "Not able to clear the egress engine %d port %d", eng_id, data->port_no);
            return MEPA_RC_ERR_TS_ENG_CLR;
        }
        T_I(MEPA_TRACE_GRP_TS, "engine conf cleared");
    } else {
        /* initialize the engine first time */
        /* since only two encaps is used */
        T_D(MEPA_TRACE_GRP_TS, "Initializing engine (%d)flow...\n", eng_id);
        memcpy(&flow_conf, &eng_conf->flow_conf, sizeof(phy25g_ts_engine_flow_conf_t));
        eng_conf->encap_type = encap;
        eng_conf->eng_used = TRUE;
        eng_conf->flow_match_mode = TRUE;
        flow_conf.flow_conf.ptp.eth1_opt.comm_opt.etype = eth_in->vlan_conf.etype;
        flow_conf.flow_conf.ptp.eth1_opt.comm_opt.tpid = eth_in->vlan_conf.tpid;
        flow_conf.flow_conf.ptp.eth1_opt.comm_opt.pbb_en =  eth_in->vlan_conf.pbb_en;
        //Engine enable
        flow_conf.eng_mode = TRUE;
        eth_flow->flow_en = TRUE;
        eth_flow->addr_match_mode = lan80xx_get_vs_addr_type(pkt_class_conf->eth_class_conf.mac_match_mode);
        eth_flow->addr_match_select = lan80xx_get_vs_mac_type(pkt_class_conf->eth_class_conf.mac_match_select);
        memcpy(eth_flow->mac_addr, eth_in->mac_addr, sizeof(eth_in->mac_addr));
        eth_flow->vlan_check = eth_in->vlan_check;
        eth_flow->num_tag = eth_in->vlan_conf.num_tag;
        eth_flow->tag_range_mode = LAN80XX_PHY_TS_TAG_RANGE_NONE;

        if ((flow_conf.flow_conf.ptp.eth1_opt.comm_opt.pbb_en) &&
            ((eth_in->vlan_conf.outer_tag.mode == MEPA_TS_MATCH_MODE_RANGE) ||
             (eth_in->vlan_conf.inner_tag.mode == MEPA_TS_MATCH_MODE_RANGE))) {
            T_E(MEPA_TRACE_GRP_TS, " For pbb enabled case, tag range cannot be configured");
            return MEPA_RC_ERROR;
        }

        if (eth_in->vlan_conf.pbb_en) {
            eth_flow->outer_tag_type = LAN80XX_PHY_TS_TAG_TYPE_B;
            eth_flow->inner_tag_type = LAN80XX_PHY_TS_TAG_TYPE_I;
        } else if (eth_flow->num_tag > 0) {
            eth_flow->outer_tag_type = (eth_in->vlan_conf.tpid) ? LAN80XX_PHY_TS_TAG_TYPE_S : LAN80XX_PHY_TS_TAG_TYPE_C;
            eth_flow->inner_tag_type = (eth_in->vlan_conf.tpid) ? LAN80XX_PHY_TS_TAG_TYPE_S : LAN80XX_PHY_TS_TAG_TYPE_C;
        } else if (eth_flow->num_tag > 2) {
            T_E(MEPA_TRACE_GRP_TS, "Tag count should not be greater than 2");
            return MEPA_RC_ERR_TS_ENG_CLR;
        }

        if (eth_in->vlan_conf.outer_tag.mode == MEPA_TS_MATCH_MODE_RANGE) {
            eth_flow->tag_range_mode = LAN80XX_PHY_TS_TAG_RANGE_OUTER;
            range_in =  &pkt_class_conf->eth_class_conf.vlan_conf.outer_tag.match.range;
            eth_flow->outer_tag.range.upper = range_in->upper;
            eth_flow->outer_tag.range.lower = range_in->lower;
        } else {//MEPA_TS_MATCH_MODE_VALUE
            val_in = &pkt_class_conf->eth_class_conf.vlan_conf.outer_tag.match.value;
            eth_flow->outer_tag.value.val = val_in->val;
            eth_flow->outer_tag.value.mask = val_in->mask;
        }

        if (eth_in->vlan_conf.inner_tag.mode == MEPA_TS_MATCH_MODE_RANGE) {
            eth_flow->tag_range_mode = LAN80XX_PHY_TS_TAG_RANGE_INNER;
            range_in = &pkt_class_conf->eth_class_conf.vlan_conf.inner_tag.match.range;
            eth_flow->inner_tag.range.upper = range_in->upper;
            eth_flow->inner_tag.range.lower = range_in->lower;
        } else {//MEPA_TS_MATCH_MODE_VALUE
            val_in = &pkt_class_conf->eth_class_conf.vlan_conf.inner_tag.match.value;
            eth_flow->inner_tag.value.val = val_in->val;
            eth_flow->inner_tag.value.mask = val_in->mask;
        }



        if (eng_conf->encap_type == LAN80XX_PHY_TS_ENCAP_ETH_IP_IP_PTP) {
            ip2_flow =  &flow_conf.flow_conf.ptp.ip2_opt.flow_opt[eng_flow];
            in_ip2_conf = &pkt_class_conf->ip2_class_conf;

            ip2_flow->flow_en = TRUE;
            ip2_flow->match_mode = lan80xx_get_vs_ntw_type(in_ip2_conf->ip_match_mode);
            if (in_ip2_conf->ip_ver == MEPA_TS_IP_VER_4) {
                flow_conf.flow_conf.ptp.ip2_opt.comm_opt.ip_mode = LAN80XX_PHY_TS_IP_VER_4;
                flow_conf.flow_conf.ptp.ip2_opt.comm_opt.dport_mask = in_ip2_conf->udp_dport_en ? 0xFFFF : 0;
                flow_conf.flow_conf.ptp.ip2_opt.comm_opt.dport_val =  in_ip2_conf->udp_dport;
                flow_conf.flow_conf.ptp.ip2_opt.comm_opt.sport_mask = in_ip2_conf->udp_sport_en ? 0xFFFF : 0;
                flow_conf.flow_conf.ptp.ip2_opt.comm_opt.sport_val =  in_ip2_conf->udp_sport;
                ip2_flow->ip_addr.ipv4.addr = in_ip2_conf->ip_addr.ipv4.addr;
                ip2_flow->ip_addr.ipv4.mask = in_ip2_conf->ip_addr.ipv4.mask;
            } else {
                flow_conf.flow_conf.ptp.ip2_opt.comm_opt.ip_mode = LAN80XX_PHY_TS_IP_VER_6;
                memcpy(&ip2_flow->ip_addr.ipv6.addr, &in_ip2_conf->ip_addr.ipv6.addr, sizeof(in_ip2_conf->ip_addr.ipv6.addr));
                memcpy(&ip2_flow->ip_addr.ipv6.mask, &in_ip2_conf->ip_addr.ipv6.mask, sizeof(in_ip2_conf->ip_addr.ipv6.mask));
            }
        }

        if ((eng_conf->encap_type == LAN80XX_PHY_TS_ENCAP_ETH_IP_PTP) ||
            (eng_conf->encap_type == LAN80XX_PHY_TS_ENCAP_ETH_ETH_IP_PTP) || (eng_conf->encap_type == LAN80XX_PHY_TS_ENCAP_ETH_IP_IP_PTP)
            || (eng_conf->encap_type == LAN80XX_PHY_TS_ENCAP_ETH_MPLS_IP_PTP) || (eng_conf->encap_type == LAN80XX_PHY_TS_ENCAP_ETH_MPLS_ETH_IP_PTP)) {
            in_ip_conf = &pkt_class_conf->ip_class_conf;
            ip_flow->flow_en = TRUE;
            ip_flow->match_mode = lan80xx_get_vs_ntw_type(in_ip_conf->ip_match_mode);
            if (in_ip_conf->ip_ver == MEPA_TS_IP_VER_4) {
                flow_conf.flow_conf.ptp.ip1_opt.comm_opt.ip_mode = LAN80XX_PHY_TS_IP_VER_4;
                flow_conf.flow_conf.ptp.ip1_opt.comm_opt.dport_mask = in_ip_conf->udp_dport_en ? 0xFFFF : 0;
                flow_conf.flow_conf.ptp.ip1_opt.comm_opt.dport_val = in_ip_conf->udp_dport;
                flow_conf.flow_conf.ptp.ip1_opt.comm_opt.sport_mask = in_ip_conf->udp_sport_en ? 0xFFFF : 0;
                flow_conf.flow_conf.ptp.ip1_opt.comm_opt.sport_val = in_ip_conf->udp_sport;
                ip_flow->ip_addr.ipv4.addr = in_ip_conf->ip_addr.ipv4.addr;
                ip_flow->ip_addr.ipv4.mask = in_ip_conf->ip_addr.ipv4.mask;
            } else {
                flow_conf.flow_conf.ptp.ip1_opt.comm_opt.ip_mode = LAN80XX_PHY_TS_IP_VER_6;
                memcpy(&ip_flow->ip_addr.ipv6.addr, &in_ip_conf->ip_addr.ipv6.addr, sizeof(in_ip_conf->ip_addr.ipv6.addr));
                memcpy(&ip_flow->ip_addr.ipv6.mask, &in_ip_conf->ip_addr.ipv6.mask, sizeof(in_ip_conf->ip_addr.ipv6.mask));
            }
        }


        if ((eng_conf->encap_type  == LAN80XX_PHY_TS_ENCAP_ETH_ETH_PTP) || (eng_conf->encap_type == LAN80XX_PHY_TS_ENCAP_ETH_ETH_IP_PTP)
            || (eng_conf->encap_type == LAN80XX_PHY_TS_ENCAP_ETH_MPLS_ETH_PTP) || (eng_conf->encap_type == LAN80XX_PHY_TS_ENCAP_ETH_MPLS_ETH_IP_PTP)) {
            eth2_flow = &flow_conf.flow_conf.ptp.eth2_opt.flow_opt[eng_flow];
            eth2_in =  &pkt_class_conf->eth2_class_conf;

            flow_conf.flow_conf.ptp.eth2_opt.comm_opt.etype = eth2_in->vlan_conf.etype;
            flow_conf.flow_conf.ptp.eth2_opt.comm_opt.tpid = eth2_in->vlan_conf.tpid;

            eth2_flow->flow_en = TRUE;
            eth2_flow->addr_match_mode = lan80xx_get_vs_addr_type(pkt_class_conf->eth2_class_conf.mac_match_mode);
            eth2_flow->addr_match_select = lan80xx_get_vs_mac_type(pkt_class_conf->eth2_class_conf.mac_match_select);
            eth2_flow->vlan_check = pkt_class_conf->eth2_class_conf.vlan_check;
            eth2_flow->num_tag = pkt_class_conf->eth2_class_conf.vlan_conf.num_tag;

            if (eth2_flow->num_tag > 0) {
                eth2_flow->outer_tag_type = eth2_in->vlan_conf.tpid ? LAN80XX_PHY_TS_TAG_TYPE_S : LAN80XX_PHY_TS_TAG_TYPE_C;
                eth2_flow->inner_tag_type =  eth2_in->vlan_conf.tpid ? LAN80XX_PHY_TS_TAG_TYPE_S : LAN80XX_PHY_TS_TAG_TYPE_C;
            } else if (eth2_flow->num_tag > 2) {
                T_E(MEPA_TRACE_GRP_TS, "Tag should not be greater than 2");
                return MEPA_RC_ERR_TS_ENG_CLR;
            }

            if (eth2_in->vlan_conf.outer_tag.mode == MEPA_TS_MATCH_MODE_RANGE) {
                eth2_flow->tag_range_mode = LAN80XX_PHY_TS_TAG_RANGE_OUTER;
                range_in =  &pkt_class_conf->eth2_class_conf.vlan_conf.outer_tag.match.range;
                eth2_flow->outer_tag.range.upper = range_in->upper;
                eth2_flow->outer_tag.range.lower = range_in->lower;
            } else {
                //MEPA_TS_MATCH_MODE_VALUE
                val_in = &pkt_class_conf->eth2_class_conf.vlan_conf.outer_tag.match.value;
                eth2_flow->outer_tag.value.val = val_in->val;
                eth2_flow->outer_tag.value.mask = val_in->mask;
            }

            if (eth2_in->vlan_conf.inner_tag.mode == MEPA_TS_MATCH_MODE_RANGE) {
                eth2_flow->tag_range_mode = LAN80XX_PHY_TS_TAG_RANGE_INNER;
                range_in = &pkt_class_conf->eth2_class_conf.vlan_conf.inner_tag.match.range;
                eth2_flow->inner_tag.range.upper = range_in->upper;
                eth2_flow->inner_tag.range.lower = range_in->lower;
            } else {
                val_in = &pkt_class_conf->eth2_class_conf.vlan_conf.inner_tag.match.value;
                eth2_flow->inner_tag.value.val = val_in->val;
                eth2_flow->inner_tag.value.mask = val_in->mask;
            }

            memcpy(eth2_flow->mac_addr, pkt_class_conf->eth2_class_conf.mac_addr, sizeof(pkt_class_conf->eth2_class_conf.mac_addr));
        }
        /**
         * MEPA-1163
         * Update the new flow config for mpls as per the pkt_class_config
         **/
        if ((eng_conf->encap_type == LAN80XX_PHY_TS_ENCAP_ETH_MPLS_ETH_PTP) ||
            (eng_conf->encap_type == LAN80XX_PHY_TS_ENCAP_ETH_MPLS_ETH_IP_PTP) ||
            (eng_conf->encap_type == LAN80XX_PHY_TS_ENCAP_ETH_MPLS_IP_PTP)) {
            void *Ptrmem = NULL;
            flow_conf.flow_conf.ptp.mpls_opt.comm_opt.cw_en = pkt_class_conf->mpls_class_conf.cw_en;
            mpls_flow->flow_en = pkt_class_conf->mpls_class_conf.flow_en;
            mpls_flow->stack_depth = pkt_class_conf->mpls_class_conf.stack_depth;
            mpls_flow->stack_ref_point = pkt_class_conf->mpls_class_conf.stack_ref_point;
            Ptrmem = memcpy(&mpls_flow->stack_level.top_down, &pkt_class_conf->mpls_class_conf.stack_level, sizeof(mpls_flow->stack_level.top_down));
            if (Ptrmem == NULL) {
                T_E (MEPA_TRACE_GRP_TS, "TS mpls flow update failed\n");
                return MEPA_RC_ERROR;
            }
        }

        if ((rc = lan80xx_ts_ingress_engine_conf_set(dev, eng_id, eng_flow, &flow_conf)) != MEPA_RC_OK) {
            T_E(MEPA_TRACE_GRP_TS, "TS ingress engine conf set failed (%d)\n", rc);
            return MEPA_RC_ERROR;
        } else {
            /* Save the flow config */
            memcpy(&eng_conf->flow_conf, &flow_conf,
                   sizeof(phy25g_ts_engine_flow_conf_t));
        }

    }
    return MEPA_RC_OK;
}

static mepa_rc lan80xx_ts_rx_classifier_conf_get(mepa_device_t *dev, uint16_t in_flow,
                                                 mepa_ts_classifier_t *const out_conf)
{
    phy25g_phy_state_t *data = (phy25g_phy_state_t *)dev->data;
    return lan80xx_phy_rx_classifier_conf_get(dev, data->port_no, out_conf);
}

static mepa_rc lan80xx_ts_tx_clock_conf_set(struct mepa_device *dev, uint16_t clock_id,
                                            const mepa_ts_ptp_clock_conf_t *const ptpclock_conf)
{

    mepa_rc rc = MEPA_RC_OK;
    phy25g_phy_state_t *data = (phy25g_phy_state_t *) dev->data;
    phy25g_ts_engine_action_t action_conf;
    phy25g_ts_eng_conf_t *eng_conf;
    phy25g_ts_ptp_engine_action_t *action;
    if (clock_id > LAN80XX_MAX_CLK_PER_ENGINE) {
        T_E(MEPA_TRACE_GRP_TS, "Max Clock id allowed is: %d ", LAN80XX_MAX_CLK_PER_ENGINE);
        return MEPA_RC_ERROR;
    }
    uint16_t eng_id = clock_id;
    eng_conf = &data->phy_ts_port_conf.egress_eng_conf[eng_id];
    if (!eng_conf->eng_used) {
        T_E(MEPA_TRACE_GRP_TS, "Engine not initialized");
        return MEPA_RC_ERROR;
    }
    memcpy(&action_conf, &eng_conf->action_conf, sizeof(phy25g_ts_engine_action_t));
    action = &action_conf.action.ptp_conf;
    if ((ptpclock_conf->clk_mode == MEPA_TS_PTP_CLOCK_MODE_NONE) ||
        (!ptpclock_conf->enable)) {
        T_I(MEPA_TRACE_GRP_TS, "disabling action for engine ");
        if (action->enable) {
            action->enable = FALSE;
        } else {
            T_E(MEPA_TRACE_GRP_TS, "Prev Action not found for Engine");
            return MEPA_RC_ERROR;
        }
    } else if (action->enable) {
        T_I(MEPA_TRACE_GRP_TS, "action enabled already with clk_mode %d in mode %d\n",
            action->clk_mode, ptpclock_conf->clk_mode);
        if ((action->clk_mode != ptpclock_conf->clk_mode) ||
            (action->delaym_type != ptpclock_conf->delaym_type) ||
            (action->cf_update != ptpclock_conf->cf_update)) {
            T_E(MEPA_TRACE_GRP_TS, "Action in use");
            return MEPA_RC_ERR_TS_ACTION_IN_USE;
        }
    } else {
        T_I(MEPA_TRACE_GRP_TS, "clk_mode %d delaym_type %d\n",
            ptpclock_conf->clk_mode, ptpclock_conf->delaym_type);
        action->enable = TRUE;
        action->clk_mode = ptpclock_conf->clk_mode;
        action->delaym_type = ptpclock_conf->delaym_type;
        action->cf_update = ptpclock_conf->cf_update;
        if (ptpclock_conf->ptp_class_conf.domain.mode == MEPA_TS_MATCH_MODE_RANGE) {
            action->ptp_conf.range_en = 1;
            action->ptp_conf.domain.range.lower = ptpclock_conf->ptp_class_conf.domain.match.range.lower;
            action->ptp_conf.domain.range.upper = ptpclock_conf->ptp_class_conf.domain.match.range.upper;
        } else {
            action->ptp_conf.range_en = 0;
            action->ptp_conf.domain.value.val  = ptpclock_conf->ptp_class_conf.domain.match.value.val & 0xff;
            action->ptp_conf.domain.value.mask = ptpclock_conf->ptp_class_conf.domain.match.value.mask & 0xff;
        }
    }
    if ((rc = lan80xx_ts_egress_engine_action_set(dev, data->port_no, eng_id, &action_conf)) != MEPA_RC_OK) {
        return MEPA_RC_ERROR;
    }

    return MEPA_RC_OK;
}

static mepa_rc lan80xx_ts_rx_clock_conf_set(struct mepa_device *dev, uint16_t clock_id,
                                            const mepa_ts_ptp_clock_conf_t *const ptpclock_conf)
{
    mepa_rc rc = MEPA_RC_OK;
    phy25g_phy_state_t *data = (phy25g_phy_state_t *) dev->data;
    phy25g_ts_engine_action_t action_conf;
    phy25g_ts_eng_conf_t *eng_conf;
    phy25g_ts_ptp_engine_action_t *action;

    if (clock_id >= LAN80XX_MAX_CLK_PER_ENGINE) {
        T_E(MEPA_TRACE_GRP_TS, "Max Clock id allowed is: %d ", LAN80XX_MAX_CLK_PER_ENGINE);
        return MEPA_RC_ERROR;
    }
    uint16_t eng_id = clock_id;
    eng_conf = &data->phy_ts_port_conf.ingress_eng_conf[eng_id];
    if (!eng_conf->eng_used) {
        T_E(MEPA_TRACE_GRP_TS, "Engine not initialized");
        return MEPA_RC_ERROR;
    }
    memcpy(&action_conf, &eng_conf->action_conf, sizeof(phy25g_ts_engine_action_t));
    action = &action_conf.action.ptp_conf;
    if ((ptpclock_conf->clk_mode == MEPA_TS_PTP_CLOCK_MODE_NONE) ||
        (!ptpclock_conf->enable)) {
        T_I(MEPA_TRACE_GRP_TS, "disabling action for engine ");
        if (action->enable) {
            action->enable = FALSE;
        } else {
            T_E(MEPA_TRACE_GRP_TS, "Prev Action not found for Engine");
            return MEPA_RC_ERROR;
        }
    } else if (action->enable) {
        T_I(MEPA_TRACE_GRP_TS, "action enabled already with clk_mode %d in mode %d\n",
            action->clk_mode, ptpclock_conf->clk_mode);
        if ((action->clk_mode != ptpclock_conf->clk_mode) ||
            (action->delaym_type != ptpclock_conf->delaym_type) ||
            (action->cf_update != ptpclock_conf->cf_update)) {
            T_E(MEPA_TRACE_GRP_TS, "Action in use");
            return MEPA_RC_ERR_TS_ACTION_IN_USE;
        }
    } else {
        T_I(MEPA_TRACE_GRP_TS, "clk_mode %d delaym_type %d\n",
            ptpclock_conf->clk_mode, ptpclock_conf->delaym_type);
        action->enable = TRUE;
        action->clk_mode = ptpclock_conf->clk_mode;
        action->delaym_type = ptpclock_conf->delaym_type;
        action->cf_update = ptpclock_conf->cf_update;
        action->delay_req_recieve_timestamp = data->ts.dly_req_recv_10byte_ts;
    }
    if (ptpclock_conf->ptp_class_conf.domain.mode == MEPA_TS_MATCH_MODE_RANGE) {
        action->ptp_conf.range_en = 1;
        action->ptp_conf.domain.range.lower = ptpclock_conf->ptp_class_conf.domain.match.range.lower;
        action->ptp_conf.domain.range.upper = ptpclock_conf->ptp_class_conf.domain.match.range.upper;
    } else {
        action->ptp_conf.range_en = 0;
        action->ptp_conf.domain.value.val  = ptpclock_conf->ptp_class_conf.domain.match.value.val & 0xff;
        action->ptp_conf.domain.value.mask = ptpclock_conf->ptp_class_conf.domain.match.value.mask & 0xff;
    }
    if ((rc = lan80xx_ts_ingress_engine_action_set(dev, data->port_no, eng_id, &action_conf)) != MEPA_RC_OK) {
        return MEPA_RC_ERROR;
    }

    return MEPA_RC_OK;
}

static mepa_rc lan80xx_ts_tx_clock_conf_get(mepa_device_t *dev, uint16_t clock_id,
                                            mepa_ts_ptp_clock_conf_t *const clk_conf)
{
    return lan80xx_ts_tx_clock_conf_get_priv(dev, clock_id, clk_conf);
}

static mepa_rc lan80xx_ts_rx_clock_conf_get(mepa_device_t *dev, uint16_t clock_id,
                                            mepa_ts_ptp_clock_conf_t *const clk_conf)
{
    return lan80xx_ts_rx_clock_conf_get_priv(dev, clock_id, clk_conf);
}

static mepa_rc lan80xx_ts_ltc_ls_en(mepa_device_t *dev, mepa_ts_ls_type_t const ls_type)
{
    phy25g_phy_state_t *data = (phy25g_phy_state_t *)dev->data;
    phy25g_ptp_action_type_t ltc_ls_action;
    mesa_rc rc = MEPA_RC_OK;

    MEPA_ENTER(dev);
    ltc_ls_action = mepa_to_lan80xx_ls_action(ls_type);
    rc = lan80xx_phy_ts_ltc_ls_action_set(dev, data->port_no, ltc_ls_action);
    MEPA_EXIT(dev);

    return rc;

}

static mepa_rc lan80xx_ts_mode_get(mepa_device_t *dev, mepa_bool_t *const enable)
{
    mepa_rc rc = MEPA_RC_ERROR;

    MEPA_ENTER(dev);
    rc = lan80xx_ts_mode_get_priv(dev, enable);
    MEPA_EXIT(dev);

    return rc;
}

static mepa_rc lan80xx_ts_mode_set(mepa_device_t *dev, const mepa_bool_t enable)
{
    mepa_rc rc = MEPA_RC_ERROR;

    MEPA_ENTER(dev);
    rc = lan80xx_ts_mode_set_priv(dev, enable);
    MEPA_EXIT(dev);

    return rc;
}

static mepa_rc lan80xx_ts_ltc_get(mepa_device_t *dev, mepa_timestamp_t *const ts)
{
    phy25g_phy_state_t *data = (phy25g_phy_state_t *)dev->data;
    u32 value = 0;

    LAN80XX_CSR_RD(dev, data->port_no,
                   LAN80XX_IOREG(MMD_ID_PTP_LTC, 1, LAN80XX_PTP_LTC_PTP_CUR_SEC_LSB),
                   &value);
    ts->seconds.low = (value & 0xffffffff);

    LAN80XX_CSR_RD(dev, data->port_no,
                   LAN80XX_IOREG(MMD_ID_PTP_LTC, 1, LAN80XX_PTP_LTC_PTP_CUR_SEC_MSB),
                   &value);
    ts->seconds.high = (value & 0xffff) ;

    LAN80XX_CSR_RD(dev, data->port_no,
                   LAN80XX_IOREG(MMD_ID_PTP_LTC, 1, LAN80XX_PTP_LTC_PTP_CUR_NSEC),
                   &value);
    ts->nanoseconds = (value & 0x3fffffff);

    LAN80XX_CSR_RD(dev, data->port_no,
                   LAN80XX_IOREG(MMD_ID_PTP_LTC, 1, LAN80XX_PTP_LTC_PTP_CUR_NSEC_FRAC),
                   &value);
    ts->picoseconds = (value & 0xff);

    return MEPA_RC_OK;
}

static mepa_rc lan80xx_ts_ltc_set(mepa_device_t *dev, const mepa_timestamp_t *const ts)
{
    phy25g_phy_state_t *data = (phy25g_phy_state_t *)dev->data;
    phy25g_phy_timestamp_t m25gts;
    mepa_rc rc = MEPA_RC_ERROR;

    m25gts.seconds.high = ts->seconds.high;
    m25gts.seconds.low = ts->seconds.low;
    m25gts.nanoseconds = ts->nanoseconds;
    m25gts.subnanoseconds  = ts->picoseconds;
    MEPA_ENTER(dev);
    rc = lan80xx_ts_csr_ptptime_set_priv(dev, data->port_no, &m25gts);
    MEPA_EXIT(dev);

    return rc;
}

static void lan80xx_phy_ts_fifo_read_cb(mepa_device_t  *dev,
                                        const mepa_port_no_t           port_no,
                                        const phy25g_phy_timestamp_t     *const fifo_ts,
                                        const phy25g_ts_fifo_sig_t   *const sig,
                                        void                           *cntxt,
                                        const phy25g_ts_fifo_status_t status)
{
    mepa_ts_fifo_sig_t mep_sig;
    mepa_timestamp_t ts;
    ts.seconds.high = fifo_ts->seconds.high;
    ts.seconds.low  = fifo_ts->seconds.low;
    ts.nanoseconds  = fifo_ts->nanoseconds;
    mep_sig.msg_type = sig->msg_type;
    mep_sig.domain_num = sig->domain_num;
    mep_sig.sequence_id = sig->sequence_id;
    memcpy(mep_sig.src_port_identity, sig->src_port_identity, sizeof(mep_sig.src_port_identity));
    mep_sig.has_crc_src = FALSE;
    mep_sig.crc_src_port = 0;
    if (fifo_cb != NULL) {
        fifo_cb(port_no, &ts, &mep_sig, (mepa_ts_fifo_status_t)status);
    }
}

static void lan80xx_ts_fifo_read_install(mepa_device_t  *dev, mepa_ts_fifo_read_t rd_cb)
{
    fifo_cb = rd_cb ? rd_cb : fifo_cb;
    lan80xx_phy_ts_fifo_read_install(dev, lan80xx_phy_ts_fifo_read_cb, NULL);
}

static mepa_rc lan80xx_ts_fifo_empty(mepa_device_t  *dev)
{
    phy25g_phy_state_t *data = (phy25g_phy_state_t *)dev->data;
    return lan80xx_phy_ts_fifo_empty(dev, data->port_no, NULL, NULL, TRUE);
}

static mepa_rc lan80xx_ts_fifo_get(mepa_device_t *dev, mepa_fifo_ts_entry_t ts_list[], const size_t size, uint32_t *const num)
{
    phy25g_phy_state_t *data = (phy25g_phy_state_t *)dev->data;
    phy25g_ts_fifo_entry_t phy25g_ts_entry[LAN80XX_PHY_TS_FIFO_MAX_ENTRIES];
    u16 FifoEntryIndex = 0;

    if (num == NULL) {
        T_E(MEPA_TRACE_GRP_GEN, "Invalid argument\n");
        return MEPA_RC_ERROR;
    }


    if (size < LAN80XX_PHY_TS_FIFO_MAX_ENTRIES) {
        T_E(MEPA_TRACE_GRP_GEN, "Size of Input TS list is less than 16\n");
        return MEPA_RC_ERROR;
    }

    if (lan80xx_phy_ts_fifo_get(dev, data->port_no, phy25g_ts_entry, LAN80XX_PHY_TS_FIFO_MAX_ENTRIES, num) == MEPA_RC_OK) {
        for (FifoEntryIndex = 0; FifoEntryIndex < *num; FifoEntryIndex++) {
            ts_list[FifoEntryIndex].ts.seconds.high = phy25g_ts_entry[FifoEntryIndex].ts.seconds.high;
            ts_list[FifoEntryIndex].ts.seconds.low = phy25g_ts_entry[FifoEntryIndex].ts.seconds.low;
            ts_list[FifoEntryIndex].ts.nanoseconds = phy25g_ts_entry[FifoEntryIndex].ts.nanoseconds;
            ts_list[FifoEntryIndex].sig.msg_type =   phy25g_ts_entry[FifoEntryIndex].sig.msg_type;
            ts_list[FifoEntryIndex].sig.domain_num = phy25g_ts_entry[FifoEntryIndex].sig.domain_num;
            memcpy(ts_list[FifoEntryIndex].sig.src_port_identity, phy25g_ts_entry[FifoEntryIndex].sig.src_port_identity, sizeof(ts_list[FifoEntryIndex].sig.src_port_identity));
            ts_list[FifoEntryIndex].sig.sequence_id = phy25g_ts_entry[FifoEntryIndex].sig.sequence_id;
            ts_list[FifoEntryIndex].sig.has_crc_src = FALSE;
        }
    } else {
        T_E(MEPA_TRACE_GRP_GEN, "Failed to get FIFO entries!!\n");
        return MEPA_RC_ERROR;
    }
    return MEPA_RC_OK;
}


static mepa_rc lan80xx_ts_clock_path_delay_get(mepa_device_t *dev, mepa_timeinterval_t *const path_delay)
{
    phy25g_phy_state_t *data = (phy25g_phy_state_t *)dev->data;
    return lan80xx_phy_ts_path_delay_get(dev, data->port_no, path_delay);
}

static mepa_rc lan80xx_ts_clock_path_delay_set(mepa_device_t *dev, const mepa_timeinterval_t *const path_delay)
{
    phy25g_phy_state_t *data = (phy25g_phy_state_t *)dev->data;
    return lan80xx_phy_ts_path_delay_set(dev, data->port_no, path_delay);
}


static mepa_rc lan80xx_ts_clock_delay_asymmetry_get(mepa_device_t *dev, mepa_timeinterval_t *const delay_asym)
{
    phy25g_phy_state_t *data = (phy25g_phy_state_t *)dev->data;
    return lan80xx_phy_ts_delay_asymmetry_get(dev, data->port_no, delay_asym);
}

static mepa_rc lan80xx_ts_clock_delay_asymmetry_set(mepa_device_t *dev, const mepa_timeinterval_t *const delay_asym)
{
    phy25g_phy_state_t *data = (phy25g_phy_state_t *)dev->data;
    return lan80xx_phy_ts_delay_asymmetry_set(dev, data->port_no, delay_asym);
}

static mepa_rc lan80xx_ts_clock_rateadj_get(mepa_device_t *dev, mepa_ts_scaled_ppb_t *const adj)
{
    phy25g_phy_state_t *data = (phy25g_phy_state_t *)dev->data;
    return lan80xx_phy_ts_clock_rateadj_get(dev, data->port_no, adj);
}

static mepa_rc lan80xx_ts_clock_rateadj_set(mepa_device_t *dev, const mepa_ts_scaled_ppb_t *const adj)
{
    phy25g_phy_state_t *data = (phy25g_phy_state_t *)dev->data;
    return lan80xx_phy_ts_clock_rateadj_set(dev, data->port_no, adj);
}

static mepa_rc lan80xx_ts_clock_egress_latency_get(mepa_device_t *dev, mepa_timeinterval_t *const latency)
{
    phy25g_phy_state_t *data = (phy25g_phy_state_t *)dev->data;
    return lan80xx_phy_ts_egress_latency_get(dev, data->port_no, latency);
}


static mepa_rc lan80xx_ts_clock_egress_latency_set(mepa_device_t *dev, const mepa_timeinterval_t *const latency)
{
    phy25g_phy_state_t *data = (phy25g_phy_state_t *)dev->data;
    return lan80xx_phy_ts_egress_latency_set(dev, data->port_no, latency);
}

static mepa_rc lan80xx_ts_clock_ingress_latency_get(mepa_device_t *dev, mepa_timeinterval_t *const latency)
{
    phy25g_phy_state_t *data = (phy25g_phy_state_t *)dev->data;
    return lan80xx_phy_ts_ingress_latency_get(dev, data->port_no, latency);
}

static mepa_rc lan80xx_ts_clock_ingress_latency_set(mepa_device_t *dev, const mepa_timeinterval_t *const latency)
{
    phy25g_phy_state_t *data = (phy25g_phy_state_t *)dev->data;
    return lan80xx_phy_ts_ingress_latency_set(dev, data->port_no, latency);
}



#if 0
static mepa_rc lan80xx_ts_clock_adj1ns(mepa_device_t *dev, const mepa_bool_t incr)
{
    phy25g_phy_state_t *data = (phy25g_phy_state_t *)dev->data;

    return lan80xx_phy_ts_ptptime_adj1ns(dev, data->port_no, incr);
}



static mepa_rc lan80xx_ts_pps_conf_set(mepa_device_t *dev, const mepa_ts_pps_conf_t *const phy_pps_conf)
{

    phy25g_ts_pps_conf_t pps_conf;

    pps_conf.pps_pulse_width = phy_pps_conf->pps_width_adj;
    pps_conf.pps_pulse_interval = phy_pps_conf->pps_offset;
//    return lan80xx_phy_ts_pps_conf_set(dev, &pps_conf);
}



static mepa_rc lan80xx_ts_pps_conf_get (mepa_device_t *dev, mepa_ts_pps_conf_t *const phy_pps_conf)
{

    phy25g_ts_pps_conf_t pps_conf;
    mesa_rc rc;

    if ((rc =  lan80xx_phy_ts_pps_conf_get(dev, &pps_conf)) == MEPA_RC_OK) {
        phy_pps_conf->pps_width_adj = pps_conf.pps_pulse_width;
    }
    return rc;
}
#endif

static mepa_rc lan80xx_ts_stats_get(mepa_device_t *dev, mepa_ts_stats_t *const statistics)
{
    phy25g_phy_state_t *data = (phy25g_phy_state_t *)dev->data;
    phy25g_phy_ts_stats_t stats;
    mesa_rc rc;

    if ((rc = lan80xx_phy_ts_stats_get(dev, data->port_no, &stats)) == MEPA_RC_OK) {
        statistics->ingr_pream_shrink_err = stats.ingr_pream_shrink_err;
        statistics->egr_pream_shrink_err = stats.egr_pream_shrink_err;
        statistics->ingr_fcs_err         = stats.ingr_fcs_err;
        statistics->egr_fcs_err          = stats.egr_fcs_err;
        statistics->ingr_frm_mod_cnt     = stats.ingr_frm_mod_cnt;
        statistics->egr_frm_mod_cnt      = stats.egr_frm_mod_cnt;
        statistics->ts_fifo_tx_cnt       = stats.ts_fifo_tx_cnt;
        statistics->ts_fifo_drop_cnt     = stats.ts_fifo_drop_cnt;
        return MEPA_RC_OK;
    } else {
        return MEPA_RC_ERROR;
    }
}


static mepa_rc lan80xx_ts_event_get(mepa_device_t *dev, mepa_ts_event_t *const ev_mask)
{
    phy25g_phy_state_t *data = (phy25g_phy_state_t *)dev->data;

    return lan80xx_phy_ts_event_enable_get(dev, data->port_no, ev_mask);
}


static mepa_rc lan80xx_ts_event_set(mepa_device_t *dev, const mepa_bool_t enable, const mepa_ts_event_t ev_mask)
{
    phy25g_phy_state_t *data = (phy25g_phy_state_t *)dev->data;


    return lan80xx_phy_ts_event_enable_set(dev, data->port_no, enable, ev_mask);


}


static mepa_rc lan80xx_ts_event_poll(mepa_device_t *dev, mepa_ts_event_t  *const status)
{
    phy25g_phy_state_t *data = (phy25g_phy_state_t *)dev->data;
    return lan80xx_phy_ts_event_poll(dev, data->port_no, status);

}

mepa_ts_driver_t lan80xx_ts_drivers = {
    .mepa_ts_reset                      = lan80xx_ts_reset,
    .mepa_ts_init_conf_get              = lan80xx_ts_init_conf_get,
    .mepa_ts_init_conf_set              = lan80xx_ts_init_conf_set,
    .mepa_ts_mode_get                   = lan80xx_ts_mode_get,
    .mepa_ts_mode_set                   = lan80xx_ts_mode_set,
    .mepa_ts_tx_classifier_conf_set     = lan80xx_ts_tx_classifier_conf_set,
    .mepa_ts_tx_classifier_conf_get     = lan80xx_ts_tx_classifier_conf_get,
    .mepa_ts_rx_classifier_conf_set     = lan80xx_ts_rx_classifier_conf_set,
    .mepa_ts_rx_classifier_conf_get     = lan80xx_ts_rx_classifier_conf_get,
    .mepa_ts_tx_clock_conf_set          = lan80xx_ts_tx_clock_conf_set,
    .mepa_ts_tx_clock_conf_get          = lan80xx_ts_tx_clock_conf_get,
    .mepa_ts_rx_clock_conf_set          = lan80xx_ts_rx_clock_conf_set,
    .mepa_ts_rx_clock_conf_get          = lan80xx_ts_rx_clock_conf_get,
    .mepa_ts_fifo_read_install          = lan80xx_ts_fifo_read_install,
    .mepa_ts_fifo_empty                 = lan80xx_ts_fifo_empty,
    .mepa_ts_fifo_get                   = lan80xx_ts_fifo_get,
    .mepa_ts_ltc_get                    = lan80xx_ts_ltc_get,
    .mepa_ts_ltc_set                    = lan80xx_ts_ltc_set,
    .mepa_ts_ltc_ls_en                  = lan80xx_ts_ltc_ls_en,
    .mepa_ts_delay_asymmetry_get        = lan80xx_ts_clock_delay_asymmetry_get,
    .mepa_ts_delay_asymmetry_set        = lan80xx_ts_clock_delay_asymmetry_set,
    .mepa_ts_path_delay_get             = lan80xx_ts_clock_path_delay_get,
    .mepa_ts_path_delay_set             = lan80xx_ts_clock_path_delay_set,
    .mepa_ts_clock_rateadj_get          = lan80xx_ts_clock_rateadj_get,
    .mepa_ts_clock_rateadj_set          = lan80xx_ts_clock_rateadj_set,
    .mepa_ts_egress_latency_get         = lan80xx_ts_clock_egress_latency_get,
    .mepa_ts_egress_latency_set         = lan80xx_ts_clock_egress_latency_set,
    .mepa_ts_ingress_latency_get        = lan80xx_ts_clock_ingress_latency_get,
    .mepa_ts_ingress_latency_set        = lan80xx_ts_clock_ingress_latency_set,
//   .mepa_ts_clock_adj1ns               = lan80xx_ts_clock_adj1ns
//    .mepa_ts_pps_conf_get               = lan80xx_ts_pps_conf_get,
//    .mepa_ts_pps_conf_set               = lan80xx_ts_pps_conf_set,
    .mepa_ts_stats_get                  = lan80xx_ts_stats_get,
    .mepa_ts_event_get                  = lan80xx_ts_event_get,
    .mepa_ts_event_set                  = lan80xx_ts_event_set,
    .mepa_ts_event_poll                 = lan80xx_ts_event_poll,
};



