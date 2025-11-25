// Copyright (c) 2004-2020 Microchip Technology Inc. and its subsidiaries.
// SPDX-License-Identifier: MIT

#include <stdio.h>
#include <unistd.h>
#include <ctype.h>
#include "microchip/ethernet/switch/api.h"
#include "microchip/ethernet/board/api.h"
#include "main.h"
#include "trace.h"
#include "cli.h"
#include "port.h"
#include "phy_demo_apps.h"
#include "mesa-rpc.h"
#include <mepa_apps/phy_ts_demo.h>


#define MAX_PRTS    20                   /* Number of Ports in EDSx */
#define TXT_DISABLED 0
#define TXT_ENABLED  1
#define TXT_BYPASSED 2
#define TXT_NOT_APP  3
#define TXT_NOT_TRIG 4
#define TXT_TRIG     5

#define PRINT_SIG_FIELD(flag, desc) \
    if (sig_mask & (flag)) {        \
        cli_printf("%d) %s\n", count++, (desc)); \
    }

static uint8_t packet_count = 0;


meba_inst_t meba_ts_instance;
ts_keyword_parsed ts_keyword;

static mscc_appl_trace_module_t trace_module = {
    .name = "phy_ts"
};

enum {
    TRACE_GROUP_DEFAULT,
    TRACE_GROUP_CNT
};

static mscc_appl_trace_group_t trace_groups[10] = {
    {
        .name = "default",
        .level = MESA_TRACE_LEVEL_ERROR
    },
};

/**
 * \brief   : Status of TS Block on all ports
 * \command : "ts port_state"
 */
static void cli_cmd_ts_port_state(cli_req_t *req);

/**
 * \brief   : Enables TS Block for given ports
 * \command : "ts_enable"
 */
static void cli_cmd_ts_ena(cli_req_t *req);

/**
 * \brief   : Disables TS Block for given ports
 * \command : "ts_disable"
 */
static void cli_cmd_ts_dis(cli_req_t *req);

/**
 * \brief   : Configures TS Block Tx classifier for given ports
 * \command : "tx_class_conf"
 */
static void cli_cmd_ts_tx_class_conf(cli_req_t *req);

/**
 * \brief   : Configures TS Block Tx clock for given ports
 * \command : "tx_clock_conf"
 */
static void cli_cmd_ts_tx_clock_conf(cli_req_t *req);

/**
 * \brief   : Configures TS Block Rx classifier for given ports
 * \command : "Rx_class_conf"
 */
static void cli_cmd_ts_rx_class_conf(cli_req_t *req);

/**
 * \brief   : Configures TS Block Rx clock for given ports
 * \command : "Rx_clock_conf"
 */
static void cli_cmd_ts_rx_clock_conf(cli_req_t *req);


static int cli_cmd_parse_tc_op (cli_req_t *req)
{
    ts_configuration *mreq = req->module_req;

    if (ts_keyword.tc_op_parsed == 1) {
        uint8_t tc_op = atoi (req->cmd);

        switch (tc_op) {
        case 0:
            mreq->tc_op_mode = MEPA_TS_TC_OP_MODE_A;
            break;

        case 1:
            mreq->tc_op_mode = MEPA_TS_TC_OP_MODE_B;
            break;

        case 2:
            mreq->tc_op_mode = MEPA_TS_TC_OP_MODE_C;
            break;

        default:
            cli_printf("\n Invalid input\n");
            return 1;
        }
        ts_keyword.tc_op_parsed = 0;
    }
    return 0;
}

static int cli_cmd_parse_tx_fifo (cli_req_t *req)
{
    ts_configuration *mreq = req->module_req;

    if (ts_keyword.tx_fifo_parsed == 1) {
        uint8_t tx_fifo = atoi (req->cmd);

        switch (tx_fifo) {
        case 0:
            mreq->tx_fifo_mode = MEPA_TS_FIFO_MODE_NORMAL;
            break;

        case 1:
            mreq->tx_fifo_mode = MEPA_TS_FIFO_MODE_SPI;
            break;

        default:
            cli_printf("\n Invalid input\n");
            return 1;
        }
        ts_keyword.tx_fifo_parsed = 0;
    }
    return 0;
}
static int cli_cmd_parse_clksrc (cli_req_t *req)
{
    ts_configuration *mreq = req->module_req;

    if (ts_keyword.clk_src_parsed == 1) {
        uint8_t clk_src = atoi (req->cmd);

        switch (clk_src) {
        case 0:
            mreq->clk_src = MEPA_TS_CLOCK_SRC_INTERNAL;
            break;

        case 1:
            mreq->clk_src = MEPA_TS_CLOCK_SRC_FROM_RX_PORT0;
            break;

        case 2:
            mreq->clk_src = MEPA_TS_CLOCK_SRC_FROM_RX_PORT1;
            break;

        case 3:
            mreq->clk_src = MEPA_TS_CLOCK_SRC_FROM_RX_PORT2;
            break;

        case 4:
            mreq->clk_src = MEPA_TS_CLOCK_SRC_FROM_RX_PORT3;
            break;

        case 5:
            mreq->clk_src = MEPA_TS_CLOCK_SRC_EXTERNAL;
            break;

        default:
            cli_printf("\n Invalid input\n");
            return 1;
        }
        ts_keyword.clk_src_parsed = 0;
    }
    return 0;
}

static int cli_cmd_parse_encap (cli_req_t *req)
{
    ts_configuration *mreq = req->module_req;

    if (ts_keyword.encap_type_parsed == 1) {
        uint8_t encap_type = atoi (req->cmd);
        if (encap_type >= 3) {
            encap_type = (encap_type + 1);
        }
        mreq->encap_type = encap_type;

        ts_keyword.encap_type_parsed = 0;
    }
    return 0;
}

static int cli_cmd_parse_floating_point (cli_req_t *req)
{
    ts_configuration *mreq = req->module_req;

    if (ts_keyword.delay_parsed == 1) {
        char *endPtr;
        mreq->delay = strtod(req->cmd, &endPtr);
        ts_keyword.delay_parsed = 0;
    } else if (ts_keyword.rateadj_parsed == 1) {
        char *endPtr;
        mreq->rateadj_ppb = strtod(req->cmd, &endPtr);
        ts_keyword.rateadj_parsed = 0;
    }
    return 0;
}

static int cli_cmd_parse_ltc_time (cli_req_t *req)
{
    ts_configuration *mreq = req->module_req;

    if (ts_keyword.ltc_time_parsed == 1) {
        ts_keyword.ltc_time_parsed = 0;
        if (sscanf(req->cmd, "%hu:%u:%u:%hhu", &mreq->sec_high, &mreq->sec_low, &mreq->nanoseconds, &mreq->picoseconds) != 4) {
            cli_printf("\n Invalid Format");
            return 0;
        }
    } else if (ts_keyword.delta_adj_parsed == 1) {
        ts_keyword.delta_adj_parsed = 0;
        if (sscanf(req->cmd, "%u:%hhu", &mreq->nanoseconds, &mreq->picoseconds) != 2) {
            cli_printf("\n Invalid Format");
            return 0;
        }
    }
    return 0;
}

static int cli_cmd_parse_keyword(cli_req_t *req)
{
    if (!strncasecmp(req->cmd, KEYWORD_CLOCK_SOURCE, strlen(req->cmd))) {
        ts_keyword.clk_src_parsed = 1;
    } else if (!strncasecmp(req->cmd, KEYWORD_TX_FIFO_MODE, strlen(req->cmd))) {
        ts_keyword.tx_fifo_parsed = 1;
    } else if (!strncasecmp(req->cmd, KEYWORD_TC_OP_MODE, strlen(req->cmd))) {
        ts_keyword.tc_op_parsed = 1;
    } else if (!strncasecmp(req->cmd, KEYWORD_TC_OP_MODE, strlen(req->cmd))) {
        ts_keyword.tc_op_parsed = 1;
    } else if (!strncasecmp(req->cmd, KEYWORD_DLY_REQ_10B, strlen(req->cmd))) {
        ts_keyword.dly_req_10b_parsed = 1;
    } else if (!strncasecmp(req->cmd, KEYWORD_TX_AUTO_FOLLOWUP, strlen(req->cmd))) {
        ts_keyword.tx_auto_f_parsed = 1;
    } else if (!strncasecmp(req->cmd, KEYWORD_MCH_EN, strlen(req->cmd))) {
        ts_keyword.mch_en_parsed = 1;
    } else if (!strncasecmp(req->cmd, KEYWORD_ENCAP_TYPE, strlen(req->cmd))) {
        ts_keyword.encap_type_parsed = 1;
    } else if (!strncasecmp(req->cmd, KEYWORD_CLOCK_ID, strlen(req->cmd))) {
        ts_keyword.clk_id_parsed = 1;
    } else if (!strncasecmp(req->cmd, KEYWORD_CLOCK_MODE, strlen(req->cmd))) {
        ts_keyword.clk_mode_parsed = 1;
    } else if (!strncasecmp(req->cmd, KEYWORD_DELAY_TYPE, strlen(req->cmd))) {
        ts_keyword.delaym_type_parsed = 1;
    } else if (!strncasecmp(req->cmd, KEYWORD_LTC_TIME, strlen(req->cmd))) {
        ts_keyword.ltc_time_parsed = 1;
    } else if (!strncasecmp(req->cmd, KEYWORD_LTC_TIME, strlen(req->cmd))) {
        ts_keyword.ltc_time_parsed = 1;
    } else if (!strncasecmp(req->cmd, KEYWORD_DELAY_MODE, strlen(req->cmd))) {
        ts_keyword.delay_mode_parsed = 1;
    } else if (!strncasecmp(req->cmd, KEYWORD_DELAY, strlen(req->cmd))) {
        ts_keyword.delay_parsed = 1;
    } else if (!strncasecmp(req->cmd, KEYWORD_CONF_SEL, strlen(req->cmd))) {
        ts_keyword.conf_sel_parsed = 1;
    } else if (!strncasecmp(req->cmd, KEYWORD_RATEADJ, strlen(req->cmd))) {
        ts_keyword.rateadj_parsed = 1;
    } else if (!strncasecmp(req->cmd, KEYWORD_ACTION, strlen(req->cmd))) {
        ts_keyword.action_parsed = 1;
    } else if (!strncasecmp(req->cmd, KEYWORD_LS_CTRL_SEL, strlen(req->cmd))) {
        ts_keyword.ls_ctrl_sel_parsed = 1;
    } else if (!strncasecmp(req->cmd, KEYWORD_CLK_SEL, strlen(req->cmd))) {
        ts_keyword.clk_sel_parsed = 1;
    } else if (!strncasecmp(req->cmd, KEYWORD_PIN_SEL, strlen(req->cmd))) {
        ts_keyword.pin_sel_parsed = 1;
    } else if (!strncasecmp(req->cmd, KEYWORD_POL, strlen(req->cmd))) {
        ts_keyword.pol_parsed = 1;
    } else if (!strncasecmp(req->cmd, KEYWORD_SYNC_MODE, strlen(req->cmd))) {
        ts_keyword.sync_mode_parsed = 1;
    } else if (!strncasecmp(req->cmd, KEYWORD_NS_ENABLE, strlen(req->cmd))) {
        ts_keyword.ns_enable_parsed = 1;
    } else if (!strncasecmp(req->cmd, KEYWORD_PPS_WIDTH, strlen(req->cmd))) {
        ts_keyword.pps_width_parsed = 1;
    } else if (!strncasecmp(req->cmd, KEYWORD_PPS_INTERVAL, strlen(req->cmd))) {
        ts_keyword.pps_interval_parsed = 1;
    } else if (!strncasecmp(req->cmd, KEYWORD_WFH_PERIOD, strlen(req->cmd))) {
        ts_keyword.wfh_parsed = 1;
    } else if (!strncasecmp(req->cmd, KEYWORD_WFL_PERIOD, strlen(req->cmd))) {
        ts_keyword.wfl_parsed = 1;
    } else if (!strncasecmp(req->cmd, KEYWORD_DELTA_ADJ, strlen(req->cmd))) {
        ts_keyword.delta_adj_parsed = 1;
    } else if (!strncasecmp(req->cmd, KEYWORD_EPPS_DET_CFG, strlen(req->cmd))) {
        ts_keyword.epps_det_cfg_parsed = 1;
    } else if (!strncasecmp(req->cmd, KEYWORD_SIG_MASK, strlen(req->cmd))) {
        ts_keyword.sig_mask_parsed = 1;
    } else if (!strncasecmp(req->cmd, "out_mode", strlen(req->cmd))) {
        ts_keyword.output_mode_parsed = 1;
    } else if (!strncasecmp(req->cmd, "eng_id", strlen(req->cmd))) {
        ts_keyword.eng_idx_parsed = 1;
    } else if (!strncasecmp(req->cmd, KEYWORD_TS_MMD_ID, strlen(req->cmd))) {
        ts_keyword.mmd = 1;
    } else if (!strncasecmp(req->cmd, KEYWORD_TS_CSR_ADDR, strlen(req->cmd))) {
        ts_keyword.csr_addr = 1;
    } else if (!strncasecmp(req->cmd, KEYWORD_TS_CSR_VAL, strlen(req->cmd))) {
        ts_keyword.csr_value = 1;
    }else {
        return -1;
    }

    return 0;
}
static int cli_cmd_parse_u8_param(cli_req_t *req)
{
    uint8_t value;
    ts_configuration *mreq = req->module_req;

    if (ts_keyword.clk_mode_parsed == 1) {
        cli_parm_u8(req, &value, 0, MASK_8BIT);
        mreq->clk_mode = value;
        ts_keyword.clk_mode_parsed = 0;
    } else if (ts_keyword.delaym_type_parsed == 1) {
        cli_parm_u8(req, &value, 0, MASK_8BIT);
        mreq->delay_type = value;
        ts_keyword.delaym_type_parsed = 0;
    } else if (ts_keyword.delay_mode_parsed == 1) {
        cli_parm_u8(req, &value, 0, MASK_8BIT);
        mreq->delay_mode = value;
        ts_keyword.delay_mode_parsed = 0;
    } else if (ts_keyword.conf_sel_parsed == 1) {
        cli_parm_u8(req, &value, 0, MASK_8BIT);
        mreq->config = value;
        ts_keyword.conf_sel_parsed = 0;
    } else if (ts_keyword.action_parsed == 1) {
        cli_parm_u8(req, &value, 0, MASK_8BIT);
        mreq->action = value;
        ts_keyword.action_parsed = 0;
    } else if (ts_keyword.ls_ctrl_sel_parsed == 1) {
        cli_parm_u8(req, &value, 0, MASK_8BIT);
        mreq->ls_ctrl_sel = value;
        ts_keyword.ls_ctrl_sel_parsed = 0;
    } else if (ts_keyword.clk_sel_parsed == 1) {
        cli_parm_u8(req, &value, 0, MASK_8BIT);
        mreq->clk_select = value;
        ts_keyword.clk_sel_parsed = 0;
    } else if (ts_keyword.pin_sel_parsed == 1) {
        cli_parm_u8(req, &value, 0, MASK_8BIT);
        mreq->pin_select = value;
        ts_keyword.pin_sel_parsed = 0;
    } else if (ts_keyword.sync_mode_parsed == 1) {
        cli_parm_u8(req, &value, 0, MASK_8BIT);
        mreq->pin_sync_mode = value;
        ts_keyword.sync_mode_parsed = 0;
    } else if (ts_keyword.epps_det_cfg_parsed == 1) {
        cli_parm_u8(req, &value, 0, MASK_8BIT);
        mreq->epps_det_cfg = value;
        ts_keyword.epps_det_cfg_parsed = 0;
    } else if (ts_keyword.output_mode_parsed == 1) {
        cli_parm_u8(req, &value, 0, MASK_8BIT);
        mreq->output_mode = value;
        ts_keyword.output_mode_parsed = 0;
    } else {
        return -1;
    }

    return 0;
}
static int cli_cmd_parse_u16_param(cli_req_t *req)
{
    uint16_t value;
    ts_configuration *mreq = req->module_req;

    if (ts_keyword.eng_idx_parsed == 1) {
        cli_parm_u16(req, &value, 0, MASK_16BIT);
        mreq->eng_id = value;
        ts_keyword.eng_idx_parsed = 0;
    } else if (ts_keyword.clk_id_parsed == 1) {
        cli_parm_u16(req, &value, 0, MASK_16BIT);
        mreq->clk_id = value;
        ts_keyword.clk_id_parsed = 0;
    } else if (ts_keyword.sig_mask_parsed == 1) {
        cli_parm_u16(req, &value, 0, MASK_16BIT);
        mreq->sig_mask = value;
        ts_keyword.sig_mask_parsed = 0;
    } else if (ts_keyword.mmd == 1) {
        cli_parm_u16(req, &value, 0, MASK_16BIT);
        mreq->mmd = value;
        ts_keyword.mmd = 0;
    } else if (ts_keyword.csr_addr == 1) {
        cli_parm_u16(req, &value, 0, MASK_16BIT);
        mreq->csr_addr = value;
        ts_keyword.csr_addr = 0;
    } else {
        return -1;
    }

    return 0;
}
static int cli_cmd_parse_u32_param(cli_req_t *req)
{
    uint32_t value;
    ts_configuration *mreq = req->module_req;

    if (ts_keyword.pps_width_parsed == 1) {
        cli_parm_u32(req, &value, 0, MASK_32BIT);
        mreq->pps_width = value;
        ts_keyword.pps_width_parsed = 0;
    } else if (ts_keyword.pps_interval_parsed == 1) {
        cli_parm_u32(req, &value, 0, MASK_32BIT);
        mreq->pps_interval = value;
        ts_keyword.pps_interval_parsed = 0;
    } else if (ts_keyword.wfh_parsed == 1) {
        cli_parm_u32(req, &value, 0, MASK_32BIT);
        mreq->wfh_period = value;
        ts_keyword.wfh_parsed = 0;
    } else if (ts_keyword.wfl_parsed == 1) {
        cli_parm_u32(req, &value, 0, MASK_32BIT);
        mreq->wfl_period = value;
        ts_keyword.wfl_parsed = 0;
    } else if (ts_keyword.csr_value == 1) {
        cli_parm_u32(req, &value, 0, MASK_32BIT);
        mreq->csr_value = value;
        ts_keyword.csr_value = 0;
    } else {
        return -1;
    }
    return 0;
}
static int cli_cmd_parse_boolean(cli_req_t *req)
{
    ts_configuration *mreq = req->module_req;

    if (!strncasecmp(req->cmd, KEYWORD_LOAD, strlen(req->cmd))) {
        mreq->tod_load = 1;
        mreq->tod_save = 0;
    } else if (!strncasecmp(req->cmd, KEYWORD_SAVE, strlen(req->cmd))) {
        mreq->tod_load = 0;
        mreq->tod_save = 1;
    }
    uint8_t input = atoi(req->cmd);
    if ((input == 0) || (input == 1)) {
        if (ts_keyword.dly_req_10b_parsed) {
            mreq->dly_req_10b = input;
            ts_keyword.dly_req_10b_parsed = 0;
        } else if (ts_keyword.tx_auto_f_parsed) {
            mreq->tx_auto_f = input;
            ts_keyword.tx_auto_f_parsed = 0;
        } else if (ts_keyword.mch_en_parsed) {
            mreq->mch_en = input;
            ts_keyword.mch_en_parsed = 0;
        } else if (ts_keyword.pol_parsed) {
            mreq->pin_inv_pol = input;
            ts_keyword.pol_parsed = 0;
        } else if (ts_keyword.ns_enable_parsed) {
            mreq->ns_enable = input;
            ts_keyword.ns_enable_parsed = 0;
        }
    } else {
        T_E("Invalid Input");
        return 1;
    }

    return 0;
}

static int mepa_dev_check(meba_inst_t meba_instance, mepa_port_no_t port_no)
{
    if (!meba_instance->phy_devices[port_no]) {
        return MEPA_RC_ERROR;
    }
    return MEPA_RC_OK;
}
const char *cli_ts_capable_txt(mesa_bool_t enabled)
{
    return (enabled ? "Capable" : "NotCapable");
}

// KA - Check
const char *cli_ts_state_txt(int status)
{
    switch (status) {
    case 0:
        return "Disabled";
        break;
    case 1:
        return "Enabled";
        break;
    case 3:
        return "  --";
        break;
    }
    return 0;
}

static void test_ts_phy_fifo_read( const mepa_port_no_t           port_no,
                                   const mepa_timestamp_t     *const fifo_ts,
                                   const mepa_ts_fifo_sig_t   *const sig,
                                   const mepa_ts_fifo_status_t status)
{
    packet_count = packet_count + 1;

    const uint8_t zero_mac[6] = {0};
    const uint8_t zero_ip[4] = {0};

    mepa_bool_t is_dmac_zero = (memcmp(sig->dmac_addr.addr, zero_mac, sizeof(zero_mac)) == 0);

    mepa_bool_t is_ipv4_zero = (memcmp(sig->dest_ipv4, zero_ip, sizeof(zero_ip)) == 0);

    cli_printf("\n");
    cli_printf("PHY Fifo read: Packet : %d -> port_no %u, msg_type: %d, domain %d, seq %d, Source portId : 0x%x:%x:%x:%x:%x:%x:%x:%x:%x:%x\n", packet_count, (port_no + 1), sig->msg_type, sig->domain_num,
               sig->sequence_id, sig->src_port_identity[0], sig->src_port_identity[1], sig->src_port_identity[2], sig->src_port_identity[3], sig->src_port_identity[4], sig->src_port_identity[5],
               sig->src_port_identity[6], sig->src_port_identity[7], sig->src_port_identity[8], sig->src_port_identity[9]);

    if (sig->dmac_sig_supported == TRUE && is_ipv4_zero) {
        cli_printf("PHY Fifo read: Packet : %d -> DMAC : 0x%02x:0x%02x:0x%02x:0x%02x:0x%02x:0x%02x\n", packet_count, sig->dmac_addr.addr[0], sig->dmac_addr.addr[1], sig->dmac_addr.addr[2],
                   sig->dmac_addr.addr[3], sig->dmac_addr.addr[4], sig->dmac_addr.addr[5]);
    }

    if (sig->ipv4_sig_supported == TRUE && is_dmac_zero) {
        cli_printf("PHY Fifo read: Packet : %d -> Dest IPv4 : %d.%d.%d.%d\n", packet_count, sig->dest_ipv4[0], sig->dest_ipv4[1], sig->dest_ipv4[2], sig->dest_ipv4[3]);
    }
    cli_printf("PHY Fifo read: Packet : %d -> Dest IPv6(hex) : %02x%02x.%02x%02x.%02x%02x.%02x%02x.%02x%02x.%02x%02x.%02x%02x.%02x%02x\n", packet_count, sig->ipv6_dest_addr[15], sig->ipv6_dest_addr[14],
              sig->ipv6_dest_addr[13], sig->ipv6_dest_addr[12],sig->ipv6_dest_addr[11],sig->ipv6_dest_addr[10],sig->ipv6_dest_addr[9],sig->ipv6_dest_addr[8],sig->ipv6_dest_addr[7],sig->ipv6_dest_addr[6],
              sig->ipv6_dest_addr[5], sig->ipv6_dest_addr[4],sig->ipv6_dest_addr[3],sig->ipv6_dest_addr[2],sig->ipv6_dest_addr[1],sig->ipv6_dest_addr[0]);

    cli_printf("PHY Fifo read: Packet : %d -> tx time:  Sec_Hi:%d, Sec_Low:%u, Nsec: %u, sub-Nsec %u\n", packet_count, fifo_ts->seconds.high, fifo_ts->seconds.low, fifo_ts->nanoseconds, fifo_ts->picoseconds);
}

static void cli_cmd_ts_port_state(cli_req_t *req)
{
    mepa_rc rc;
    mepa_bool_t state;
    int status = 0;
    printf("\n");
    cli_printf("Port    Status");
    cli_printf("\n----------------------\n");
    printf("\n");

    mepa_port_no_t  port_no;

    for (int iport = 0; iport < MAX_PRTS; iport++) {
        port_no = iport2uport(iport);
        if ((rc = mepa_dev_check(meba_ts_instance, iport)) != MEPA_RC_OK) {
            status = TXT_NOT_APP;
            cli_printf("%-8u%s\n",
                       port_no,
                       cli_ts_state_txt(status));
            continue;
        }
        if ((rc = mepa_ts_mode_get(meba_ts_instance->phy_devices[iport], &state)) == MEPA_RC_OK) {
            status = state ? TXT_ENABLED : TXT_DISABLED;
        } else {
            status = TXT_DISABLED;
        }

        cli_printf("%-8u%s\n",
                   port_no,
                   cli_ts_state_txt(status));
    }
    return;
}

static int get_user_input(const char *prompt)
{
    char input[10];
    cli_printf("%s", prompt);
    if (fgets(input, sizeof(input), stdin) != NULL) {
        return atoi(input);
    }
    return -1; // Return -1 in case of an error
}

static void cli_cmd_ts_conf_init(cli_req_t *req)
{
    mepa_port_no_t  port_no;
    mepa_ts_init_conf_t ts_init_conf;
    ts_configuration *mreq = req->module_req;
    mepa_rc rc;


    for (int iport = 0; iport < MAX_PRTS; iport++) {
        port_no = iport2uport(iport);
        if (req->port_list[port_no] == 0) {
            continue;
        }
        if (!meba_ts_instance->phy_devices[iport]) {
            cli_printf(" Dev is Not Created for the port : %d\n", (iport + 1));
            return;
        }

        rc = mepa_ts_init_conf_get(meba_ts_instance->phy_devices[iport], &ts_init_conf);
        if (rc != MEPA_RC_OK) {
            cli_printf("\n Failed to get the Ts Port init on port : %d\n", (iport + 1));
            return;
        }

        // Update default configs
        ts_init_conf.clk_freq = MEPA_TS_CLOCK_FREQ_15625M;
        ts_init_conf.clk_src = mreq->clk_src;
        ts_init_conf.rx_ts_pos = MEPA_TS_RX_TIMESTAMP_POS_IN_PTP;
        ts_init_conf.rx_ts_len = MEPA_TS_RX_TIMESTAMP_LEN_30BIT;
        ts_init_conf.tx_fifo_mode = mreq->tx_fifo_mode;
        ts_init_conf.tx_ts_len = MEPA_TS_FIFO_TIMESTAMP_LEN_4BYTE;
        ts_init_conf.tx_fifo_spi_conf = 0;
        ts_init_conf.auto_clear_ls = 0;
        ts_init_conf.tc_op_mode = mreq->tc_op_mode;
        ts_init_conf.dly_req_recv_10byte_ts = mreq->dly_req_10b;
        ts_init_conf.tx_auto_followup_ts = mreq->tx_auto_f;
        ts_init_conf.mch_pch_conf.pch_en = 0;
        ts_init_conf.mch_pch_conf.save_ts_with_crc_err = 0;
        ts_init_conf.mch_pch_conf.mch_en = mreq->mch_en;
        ts_init_conf.mch_pch_conf.ts_len_ing = MEPA_MCH_TS_32NS_ONS;
        ts_init_conf.mch_pch_conf.ts_len_egr = MEPA_MCH_TS_32NS_ONS;


        if (MEPA_RC_OK == mepa_ts_init_conf_set(meba_ts_instance->phy_devices[iport], &ts_init_conf)) {
            cli_printf("\n ...... TS Block Configured on Port : %d......\n", (iport + 1));
        } else {
            cli_printf("\n ...... TS Block Configuration Failed on Port : %d......\n", (iport + 1));
        }

        /* Register callback to read TS FIFO */
        mepa_ts_fifo_read_t rd_cb = &test_ts_phy_fifo_read;
        if (mepa_ts_fifo_read_install(meba_ts_instance->phy_devices[iport], rd_cb) != MEPA_RC_OK) {
            cli_printf("\n Failed to register callback to read FIFO \n");
        }

        /* Enable TS FIFO Event for FIFO capture  in case of BC-2step mode */
        mepa_ts_event_t  event_mask;
        event_mask = MEPA_TS_EGR_TIMESTAMP_CAPTURED | MEPA_TS_EGR_FIFO_OVERFLOW;
        mepa_ts_event_set(meba_ts_instance->phy_devices[iport], TRUE, event_mask);

    }
    return;
}

// Helper function to trim whitespace
static char *trim(char *str)
{
    char *end;
    while (isspace((unsigned char)*str)) {
        str++;
    }
    if (*str == 0) {
        return str;
    }
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) {
        end--;
    }
    end[1] = '\0';
    return str;
}

// Helper function to map string to enum value
static int parse_enum(const char *value, const char *const *names, int count)
{
    for (int i = 0; i < count; ++i) {
        if (strcmp(value, names[i]) == 0) {
            return i;
        }
    }
    return -1;
}

static void parse_mac(const char *value, uint8_t mac[6])
{
    unsigned int b[6] = {0};
    sscanf(value, "%2x:%2x:%2x:%2x:%2x:%2x", &b[0], &b[1], &b[2], &b[3], &b[4], &b[5]);
    for (int i = 0; i < 6; ++i) {
        mac[i] = (uint8_t)b[i];
    }
}

static void parse_u32_array(const char *value, uint32_t *arr, int n)
{
    char buf[256];
    strncpy(buf, value, sizeof(buf));
    buf[sizeof(buf) - 1] = 0;
    char *token = strtok(buf, ",");
    int i = 0;
    while (token && i < n) {
        arr[i++] = (uint32_t)strtoul(token, NULL, 0);
        token = strtok(NULL, ",");
    }
    while (i < n) {
        arr[i++] = 0;
    }
}

const char *match_mode_names[] = {
    "MEPA_TS_MATCH_MODE_RANGE",
    "MEPA_TS_MATCH_MODE_VALUE"
};
const char *clk_mode_names[] = {
    "MEPA_TS_PTP_CLOCK_MODE_NONE",
    "MEPA_TS_PTP_CLOCK_MODE_BC1STEP",
    "MEPA_TS_PTP_CLOCK_MODE_BC2STEP",
    "MEPA_TS_PTP_CLOCK_MODE_TC1STEP",
    "MEPA_TS_PTP_CLOCK_MODE_TC2STEP"
};
const char *delaym_type_names[] = {
    "MEPA_TS_PTP_DELAYM_P2P",
    "MEPA_TS_PTP_DELAYM_E2E"
};
const char *pkt_encap_names[] = {
    "MEPA_TS_ENCAP_NONE",
    "MEPA_TS_ENCAP_ETH_PTP",
    "MEPA_TS_ENCAP_ETH_IP_PTP",
    "MEPA_TS_ENCAP_ETH_HSR_PTP",
    "MEPA_TS_ENCAP_ETH_IP_IP_PTP",
    "MEPA_TS_ENCAP_ETH_ETH_PTP",
    "MEPA_TS_ENCAP_ETH_ETH_IP_PTP",
    "MEPA_TS_ENCAP_ETH_MPLS_IP_PTP",
    "MEPA_TS_ENCAP_ETH_MPLS_ETH_PTP",
    "MEPA_TS_ENCAP_ETH_MPLS_ETH_IP_PTP"
};
const char *mac_match_mode_names[] = {
    "MEPA_TS_ETH_ADDR_MATCH_ANY",
    "MEPA_TS_ETH_ADDR_MATCH_48BIT",
    "MEPA_TS_ETH_ADDR_MATCH_ANY_UNICAST",
    "MEPA_TS_ETH_ADDR_MATCH_ANY_MULTICAST"
};
const char *mac_match_select_names[] = {
    "MEPA_TS_ETH_MATCH_NONE",
    "MEPA_TS_ETH_MATCH_DEST_ADDR",
    "MEPA_TS_ETH_MATCH_SRC_ADDR",
    "MEPA_TS_ETH_MATCH_SRC_OR_DEST"
};
const char *ip_ver_names[] = {
    "MEPA_TS_IP_VER_4",
    "MEPA_TS_IP_VER_6"
};
const char *ip_match_mode_names[] = {
    "MEPA_TS_IP_MATCH_NONE",
    "MEPA_TS_IP_MATCH_DEST",
    "MEPA_TS_IP_MATCH_SRC",
    "MEPA_TS_IP_MATCH_SRC_OR_DEST"
};

// Enum string tables
const char *clk_freq_names[] = {
    "MEPA_TS_CLOCK_FREQ_25M",
    "MEPA_TS_CLOCK_FREQ_125M",
    "MEPA_TS_CLOCK_FREQ_15625M",
    "MEPA_TS_CLOCK_FREQ_200M",
    "MEPA_TS_CLOCK_FREQ_250M",
    "MEPA_TS_CLOCK_FREQ_500M",
    "MEPA_TS_CLOCK_FREQ_MAX"
};
const char *clk_src_names[] = {
    "MEPA_TS_CLOCK_SRC_EXTERNAL",
    "MEPA_TS_CLOCK_SRC_CLIENT_RX",
    "MEPA_TS_CLOCK_SRC_CLIENT_TX",
    "MEPA_TS_CLOCK_SRC_INTERNAL",
    "MEPA_TS_CLOCK_SRC_125MHZ_INTERNAL_SYS_PLL",
    "MEPA_TS_CLOCK_SRC_125MHZ_QSGMII_REC_CLOCK",
    "MEPA_TS_CLOCK_SRC_EXT_1588_REF_CLOCK",
    "MEPA_TS_CLOCK_SRC_RESERVED",
    "MEPA_TS_CLOCK_SRC_FROM_RX_PORT0",
    "MEPA_TS_CLOCK_SRC_FROM_RX_PORT1",
    "MEPA_TS_CLOCK_SRC_FROM_RX_PORT2",
    "MEPA_TS_CLOCK_SRC_FROM_RX_PORT3"
};
const char *rx_ts_pos_names[] = {
    "MEPA_TS_RX_TIMESTAMP_POS_IN_PTP",
    "MEPA_TS_RX_TIMESTAMP_POS_AT_END"
};
const char *rx_ts_len_names[] = {
    "MEPA_TS_RX_TIMESTAMP_LEN_30BIT",
    "MEPA_TS_RX_TIMESTAMP_LEN_32BIT"
};
const char *tx_fifo_mode_names[] = {
    "MEPA_TS_FIFO_MODE_NORMAL",
    "MEPA_TS_FIFO_MODE_SPI"
};
const char *tx_ts_len_names[] = {
    "MEPA_TS_FIFO_TIMESTAMP_LEN_4BYTE",
    "MEPA_TS_FIFO_TIMESTAMP_LEN_10BYTE"
};
const char *tc_op_mode_names[] = {
    "MEPA_TS_TC_OP_MODE_A",
    "MEPA_TS_TC_OP_MODE_B",
    "MEPA_TS_TC_OP_MODE_C"
};
const char *ts_len_names[] = {
    "MEPA_MCH_TS_32NS_ONS",
    "MEPA_MCH_TS_28NS_4SNS",
    "MEPA_MCH_TS_24NS_8SNS",
    "MEPA_MCH_TS_16NS_16SNS"
};

// Main parser function
int parse_mepa_ts_init_conf(const char *filename, mepa_ts_init_conf_t *conf)
{
    FILE *f = fopen(filename, "r");
    if (!f) {
        return -1;
    }
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        char *trimmed = trim(line);
        if (trimmed[0] == '#' || trimmed[0] == '\0') {
            continue;
        }
        char *eq = strchr(trimmed, '=');
        if (!eq) {
            continue;
        }
        *eq = '\0';
        char *tag = trim(trimmed);
        char *value = trim(eq + 1);

        // Top-level fields
        if (strcmp(tag, "clk_freq") == 0) {
            int idx = parse_enum(value, clk_freq_names, sizeof(clk_freq_names) / sizeof(clk_freq_names[0]));
            if (idx >= 0) {
                conf->clk_freq = (mepa_ts_clock_freq_t)idx;
            }
        } else if (strcmp(tag, "clk_src") == 0) {
            int idx = parse_enum(value, clk_src_names, sizeof(clk_src_names) / sizeof(clk_src_names[0]));
            if (idx >= 0) {
                conf->clk_src = (mepa_ts_clock_src_t)idx;
            }
        } else if (strcmp(tag, "rx_ts_pos") == 0) {
            int idx = parse_enum(value, rx_ts_pos_names, sizeof(rx_ts_pos_names) / sizeof(rx_ts_pos_names[0]));
            if (idx >= 0) {
                conf->rx_ts_pos = (mepa_ts_rx_timestamp_pos_t)idx;
            }
        } else if (strcmp(tag, "rx_ts_len") == 0) {
            int idx = parse_enum(value, rx_ts_len_names, sizeof(rx_ts_len_names) / sizeof(rx_ts_len_names[0]));
            if (idx >= 0) {
                conf->rx_ts_len = (mepa_ts_rx_timestamp_len_t)idx;
            }
        } else if (strcmp(tag, "tx_fifo_mode") == 0) {
            int idx = parse_enum(value, tx_fifo_mode_names, sizeof(tx_fifo_mode_names) / sizeof(tx_fifo_mode_names[0]));
            if (idx >= 0) {
                conf->tx_fifo_mode = (mepa_ts_fifo_mode_t)idx;
            }
        } else if (strcmp(tag, "tx_ts_len") == 0) {
            int idx = parse_enum(value, tx_ts_len_names, sizeof(tx_ts_len_names) / sizeof(tx_ts_len_names[0]));
            if (idx >= 0) {
                conf->tx_ts_len = (mepa_ts_fifo_timestamp_len_t)idx;
            }
        } else if (strcmp(tag, "tx_fifo_spi_conf") == 0) {
            conf->tx_fifo_spi_conf = atoi(value) ? 1 : 0;
        } else if (strcmp(tag, "auto_clear_ls") == 0) {
            conf->auto_clear_ls = atoi(value) ? 1 : 0;
        } else if (strcmp(tag, "tc_op_mode") == 0) {
            int idx = parse_enum(value, tc_op_mode_names, sizeof(tc_op_mode_names) / sizeof(tc_op_mode_names[0]));
            if (idx >= 0) {
                conf->tc_op_mode = (mepa_ts_tc_op_mode_t)idx;
            }
        } else if (strcmp(tag, "dly_req_recv_10byte_ts") == 0) {
            conf->dly_req_recv_10byte_ts = atoi(value) ? 1 : 0;
        } else if (strcmp(tag, "tx_auto_followup_ts") == 0) {
            conf->tx_auto_followup_ts = atoi(value) ? 1 : 0;
        }
        // Nested mch_pch_conf fields
        else if (strcmp(tag, "mch_pch_conf.pch_en") == 0) {
            conf->mch_pch_conf.pch_en = atoi(value) ? 1 : 0;
        } else if (strcmp(tag, "mch_pch_conf.save_ts_with_crc_err") == 0) {
            conf->mch_pch_conf.save_ts_with_crc_err = atoi(value) ? 1 : 0;
        } else if (strcmp(tag, "mch_pch_conf.mch_en") == 0) {
            conf->mch_pch_conf.mch_en = atoi(value) ? 1 : 0;
        } else if (strcmp(tag, "mch_pch_conf.ts_len_ing") == 0) {
            int idx = parse_enum(value, ts_len_names, sizeof(ts_len_names) / sizeof(ts_len_names[0]));
            if (idx >= 0) {
                conf->mch_pch_conf.ts_len_ing = (ing_egr_ts_len_t)idx;
            }
        } else if (strcmp(tag, "mch_pch_conf.ts_len_egr") == 0) {
            int idx = parse_enum(value, ts_len_names, sizeof(ts_len_names) / sizeof(ts_len_names[0]));
            if (idx >= 0) {
                conf->mch_pch_conf.ts_len_egr = (ing_egr_ts_len_t)idx;
            }
        }
    }
    fclose(f);
    return 0;
}

#if 0
static void cli_cmd_ts_init_f(cli_req_t *req)
{
    mepa_port_no_t  port_no;
    mepa_ts_init_conf_t ts_init_conf;

    memset(&ts_init_conf, 0, sizeof(ts_init_conf));
    if (parse_mepa_ts_init_conf("/root/mepa_scripts/ts_init_conf.cfg", &ts_init_conf) == 0) {
        // conf is now filled with values from the config file
        for (int iport = 0; iport < MAX_PRTS; iport++) {
            port_no = iport2uport(iport);
            if (req->port_list[port_no] == 0) {
                continue;
            }
            if (!meba_ts_instance->phy_devices[iport]) {
                cli_printf(" Dev is Not Created for the port : %d\n", iport);
                return;
            }
            if (MEPA_RC_OK == mepa_ts_init_conf_set(meba_ts_instance->phy_devices[iport], &ts_init_conf)) {
                cli_printf("\n ...... TS Block Configured on Port : %d......\n", iport);
            } else {
                cli_printf("\n ...... TS Block Configuration Failed on Port : %d......\n", iport);
            }
        }
    } else {
        cli_printf("cfg file open failed\n");
    }
    return;
}
#endif

static void cli_cmd_ts_ena(cli_req_t *req)
{
    mepa_port_no_t  port_no;

    for (int iport = 0; iport < MAX_PRTS; iport++) {
        port_no = iport2uport(iport);
        if (req->port_list[port_no] == 0) {
            continue;
        }
        if (!meba_ts_instance->phy_devices[iport]) {
            cli_printf(" Dev is Not Created for the port : %d\n", (iport + 1));
            return;
        }
        if (MEPA_RC_OK == mepa_ts_mode_set(meba_ts_instance->phy_devices[iport], 1)) {
            cli_printf("\n ...... TS Block Enabled on Port : %d......\n", (iport + 1));
        } else {
            cli_printf("\n ...... TS Block Enable Failed on Port : %d......\n", (iport + 1));
        }
    }
    return;
}

static void cli_cmd_ts_dis(cli_req_t *req)
{
    mepa_port_no_t  port_no;

    for (int iport = 0; iport < MAX_PRTS; iport++) {
        port_no = iport2uport(iport);
        if (req->port_list[port_no] == 0) {
            continue;
        }
        if (!meba_ts_instance->phy_devices[iport]) {
            cli_printf(" Dev is Not Created for the port : %d\n", iport);
            return;
        }
        if (MEPA_RC_OK == mepa_ts_mode_set(meba_ts_instance->phy_devices[iport], 0)) {
            cli_printf("\n ...... TS Block Disabled on Port : %d......\n", iport);
        } else {
            cli_printf("\n ...... TS Block Disable Failed on Port : %d......\n", iport);
        }

    }
    return;
}

int parse_ipv6_address(const char *input, uint32_t *addr)
{
    unsigned short segments[8];
    if (sscanf(input, "%hx:%hx:%hx:%hx:%hx:%hx:%hx:%hx",
               &segments[0], &segments[1], &segments[2], &segments[3],
               &segments[4], &segments[5], &segments[6], &segments[7]) == 8) {
        addr[3] = (segments[0] << 16) | segments[1];
        addr[2] = (segments[2] << 16) | segments[3];
        addr[1] = (segments[4] << 16) | segments[5];
        addr[0] = (segments[6] << 16) | segments[7];
        return 1;
    }
    return 0;
}

static int parse_ipv4_address(const char *input, uint32_t *addr)
{
    unsigned char octets[4];
    if (sscanf(input, "%hhu.%hhu.%hhu.%hhu", &octets[0], &octets[1], &octets[2], &octets[3]) == 4) {
        *addr = (octets[0] << 24) | (octets[1] << 16) | (octets[2] << 8) | octets[3];
        return 1;
    }
    return 0;
}

static int parse_mac_address(const char *input, unsigned char *octets)
{
    return sscanf(input, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
                  &octets[0], &octets[1], &octets[2], &octets[3], &octets[4], &octets[5]) == 6;
}

static void ethtype_value_update(uint16_t encap_type, uint16_t ip_ver, uint16_t num_tag,
                                 uint16_t *eth1_ethtype, uint16_t *eth2_ethtype)
{
    switch (encap_type) {
    case MEPA_TS_ENCAP_ETH_PTP:
        *eth1_ethtype = 0x88f7;
        *eth2_ethtype = 0x0;
        break;
    case MEPA_TS_ENCAP_ETH_IP_PTP:
        *eth1_ethtype = ip_ver ? 0x86dd : 0x800;
        *eth2_ethtype = 0x0;
        break;
    case MEPA_TS_ENCAP_ETH_HSR_PTP:
        *eth1_ethtype = 0x0;
        *eth2_ethtype = 0x0;
        break;
    case MEPA_TS_ENCAP_ETH_IP_IP_PTP:
        *eth1_ethtype = ip_ver ? 0x86dd : 0x800;
        *eth2_ethtype = 0x0;
        break;
    case MEPA_TS_ENCAP_ETH_ETH_PTP:
        if (num_tag == 1) {
            *eth1_ethtype = 0x88e7; /* I-Tag */
        } else if (num_tag == 2) {
            *eth1_ethtype = 0x88a8; /* B-Tag */
        }
        *eth2_ethtype = 0x88f7;
        break;
    case MEPA_TS_ENCAP_ETH_ETH_IP_PTP:
        if (num_tag == 1) {
            *eth1_ethtype = 0x88e7; /* I-Tag */
        } else if (num_tag == 2) {
            *eth1_ethtype = 0x88a8; /* B-Tag */
        }
        *eth2_ethtype = ip_ver ? 0x86dd : 0x800;
        break;
    case MEPA_TS_ENCAP_ETH_MPLS_IP_PTP:
        *eth1_ethtype = 0x8847;
        *eth2_ethtype = 0x0;
        break;
    case MEPA_TS_ENCAP_ETH_MPLS_ETH_PTP:
        *eth1_ethtype = 0x8847;
        *eth2_ethtype = 0x88f7;
        break;
    case MEPA_TS_ENCAP_ETH_MPLS_ETH_IP_PTP:
        *eth1_ethtype = 0x8847;
        *eth2_ethtype = ip_ver ? 0x86dd : 0x800;
        break;
    default:
        *eth1_ethtype = 0x0;
        *eth2_ethtype = 0x0;
        break;
    }
    return;
}

static int update_ts_classifier_encap(mepa_ts_classifier_t *ts_classifier, uint16_t encap_type)
{
    uint16_t mac_match_mode = 0, vlan_check = 0, num_tag = 0, ip_ver = 0, nheaderreq = 1;
    uint16_t eth1_ethtype = 0, eth2_ethtype = 0, ip_match = 0;
    char input[100];
    uint32_t ipv6_mask[4] = {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF};

    if ((encap_type == MEPA_TS_ENCAP_ETH_ETH_PTP) || (encap_type == MEPA_TS_ENCAP_ETH_ETH_IP_PTP) ||
        (encap_type == MEPA_TS_ENCAP_ETH_MPLS_ETH_PTP) || (encap_type == MEPA_TS_ENCAP_ETH_MPLS_ETH_IP_PTP)) {
        nheaderreq = 2;
    }


    for (uint i = 1; i <= nheaderreq; i++) {
        if (i == 1) {
            cli_printf("\n ------------------------------------------ \n");
            cli_printf("\n First Layer Ethernet Header Configuration \n");
            cli_printf("\n ------------------------------------------ \n");
        } else if (i == 2) {
            cli_printf("\n ------------------------------------------- \n");
            cli_printf("\n Second Layer Ethernet Header Configuration \n");
            cli_printf("\n ------------------------------------------- \n");
        }

        // Get MAC Match Mode
        mac_match_mode = get_user_input("\n Match PTP Packet MAC types [0 : Any, 1 : 48 Bit, 2: Any Unicast, 3: Any Multicast]: ");
        if (mac_match_mode >= 0 && mac_match_mode <= 3) {
            // 0 - MEPA_TS_ETH_ADDR_MATCH_ANY,
            // 1 - MEPA_TS_ETH_ADDR_MATCH_48BIT,
            // 2 - MEPA_TS_ETH_ADDR_MATCH_ANY_UNICAST,
            // 3 - MEPA_TS_ETH_ADDR_MATCH_ANY_MULTICAST,

            if (i == 2) {
                ts_classifier->eth2_class_conf.mac_match_mode = mac_match_mode;
            } else {
                ts_classifier->eth_class_conf.mac_match_mode = mac_match_mode;
            }

        } else {
            cli_printf("\n Invalid input for MAC Match Mode");
            return -1;
        }

        if (mac_match_mode == 1) {
            cli_printf("\n Enter MAC address (Format- xx:xx:xx:xx:xx:xx): ");
            if (fgets(input, sizeof(input), stdin) != NULL) {
                if (i == 2) {
                    if (!parse_mac_address(input, ts_classifier->eth2_class_conf.mac_addr)) {
                        cli_printf("\n Invalid MAC address format");
                    }
                } else {
                    if (!parse_mac_address(input, ts_classifier->eth_class_conf.mac_addr)) {
                        cli_printf("\n Invalid MAC address format");
                    }
                }
            }
        }

        // Get vlan check
        vlan_check = get_user_input("\n VLAN Check [0 : parse VLAN if any, 1: verify configured VLAN tag configuration]: ");
        if ((vlan_check == 0) || (vlan_check == 1)) {
            if (i == 2) {
                ts_classifier->eth2_class_conf.vlan_check = vlan_check;
            } else {
                ts_classifier->eth_class_conf.vlan_check = vlan_check;
            }
        } else {
            cli_printf("\n Invalid input for vlan check");
            return -1;
        }

        // Get vlan_conf.num_tag from user
        num_tag = get_user_input("\n Number of Tags: ");
        if (num_tag >= 0 && num_tag <= 2) {
            if (i == 2) {
                ts_classifier->eth2_class_conf.vlan_conf.num_tag = num_tag;
            } else {
                ts_classifier->eth_class_conf.vlan_conf.num_tag = num_tag;
            }
        } else {
            cli_printf("\n Invalid input for Number of Tags\n");
            return -1;
        }
    }
    ts_classifier->eth_class_conf.vlan_conf.outer_tag.mode = MEPA_TS_MATCH_MODE_VALUE;
    ts_classifier->eth_class_conf.vlan_conf.inner_tag.mode = MEPA_TS_MATCH_MODE_VALUE;
    ts_classifier->eth2_class_conf.vlan_conf.outer_tag.mode = MEPA_TS_MATCH_MODE_VALUE;
    ts_classifier->eth2_class_conf.vlan_conf.inner_tag.mode = MEPA_TS_MATCH_MODE_VALUE;

    if ((encap_type == MEPA_TS_ENCAP_ETH_IP_PTP) || (encap_type == MEPA_TS_ENCAP_ETH_IP_IP_PTP) || (encap_type == MEPA_TS_ENCAP_ETH_ETH_IP_PTP) || (encap_type == MEPA_TS_ENCAP_ETH_MPLS_IP_PTP) ||
        (encap_type == MEPA_TS_ENCAP_ETH_MPLS_ETH_IP_PTP)) {


        if (encap_type == MEPA_TS_ENCAP_ETH_IP_IP_PTP) {
            nheaderreq = 2;
        } else {
            nheaderreq = 1;
        }

        ip_ver = get_user_input("\n IP Version [0 : ip4v, 1: ip6v]: ");
        if ((ip_ver == 0) || (ip_ver == 1)) {
            ts_classifier->ip2_class_conf.ip_ver = ip_ver;
            ts_classifier->ip_class_conf.ip_ver = ip_ver;

        } else {
            cli_printf("\n Invalid input for IP Version\n");
            return -1;
        }
        ip_match = get_user_input("\n IP Address Match [0 : Disable match, 1: Desti. IP match, 2: Source IP match]: ");

        if (ip_match == 0) {
            ts_classifier->ip_class_conf.ip_match_mode = MEPA_TS_IP_MATCH_NONE;
            ts_classifier->ip2_class_conf.ip_match_mode = MEPA_TS_IP_MATCH_NONE;
        } else if (ip_match == 1) {
            ts_classifier->ip_class_conf.ip_match_mode = MEPA_TS_IP_MATCH_DEST;
            ts_classifier->ip_class_conf.ip_addr.ipv4.mask = 0xFFFFFFFF;   /* Match complete 32-bits of IP Address */
            memcpy(&ts_classifier->ip_class_conf.ip_addr.ipv6.mask, ipv6_mask, sizeof(ipv6_mask)); /* Match complete 128-bits of IP Address */
            if (nheaderreq == 2) {
                ts_classifier->ip2_class_conf.ip_match_mode = MEPA_TS_IP_MATCH_DEST;
                ts_classifier->ip2_class_conf.ip_addr.ipv4.mask = 0xFFFFFFFF;  /* Match complete 32-bits of IP Address */
                memcpy(&ts_classifier->ip2_class_conf.ip_addr.ipv6.mask, ipv6_mask, sizeof(ipv6_mask));
            }
        } else if (ip_match == 2) {
            ts_classifier->ip_class_conf.ip_match_mode = MEPA_TS_IP_MATCH_SRC;
            ts_classifier->ip_class_conf.ip_addr.ipv4.mask = 0xFFFFFFFF;   /* Match complete 32-bits of IP Address */
            memcpy(&ts_classifier->ip_class_conf.ip_addr.ipv6.mask, ipv6_mask, sizeof(ipv6_mask)); /* Match complete 128-bits of IP Address */
            if (nheaderreq == 2) {
                ts_classifier->ip2_class_conf.ip_match_mode = MEPA_TS_IP_MATCH_SRC;
                ts_classifier->ip2_class_conf.ip_addr.ipv4.mask = 0xFFFFFFFF;  /* Match complete 32-bits of IP Address */
                memcpy(&ts_classifier->ip2_class_conf.ip_addr.ipv6.mask, ipv6_mask, sizeof(ipv6_mask));
            }
        } else {
            cli_printf("\n Invalid input for IP Match selection\n");
            return -1;
        }

        if (ip_match == 0) {
            goto skip_ip_addr_get;
        }

        if (ip_ver == 1) { // ipv6
            const char *prompts[] = {
                "Enter First Layer IPv6 address (format - x:x:x:x:x:x:x:x): ",
                "Enter Second Layer IPv6 address (format - x:x:x:x:x:x:x:x): "
            };

            uint32_t (*addresses[])[4] = {
                &ts_classifier->ip_class_conf.ip_addr.ipv6.addr,
                &ts_classifier->ip2_class_conf.ip_addr.ipv6.addr
            };

            for (uint i = 0; i < nheaderreq && i < 2; i++) {
                cli_printf("\n %s", prompts[i]);
                if (fgets(input, sizeof(input), stdin) != NULL) {
                    if (!parse_ipv6_address(input, (uint32_t *)addresses[i])) {
                        cli_printf("\n Invalid IPv6 address format");
                        return -1;
                    }
                }
            }

        } else {
            const char *prompts[] = {
                "Enter First Layer IPv4 address (format - x.x.x.x): ",
                "Enter Second Layer IPv4 address (format - x.x.x.x): "
            };

            uint32_t *addresses[] = {
                &ts_classifier->ip_class_conf.ip_addr.ipv4.addr,
                &ts_classifier->ip2_class_conf.ip_addr.ipv4.addr
            };

            for (uint i = 0; i < nheaderreq && i < 2; i++) {
                cli_printf("\n %s", prompts[i]);
                if (fgets(input, sizeof(input), stdin) != NULL) {
                    if (!parse_ipv4_address(input, addresses[i])) {
                        cli_printf("\n Invalid IPv4 address format");
                        return -1;
                    }
                }
            }
        }
    }

skip_ip_addr_get:

    ethtype_value_update(encap_type, ip_ver, num_tag, &eth1_ethtype, &eth2_ethtype);
    ts_classifier->eth_class_conf.vlan_conf.etype = eth1_ethtype;
    ts_classifier->eth2_class_conf.vlan_conf.etype = eth2_ethtype;

    if ((encap_type == MEPA_TS_ENCAP_ETH_ETH_PTP) || (encap_type == MEPA_TS_ENCAP_ETH_ETH_IP_PTP)) {
        ts_classifier->eth_class_conf.vlan_conf.pbb_en = 1;
        ts_classifier->eth2_class_conf.vlan_conf.pbb_en = 0;
    } else {
        ts_classifier->eth_class_conf.vlan_conf.pbb_en = 0;
        ts_classifier->eth2_class_conf.vlan_conf.pbb_en = 0;
    }
    return 1;
}

static int update_ts_mpls_flow(mepa_ts_classifier_mpls_t *mpls_conf)
{
    uint8_t flow_en, stack_depth, stack_ref_point, cw_en;

    flow_en = get_user_input("\n Flow Enable [0 : Disable, 1: Enable]: ");
    if ((flow_en == 0) || (flow_en == 1)) {
        mpls_conf->flow_en = flow_en;
    } else {
        cli_printf("\n Invalid input for Flow Enable");
        return -1;
    }

    cw_en = get_user_input("\n CW Enable [0 : Disable, 1: Enable]: ");
    if ((cw_en == 0) || (cw_en == 1)) {
        mpls_conf->cw_en = cw_en;
    } else {
        cli_printf("\n Invalid input for CW Enable");
        return -1;
    }

    stack_depth = get_user_input("\n Max MPLS Lables allowded [1, 2, 3, 4]: ");
    if ((stack_depth > 0) && (stack_depth <= 4)) {
        mpls_conf->stack_depth = ((1 << stack_depth) - 1);
    } else {
        cli_printf("\n Invalid input for Stack Depth");
        return -1;
    }

    stack_ref_point = get_user_input("\n Stack Reference Point [0 : Top, 1: End]: ");
    if ((stack_ref_point == 0) || (stack_ref_point == 1)) {
        mpls_conf->stack_ref_point = stack_ref_point;
    } else {
        cli_printf("\n Invalid input for Stack Reference Point");
        return -1;
    }

    mpls_conf->stack_level.first_lvl.match_mode = 0;
    mpls_conf->stack_level.second_lvl.match_mode = 0;
    mpls_conf->stack_level.third_lvl.match_mode = 0;
    mpls_conf->stack_level.fourth_lvl.match_mode = 0;

#if 0
    mepa_ts_mpls_lvl_rng_t *levels[] = {
        &mpls_conf->stack_level.first_lvl,
        &mpls_conf->stack_level.second_lvl,
        &mpls_conf->stack_level.third_lvl,
        &mpls_conf->stack_level.fourth_lvl
    };
    for (int i = 0; i < stack_depth; i++) {
        cli_printf("\n Enter lower, upper, and match mode (e.g., 10 20 1): ");

        if (i < 4) { // Ensure index is within bounds
            if (scanf("%u %u %hhu", &levels[i]->lower, &levels[i]->upper, &levels[i]->match_mode) != 3) {
                cli_printf("Invalid input. Please enter three numbers.\n");
                return -1;
            }
        } else {
            cli_printf("Invalid stack level index.\n");
            return -1;
        }
    }
#endif

    return 1;
}
static void cli_cmd_ts_tx_class_conf(cli_req_t *req)
{
    mepa_rc rc;
    mepa_port_no_t  port_no;
    mepa_ts_classifier_t ts_classifier;
    ts_configuration *mreq = req->module_req;
    uint16_t flow_index = 0;
    int error = 0;
    demo_phy_info_t phy_family;

    // Initialize the structure
    memset(&ts_classifier, 0, sizeof(ts_classifier));

    if (mreq->encap_type) {
        error = update_ts_classifier_encap(&ts_classifier, mreq->encap_type);

        if (error != 1) {
            return;
        }
        error = 0;
        if ((mreq->encap_type == MEPA_TS_ENCAP_ETH_MPLS_IP_PTP) || (mreq->encap_type == MEPA_TS_ENCAP_ETH_MPLS_ETH_PTP) ||
            (mreq->encap_type == MEPA_TS_ENCAP_ETH_MPLS_ETH_IP_PTP)) {
            error = update_ts_mpls_flow(&ts_classifier.mpls_class_conf);
            if (error != 1) {
                return;
            }
        }
    }


    for (int iport = 0; iport < MAX_PRTS; iport++) {
        port_no = iport2uport(iport);
        if (req->port_list[port_no] == 0) {
            continue;
        }
        if (!meba_ts_instance->phy_devices[iport]) {
            cli_printf(" Dev is Not Created for the port : %d\n", (iport + 1));
            return;
        }

        if ((rc = phy_family_detect(meba_ts_instance, iport, &phy_family)) != MEPA_RC_OK) {
            T_E("\n Error in Detecting PHY Family on Port %d\n", (iport + 1));
            return;
        }
        flow_index = mreq->eng_id;
        if (phy_family.family == PHY_FAMILY_MALIBU_25G) {
            /* Malibu25G has 3 Engines where each engine has 8 Flow IDs */
            if (mreq->eng_id != 0) {
                flow_index = (mreq->eng_id * 8);
            }
        }
        ts_classifier.pkt_encap_type = mreq->encap_type;
        ts_classifier.enable = TRUE;
        if (MEPA_RC_OK == mepa_ts_tx_classifier_conf_set(meba_ts_instance->phy_devices[iport], flow_index, &ts_classifier)) {
            cli_printf("\n ...... TS Tx classifier Configuration on Port : %d......\n", (iport + 1));
        } else {
            cli_printf("\n ...... TS Tx classifier Configuration Failed on Port : %d......\n", (iport + 1));
        }
    }
    return;
}

int parse_mepa_ts_classifier(const char *filename, mepa_ts_classifier_t *conf)
{
    FILE *f = fopen(filename, "r");
    if (!f) {
        return -1;
    }
    char line[512];
    int ip_class_conf_ipver = -1, ip2_class_conf_ipver = -1;
    while (fgets(line, sizeof(line), f)) {
        char *trimmed = trim(line);
        if (trimmed[0] == '#' || trimmed[0] == '\0') {
            continue;
        }
        char *eq = strchr(trimmed, '=');
        if (!eq) {
            continue;
        }
        *eq = '\0';
        char *tag = trim(trimmed);
        char *value = trim(eq + 1);

        // Top-level
        if (strcmp(tag, "enable") == 0) {
            conf->enable = atoi(value) ? 1 : 0;
        } else if (strcmp(tag, "pkt_encap_type") == 0) {
            int idx = parse_enum(value, pkt_encap_names, sizeof(pkt_encap_names) / sizeof(pkt_encap_names[0]));
            if (idx >= 0) {
                conf->pkt_encap_type = (mepa_ts_pkt_encap_t)idx;
            }
        } else if (strcmp(tag, "clock_id") == 0) {
            conf->clock_id = (uint16_t)strtoul(value, NULL, 0);
        }
        // eth_class_conf
        else if (strcmp(tag, "eth_class_conf.mac_match_mode") == 0) {
            int idx = parse_enum(value, mac_match_mode_names, sizeof(mac_match_mode_names) / sizeof(mac_match_mode_names[0]));
            if (idx >= 0) {
                conf->eth_class_conf.mac_match_mode = (mepa_ts_mac_match_mode_t)idx;
            }
        } else if (strcmp(tag, "eth_class_conf.mac_match_select") == 0) {
            int idx = parse_enum(value, mac_match_select_names, sizeof(mac_match_select_names) / sizeof(mac_match_select_names[0]));
            if (idx >= 0) {
                conf->eth_class_conf.mac_match_select = (mepa_ts_mac_match_select_t)idx;
            }
        } else if (strcmp(tag, "eth_class_conf.mac_addr") == 0) {
            parse_mac(value, conf->eth_class_conf.mac_addr);
        } else if (strcmp(tag, "eth_class_conf.vlan_check") == 0) {
            conf->eth_class_conf.vlan_check = atoi(value) ? 1 : 0;
        } else if (strcmp(tag, "eth_class_conf.vlan_conf.pbb_en") == 0) {
            conf->eth_class_conf.vlan_conf.pbb_en = atoi(value) ? 1 : 0;
        } else if (strcmp(tag, "eth_class_conf.vlan_conf.tpid") == 0) {
            conf->eth_class_conf.vlan_conf.tpid = (uint16_t)strtoul(value, NULL, 0);
        } else if (strcmp(tag, "eth_class_conf.vlan_conf.etype") == 0) {
            conf->eth_class_conf.vlan_conf.etype = (uint16_t)strtoul(value, NULL, 0);
        } else if (strcmp(tag, "eth_class_conf.vlan_conf.num_tag") == 0) {
            conf->eth_class_conf.vlan_conf.num_tag = (uint8_t)atoi(value);
        } else if (strcmp(tag, "eth_class_conf.vlan_conf.outer_tag.mode") == 0) {
            conf->eth_class_conf.vlan_conf.outer_tag.mode = (mepa_ts_match_mode_t)atoi(value);
        } else if (strcmp(tag, "eth_class_conf.vlan_conf.outer_tag.match.range.lower") == 0) {
            conf->eth_class_conf.vlan_conf.outer_tag.match.range.lower = (uint16_t)strtoul(value, NULL, 0);
        } else if (strcmp(tag, "eth_class_conf.vlan_conf.outer_tag.match.range.upper") == 0) {
            conf->eth_class_conf.vlan_conf.outer_tag.match.range.upper = (uint16_t)strtoul(value, NULL, 0);
        } else if (strcmp(tag, "eth_class_conf.vlan_conf.outer_tag.match.value.val") == 0) {
            conf->eth_class_conf.vlan_conf.outer_tag.match.value.val = (uint16_t)strtoul(value, NULL, 0);
        } else if (strcmp(tag, "eth_class_conf.vlan_conf.outer_tag.match.value.mask") == 0) {
            conf->eth_class_conf.vlan_conf.outer_tag.match.value.mask = (uint16_t)strtoul(value, NULL, 0);
        } else if (strcmp(tag, "eth_class_conf.vlan_conf.inner_tag.mode") == 0) {
            conf->eth_class_conf.vlan_conf.inner_tag.mode = (mepa_ts_match_mode_t)atoi(value);
        } else if (strcmp(tag, "eth_class_conf.vlan_conf.inner_tag.match.range.lower") == 0) {
            conf->eth_class_conf.vlan_conf.inner_tag.match.range.lower = (uint16_t)strtoul(value, NULL, 0);
        } else if (strcmp(tag, "eth_class_conf.vlan_conf.inner_tag.match.range.upper") == 0) {
            conf->eth_class_conf.vlan_conf.inner_tag.match.range.upper = (uint16_t)strtoul(value, NULL, 0);
        } else if (strcmp(tag, "eth_class_conf.vlan_conf.inner_tag.match.value.val") == 0) {
            conf->eth_class_conf.vlan_conf.inner_tag.match.value.val = (uint16_t)strtoul(value, NULL, 0);
        } else if (strcmp(tag, "eth_class_conf.vlan_conf.inner_tag.match.value.mask") == 0) {
            conf->eth_class_conf.vlan_conf.inner_tag.match.value.mask = (uint16_t)strtoul(value, NULL, 0);
        }
        // ip_class_conf
        else if (strcmp(tag, "ip_class_conf.ip_ver") == 0) {
            int idx = parse_enum(value, ip_ver_names, sizeof(ip_ver_names) / sizeof(ip_ver_names[0]));
            if (idx >= 0) {
                conf->ip_class_conf.ip_ver = (mepa_ts_ip_ver_t)idx;
                ip_class_conf_ipver = idx;
            }
        } else if (strcmp(tag, "ip_class_conf.ip_match_mode") == 0) {
            int idx = parse_enum(value, ip_match_mode_names, sizeof(ip_match_mode_names) / sizeof(ip_match_mode_names[0]));
            if (idx >= 0) {
                conf->ip_class_conf.ip_match_mode = (mepa_ts_ip_match_select_t)idx;
            }
        } else if (strncmp(tag, "ip_class_conf.ip_addr.ipv4.", 27) == 0 && ip_class_conf_ipver == 0) {
            if (strcmp(tag, "ip_class_conf.ip_addr.ipv4.addr") == 0) {
                conf->ip_class_conf.ip_addr.ipv4.addr = (uint32_t)strtoul(value, NULL, 0);
            } else if (strcmp(tag, "ip_class_conf.ip_addr.ipv4.mask") == 0) {
                conf->ip_class_conf.ip_addr.ipv4.mask = (uint32_t)strtoul(value, NULL, 0);
            }
        } else if (strncmp(tag, "ip_class_conf.ip_addr.ipv6.", 27) == 0 && ip_class_conf_ipver == 1) {
            if (strcmp(tag, "ip_class_conf.ip_addr.ipv6.addr") == 0) {
                parse_u32_array(value, conf->ip_class_conf.ip_addr.ipv6.addr, 4);
            } else if (strcmp(tag, "ip_class_conf.ip_addr.ipv6.mask") == 0) {
                parse_u32_array(value, conf->ip_class_conf.ip_addr.ipv6.mask, 4);
            }
        } else if (strcmp(tag, "ip_class_conf.udp_sport_en") == 0) {
            conf->ip_class_conf.udp_sport_en = atoi(value) ? 1 : 0;
        } else if (strcmp(tag, "ip_class_conf.udp_dport_en") == 0) {
            conf->ip_class_conf.udp_dport_en = atoi(value) ? 1 : 0;
        } else if (strcmp(tag, "ip_class_conf.udp_sport") == 0) {
            conf->ip_class_conf.udp_sport = (uint16_t)strtoul(value, NULL, 0);
        } else if (strcmp(tag, "ip_class_conf.udp_dport") == 0) {
            conf->ip_class_conf.udp_dport = (uint16_t)strtoul(value, NULL, 0);
        }
        // eth2_class_conf
        else if (strncmp(tag, "eth2_class_conf.", 16) == 0) {
            char *subtag = tag + 16;
            if (strcmp(subtag, "mac_match_mode") == 0) {
                int idx = parse_enum(value, mac_match_mode_names, sizeof(mac_match_mode_names) / sizeof(mac_match_mode_names[0]));
                if (idx >= 0) {
                    conf->eth2_class_conf.mac_match_mode = (mepa_ts_mac_match_mode_t)idx;
                }
            } else if (strcmp(subtag, "mac_match_select") == 0) {
                int idx = parse_enum(value, mac_match_select_names, sizeof(mac_match_select_names) / sizeof(mac_match_select_names[0]));
                if (idx >= 0) {
                    conf->eth2_class_conf.mac_match_select = (mepa_ts_mac_match_select_t)idx;
                }
            } else if (strcmp(subtag, "mac_addr") == 0) {
                parse_mac(value, conf->eth2_class_conf.mac_addr);
            } else if (strcmp(subtag, "vlan_check") == 0) {
                conf->eth2_class_conf.vlan_check = atoi(value) ? 1 : 0;
            } else if (strcmp(subtag, "vlan_conf.pbb_en") == 0) {
                conf->eth2_class_conf.vlan_conf.pbb_en = atoi(value) ? 1 : 0;
            } else if (strcmp(subtag, "vlan_conf.tpid") == 0) {
                conf->eth2_class_conf.vlan_conf.tpid = (uint16_t)strtoul(value, NULL, 0);
            } else if (strcmp(subtag, "vlan_conf.etype") == 0) {
                conf->eth2_class_conf.vlan_conf.etype = (uint16_t)strtoul(value, NULL, 0);
            } else if (strcmp(subtag, "vlan_conf.num_tag") == 0) {
                conf->eth2_class_conf.vlan_conf.num_tag = (uint8_t)atoi(value);
            } else if (strcmp(subtag, "vlan_conf.outer_tag.mode") == 0) {
                conf->eth2_class_conf.vlan_conf.outer_tag.mode = (mepa_ts_match_mode_t)atoi(value);
            } else if (strcmp(subtag, "vlan_conf.outer_tag.match.range.lower") == 0) {
                conf->eth2_class_conf.vlan_conf.outer_tag.match.range.lower = (uint16_t)strtoul(value, NULL, 0);
            } else if (strcmp(subtag, "vlan_conf.outer_tag.match.range.upper") == 0) {
                conf->eth2_class_conf.vlan_conf.outer_tag.match.range.upper = (uint16_t)strtoul(value, NULL, 0);
            } else if (strcmp(subtag, "vlan_conf.outer_tag.match.value.val") == 0) {
                conf->eth2_class_conf.vlan_conf.outer_tag.match.value.val = (uint16_t)strtoul(value, NULL, 0);
            } else if (strcmp(subtag, "vlan_conf.outer_tag.match.value.mask") == 0) {
                conf->eth2_class_conf.vlan_conf.outer_tag.match.value.mask = (uint16_t)strtoul(value, NULL, 0);
            } else if (strcmp(subtag, "vlan_conf.inner_tag.mode") == 0) {
                conf->eth2_class_conf.vlan_conf.inner_tag.mode = (mepa_ts_match_mode_t)atoi(value);
            } else if (strcmp(subtag, "vlan_conf.inner_tag.match.range.lower") == 0) {
                conf->eth2_class_conf.vlan_conf.inner_tag.match.range.lower = (uint16_t)strtoul(value, NULL, 0);
            } else if (strcmp(subtag, "vlan_conf.inner_tag.match.range.upper") == 0) {
                conf->eth2_class_conf.vlan_conf.inner_tag.match.range.upper = (uint16_t)strtoul(value, NULL, 0);
            } else if (strcmp(subtag, "vlan_conf.inner_tag.match.value.val") == 0) {
                conf->eth2_class_conf.vlan_conf.inner_tag.match.value.val = (uint16_t)strtoul(value, NULL, 0);
            } else if (strcmp(subtag, "vlan_conf.inner_tag.match.value.mask") == 0) {
                conf->eth2_class_conf.vlan_conf.inner_tag.match.value.mask = (uint16_t)strtoul(value, NULL, 0);
            }
        }
        // ip2_class_conf
        else if (strcmp(tag, "ip2_class_conf.ip_ver") == 0) {
            int idx = parse_enum(value, ip_ver_names, sizeof(ip_ver_names) / sizeof(ip_ver_names[0]));
            if (idx >= 0) {
                conf->ip2_class_conf.ip_ver = (mepa_ts_ip_ver_t)idx;
                ip2_class_conf_ipver = idx;
            }
        } else if (strcmp(tag, "ip2_class_conf.ip_match_mode") == 0) {
            int idx = parse_enum(value, ip_match_mode_names, sizeof(ip_match_mode_names) / sizeof(ip_match_mode_names[0]));
            if (idx >= 0) {
                conf->ip2_class_conf.ip_match_mode = (mepa_ts_ip_match_select_t)idx;
            }
        } else if (strncmp(tag, "ip2_class_conf.ip_addr.ipv4.", 27) == 0 && ip2_class_conf_ipver == 0) {
            if (strcmp(tag, "ip2_class_conf.ip_addr.ipv4.addr") == 0) {
                conf->ip2_class_conf.ip_addr.ipv4.addr = (uint32_t)strtoul(value, NULL, 0);
            } else if (strcmp(tag, "ip2_class_conf.ip_addr.ipv4.mask") == 0) {
                conf->ip2_class_conf.ip_addr.ipv4.mask = (uint32_t)strtoul(value, NULL, 0);
            }
        } else if (strncmp(tag, "ip2_class_conf.ip_addr.ipv6.", 27) == 0 && ip2_class_conf_ipver == 1) {
            if (strcmp(tag, "ip2_class_conf.ip_addr.ipv6.addr") == 0) {
                parse_u32_array(value, conf->ip2_class_conf.ip_addr.ipv6.addr, 4);
            } else if (strcmp(tag, "ip2_class_conf.ip_addr.ipv6.mask") == 0) {
                parse_u32_array(value, conf->ip2_class_conf.ip_addr.ipv6.mask, 4);
            }
        } else if (strcmp(tag, "ip2_class_conf.udp_sport_en") == 0) {
            conf->ip2_class_conf.udp_sport_en = atoi(value) ? 1 : 0;
        } else if (strcmp(tag, "ip2_class_conf.udp_dport_en") == 0) {
            conf->ip2_class_conf.udp_dport_en = atoi(value) ? 1 : 0;
        } else if (strcmp(tag, "ip2_class_conf.udp_sport") == 0) {
            conf->ip2_class_conf.udp_sport = (uint16_t)strtoul(value, NULL, 0);
        } else if (strcmp(tag, "ip2_class_conf.udp_dport") == 0) {
            conf->ip2_class_conf.udp_dport = (uint16_t)strtoul(value, NULL, 0);
        }
        // mpls_class_conf
        else if (strncmp(tag, "mpls_class_conf.", 16) == 0) {
            char *subtag = tag + 16;
            if (strcmp(subtag, "cw_en") == 0) {
                conf->mpls_class_conf.cw_en = atoi(value) ? 1 : 0;
            } else if (strcmp(subtag, "flow_en") == 0) {
                conf->mpls_class_conf.flow_en = atoi(value) ? 1 : 0;
            } else if (strcmp(subtag, "stack_depth") == 0) {
                conf->mpls_class_conf.stack_depth = (uint8_t)atoi(value);
            } else if (strcmp(subtag, "stack_ref_point") == 0) {
                conf->mpls_class_conf.stack_ref_point = (uint8_t)atoi(value);
            } else if (strcmp(subtag, "stack_level.first_lvl.lower") == 0) {
                conf->mpls_class_conf.stack_level.first_lvl.lower = (uint32_t)strtoul(value, NULL, 0);
            } else if (strcmp(subtag, "stack_level.first_lvl.upper") == 0) {
                conf->mpls_class_conf.stack_level.first_lvl.upper = (uint32_t)strtoul(value, NULL, 0);
            } else if (strcmp(subtag, "stack_level.first_lvl.match_mode") == 0) {
                conf->mpls_class_conf.stack_level.first_lvl.match_mode = (uint8_t)atoi(value);
            } else if (strcmp(subtag, "stack_level.second_lvl.lower") == 0) {
                conf->mpls_class_conf.stack_level.second_lvl.lower = (uint32_t)strtoul(value, NULL, 0);
            } else if (strcmp(subtag, "stack_level.second_lvl.upper") == 0) {
                conf->mpls_class_conf.stack_level.second_lvl.upper = (uint32_t)strtoul(value, NULL, 0);
            } else if (strcmp(subtag, "stack_level.second_lvl.match_mode") == 0) {
                conf->mpls_class_conf.stack_level.second_lvl.match_mode = (uint8_t)atoi(value);
            } else if (strcmp(subtag, "stack_level.third_lvl.lower") == 0) {
                conf->mpls_class_conf.stack_level.third_lvl.lower = (uint32_t)strtoul(value, NULL, 0);
            } else if (strcmp(subtag, "stack_level.third_lvl.upper") == 0) {
                conf->mpls_class_conf.stack_level.third_lvl.upper = (uint32_t)strtoul(value, NULL, 0);
            } else if (strcmp(subtag, "stack_level.third_lvl.match_mode") == 0) {
                conf->mpls_class_conf.stack_level.third_lvl.match_mode = (uint8_t)atoi(value);
            } else if (strcmp(subtag, "stack_level.fourth_lvl.lower") == 0) {
                conf->mpls_class_conf.stack_level.fourth_lvl.lower = (uint32_t)strtoul(value, NULL, 0);
            } else if (strcmp(subtag, "stack_level.fourth_lvl.upper") == 0) {
                conf->mpls_class_conf.stack_level.fourth_lvl.upper = (uint32_t)strtoul(value, NULL, 0);
            } else if (strcmp(subtag, "stack_level.fourth_lvl.match_mode") == 0) {
                conf->mpls_class_conf.stack_level.fourth_lvl.match_mode = (uint8_t)atoi(value);
            }
        }
    }
    fclose(f);
    return 0;
}

#if 0
static void cli_cmd_ts_tx_class_f(cli_req_t *req)
{
    mepa_port_no_t  port_no;
    mepa_ts_classifier_t ts_classifier;
    uint16_t flow_index = 0;

    memset(&ts_classifier, 0, sizeof(ts_classifier));
    if (parse_mepa_ts_classifier("/root/mepa_scripts/ts_tx_class.cfg", &ts_classifier) == 0) {
        // conf is now filled with values from the config file
        for (int iport = 0; iport < MAX_PRTS; iport++) {
            port_no = iport2uport(iport);
            if (req->port_list[port_no] == 0) {
                continue;
            }
            if (!meba_ts_instance->phy_devices[iport]) {
                cli_printf(" Dev is Not Created for the port : %d\n", iport);
                return;
            }

            if (MEPA_RC_OK == mepa_ts_tx_classifier_conf_set(meba_ts_instance->phy_devices[iport], flow_index, &ts_classifier)) {
                cli_printf("\n ...... TS Tx classifier Configuration on Port : %d......\n", iport);
            } else {
                cli_printf("\n ...... TS Tx classifier Configuration Failed on Port : %d......\n", iport);
            }
        }
    } else {
        cli_printf("cfg file open failed\n");
    }
    return;
}
#endif


#if 0
static void cli_cmd_ts_rx_class_f(cli_req_t *req)
{
    mepa_port_no_t  port_no;
    mepa_ts_classifier_t ts_classifier;
    uint16_t flow_index = 0;

    memset(&ts_classifier, 0, sizeof(ts_classifier));
    if (parse_mepa_ts_classifier("/root/mepa_scripts/ts_rx_class.cfg", &ts_classifier) == 0) {
        // conf is now filled with values from the config file
        for (int iport = 0; iport < MAX_PRTS; iport++) {
            port_no = iport2uport(iport);
            if (req->port_list[port_no] == 0) {
                continue;
            }
            if (!meba_ts_instance->phy_devices[iport]) {
                cli_printf(" Dev is Not Created for the port : %d\n", iport);
                return;
            }

            if (MEPA_RC_OK == mepa_ts_rx_classifier_conf_set(meba_ts_instance->phy_devices[iport], flow_index, &ts_classifier)) {
                cli_printf("\n ...... TS Tx classifier Configuration on Port : %d......\n", iport);
            } else {
                cli_printf("\n ...... TS Tx classifier Configuration Failed on Port : %d......\n", iport);
            }
        }
    } else {
        cli_printf("cfg file open failed\n");
    }
    return;
}
#endif

static void update_ptp_clock_conf(mepa_ts_ptp_clock_conf_t *ptp_clock_conf)
{
    ptp_clock_conf->enable = 1;

    // Update ptp_class_conf
    ptp_clock_conf->ptp_class_conf.version.upper = 2;
    ptp_clock_conf->ptp_class_conf.version.lower = 2;
    ptp_clock_conf->ptp_class_conf.minor_version.upper = 1;
    ptp_clock_conf->ptp_class_conf.minor_version.lower = 0;

    // Update domain
    // PTP Domain field match value, it matches from (0 to 255)
    ptp_clock_conf->ptp_class_conf.domain.mode = MEPA_TS_MATCH_MODE_RANGE;
    ptp_clock_conf->ptp_class_conf.domain.match.range.lower = 0;
    ptp_clock_conf->ptp_class_conf.domain.match.range.upper = 255;

    // Update sdoid
    ptp_clock_conf->ptp_class_conf.sdoid.mode = MEPA_TS_MATCH_MODE_VALUE;
    ptp_clock_conf->ptp_class_conf.sdoid.match.value.val = 0;
    ptp_clock_conf->ptp_class_conf.sdoid.match.value.mask = 0;

    // Update clock mode and delay measurement type
    ptp_clock_conf->clk_mode = MEPA_TS_PTP_CLOCK_MODE_BC1STEP;
    ptp_clock_conf->delaym_type = MEPA_TS_PTP_DELAYM_E2E;
    ptp_clock_conf->cf_update = 0;
}


static void cli_cmd_ts_tx_clock_conf(cli_req_t *req)
{
    mepa_port_no_t  port_no;
    mepa_ts_ptp_clock_conf_t ts_clock;
    ts_configuration *mreq = req->module_req;
    uint16_t clk_id = 0;

    memset(&ts_clock, 0, sizeof(ts_clock));

    for (int iport = 0; iport < MAX_PRTS; iport++) {
        port_no = iport2uport(iport);
        if (req->port_list[port_no] == 0) {
            continue;
        }
        if (!meba_ts_instance->phy_devices[iport]) {
            cli_printf(" Dev is Not Created for the port : %d\n", (iport + 1));
            return;
        }

        clk_id = mreq->eng_id;

        if (mepa_ts_tx_clock_conf_get(meba_ts_instance->phy_devices[iport], clk_id, &ts_clock) != MEPA_RC_OK) {
            cli_printf("\n ...... Failed to get TS Tx Clock Configuration on Port : %d......\n", (iport + 1));
            return;
        }

        update_ptp_clock_conf(&ts_clock);

        ts_clock.clk_mode = mreq->clk_mode;
        ts_clock.delaym_type = mreq->delay_type;

        ts_clock.ptp_class_conf.domain.match.range.upper = 255;
        ts_clock.enable = TRUE;
        if (MEPA_RC_OK == mepa_ts_tx_clock_conf_set(meba_ts_instance->phy_devices[iport], clk_id, &ts_clock)) {
            cli_printf("\n ...... TS Tx Clock Configuration on Port : %d......\n", (iport + 1));
        } else {
            cli_printf("\n ...... TS Tx Clock Configuration Failed on Port : %d......\n", (iport + 1));
            return;
        }
    }
    return;
}

int parse_mepa_ts_ptp_clock_conf(const char *filename, mepa_ts_ptp_clock_conf_t *conf)
{
    FILE *f = fopen(filename, "r");
    if (!f) {
        return -1;
    }
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        char *trimmed = trim(line);
        if (trimmed[0] == '#' || trimmed[0] == '\0') {
            continue;
        }
        char *eq = strchr(trimmed, '=');
        if (!eq) {
            continue;
        }
        *eq = '\0';
        char *tag = trim(trimmed);
        char *value = trim(eq + 1);

        // Top-level
        if (strcmp(tag, "enable") == 0) {
            conf->enable = atoi(value) ? 1 : 0;
        }
        // ptp_class_conf.version
        else if (strcmp(tag, "ptp_class_conf.version.lower") == 0) {
            conf->ptp_class_conf.version.lower = (uint8_t)strtoul(value, NULL, 0);
        } else if (strcmp(tag, "ptp_class_conf.version.upper") == 0) {
            conf->ptp_class_conf.version.upper = (uint8_t)strtoul(value, NULL, 0);
        }
        // ptp_class_conf.minor_version
        else if (strcmp(tag, "ptp_class_conf.minor_version.lower") == 0) {
            conf->ptp_class_conf.minor_version.lower = (uint8_t)strtoul(value, NULL, 0);
        } else if (strcmp(tag, "ptp_class_conf.minor_version.upper") == 0) {
            conf->ptp_class_conf.minor_version.upper = (uint8_t)strtoul(value, NULL, 0);
        }
        // ptp_class_conf.domain
        else if (strcmp(tag, "ptp_class_conf.domain.mode") == 0) {
            int idx = parse_enum(value, match_mode_names, sizeof(match_mode_names) / sizeof(match_mode_names[0]));
            if (idx >= 0) {
                conf->ptp_class_conf.domain.mode = (mepa_ts_match_mode_t)idx;
            }
        } else if (strcmp(tag, "ptp_class_conf.domain.match.range.lower") == 0) {
            conf->ptp_class_conf.domain.match.range.lower = (uint8_t)strtoul(value, NULL, 0);
        } else if (strcmp(tag, "ptp_class_conf.domain.match.range.upper") == 0) {
            conf->ptp_class_conf.domain.match.range.upper = (uint8_t)strtoul(value, NULL, 0);
        } else if (strcmp(tag, "ptp_class_conf.domain.match.value.val") == 0) {
            conf->ptp_class_conf.domain.match.value.val = (uint8_t)strtoul(value, NULL, 0);
        } else if (strcmp(tag, "ptp_class_conf.domain.match.value.mask") == 0) {
            conf->ptp_class_conf.domain.match.value.mask = (uint8_t)strtoul(value, NULL, 0);
        }
        // ptp_class_conf.sdoid
        else if (strcmp(tag, "ptp_class_conf.sdoid.mode") == 0) {
            int idx = parse_enum(value, match_mode_names, sizeof(match_mode_names) / sizeof(match_mode_names[0]));
            if (idx >= 0) {
                conf->ptp_class_conf.sdoid.mode = (mepa_ts_match_mode_t)idx;
            }
        } else if (strcmp(tag, "ptp_class_conf.sdoid.match.range.lower") == 0) {
            conf->ptp_class_conf.sdoid.match.range.lower = (uint8_t)strtoul(value, NULL, 0);
        } else if (strcmp(tag, "ptp_class_conf.sdoid.match.range.upper") == 0) {
            conf->ptp_class_conf.sdoid.match.range.upper = (uint8_t)strtoul(value, NULL, 0);
        } else if (strcmp(tag, "ptp_class_conf.sdoid.match.value.val") == 0) {
            conf->ptp_class_conf.sdoid.match.value.val = (uint8_t)strtoul(value, NULL, 0);
        } else if (strcmp(tag, "ptp_class_conf.sdoid.match.value.mask") == 0) {
            conf->ptp_class_conf.sdoid.match.value.mask = (uint8_t)strtoul(value, NULL, 0);
        }
        // clk_mode
        else if (strcmp(tag, "clk_mode") == 0) {
            int idx = parse_enum(value, clk_mode_names, sizeof(clk_mode_names) / sizeof(clk_mode_names[0]));
            if (idx >= 0) {
                conf->clk_mode = (mepa_ts_ptp_clock_mode_t)idx;
            }
        }
        // delaym_type
        else if (strcmp(tag, "delaym_type") == 0) {
            int idx = parse_enum(value, delaym_type_names, sizeof(delaym_type_names) / sizeof(delaym_type_names[0]));
            if (idx >= 0) {
                conf->delaym_type = (mepa_ts_ptp_delaym_type_t)idx;
            }
        }
        // cf_update
        else if (strcmp(tag, "cf_update") == 0) {
            conf->cf_update = atoi(value) ? 1 : 0;
        }
    }
    fclose(f);
    return 0;
}

#if 0
static void cli_cmd_ts_tx_clock_f(cli_req_t *req)
{
    mepa_port_no_t  port_no;
    mepa_ts_ptp_clock_conf_t ts_clock;
    uint16_t clk_id = 0;

    memset(&ts_clock, 0, sizeof(ts_clock));
    if (parse_mepa_ts_ptp_clock_conf("/root/mepa_scripts/ts_tx_clock.cfg", &ts_clock) == 0) {
        // conf is now filled with values from the config file
        for (int iport = 0; iport < MAX_PRTS; iport++) {
            port_no = iport2uport(iport);
            if (req->port_list[port_no] == 0) {
                continue;
            }
            if (!meba_ts_instance->phy_devices[iport]) {
                cli_printf(" Dev is Not Created for the port : %d\n", iport);
                return;
            }

            if (MEPA_RC_OK == mepa_ts_tx_clock_conf_set(meba_ts_instance->phy_devices[iport], clk_id, &ts_clock)) {
                cli_printf("\n ...... TS Tx Clock Configuration on Port : %d......\n", iport);
            } else {
                cli_printf("\n ...... TS Tx Clock Configuration Failed on Port : %d......\n", iport);
                return;
            }
            /*
            * MEPA-1155
            * TS event must be enabled for all ports irrespective of the clock mode
            */
            mepa_ts_fifo_read_install(meba_ts_instance->phy_devices[iport], NULL);
            mepa_ts_event_set(meba_ts_instance->phy_devices[iport], 1, 0xFFFF);
        }
    } else {
        cli_printf("cfg file open failed\n");
    }

    return;
}
#endif


#if 0
static void cli_cmd_ts_rx_clock_f(cli_req_t *req)
{
    mepa_port_no_t  port_no;
    mepa_ts_ptp_clock_conf_t ts_clock;
    uint16_t clk_id = 0;

    memset(&ts_clock, 0, sizeof(ts_clock));
    if (parse_mepa_ts_ptp_clock_conf("mepa_scripts/ts_rx_clock.cfg", &ts_clock) == 0) {
        // conf is now filled with values from the config file
        for (int iport = 0; iport < MAX_PRTS; iport++) {
            port_no = iport2uport(iport);
            if (req->port_list[port_no] == 0) {
                continue;
            }
            if (!meba_ts_instance->phy_devices[iport]) {
                cli_printf(" Dev is Not Created for the port : %d\n", iport);
                return;
            }

            if (MEPA_RC_OK == mepa_ts_rx_clock_conf_set(meba_ts_instance->phy_devices[iport], clk_id, &ts_clock)) {
                cli_printf("\n ...... TS Tx Clock Configuration on Port : %d......\n", iport);
            } else {
                cli_printf("\n ...... TS Tx Clock Configuration Failed on Port : %d......\n", iport);
                return;
            }
            /*
            * MEPA-1155
            * TS event must be enabled for all ports irrespective of the clock mode
            */
            mepa_ts_fifo_read_install(meba_ts_instance->phy_devices[iport], NULL);
            mepa_ts_event_set(meba_ts_instance->phy_devices[iport], 1, 0xFFFF);
        }
    }
    return;
}
#endif


static void cli_cmd_ts_rx_class_conf(cli_req_t *req)
{
    mepa_rc rc;
    mepa_port_no_t  port_no;
    mepa_ts_classifier_t ts_classifier;
    ts_configuration *mreq = req->module_req;
    uint16_t flow_index = 0;
    int error = 0;
    // Initialize the structure
    memset(&ts_classifier, 0, sizeof(ts_classifier));
    demo_phy_info_t phy_family;

    if (mreq->encap_type) {
        error = update_ts_classifier_encap(&ts_classifier, mreq->encap_type);
        if (error != 1) {
            return;
        }

        if ((mreq->encap_type == MEPA_TS_ENCAP_ETH_MPLS_IP_PTP) || (mreq->encap_type == MEPA_TS_ENCAP_ETH_MPLS_ETH_PTP) || \
            (mreq->encap_type == MEPA_TS_ENCAP_ETH_MPLS_ETH_IP_PTP)) {
            error = update_ts_mpls_flow(&ts_classifier.mpls_class_conf);

            if (error != 1) {
                return;
            }
        }
    }
    for (int iport = 0; iport < MAX_PRTS; iport++) {
        port_no = iport2uport(iport);
        if (req->port_list[port_no] == 0) {
            continue;
        }
        if (!meba_ts_instance->phy_devices[iport]) {
            cli_printf(" Dev is Not Created for the port : %d\n", (iport + 1));
            return;
        }

        if ((rc = phy_family_detect(meba_ts_instance, iport, &phy_family)) != MEPA_RC_OK) {
            T_E("\n Error in Detecting PHY Family on Port %d\n", (iport + 1));
            return;
        }

        flow_index = mreq->eng_id;

        if (phy_family.family == PHY_FAMILY_MALIBU_25G) {
            /* Malibu25G has 3 Engines where each engine has 8 Flow IDs */
            if (mreq->eng_id != 0) {
                flow_index = (mreq->eng_id * 8);
            }
        }
        ts_classifier.pkt_encap_type = mreq->encap_type;
        ts_classifier.enable = TRUE;
        if (MEPA_RC_OK == mepa_ts_rx_classifier_conf_set(meba_ts_instance->phy_devices[iport], flow_index, &ts_classifier)) {
            cli_printf("\n ...... TS Rx classifier Configuration on Port : %d......\n", (iport + 1));
        } else {
            cli_printf("\n ...... TS Rx classifier Configuration Failed on Port : %d......\n", (iport + 1));
        }
    }
    return;
}

static void cli_cmd_ts_rx_clock_conf(cli_req_t *req)
{
    mepa_port_no_t  port_no;
    mepa_ts_ptp_clock_conf_t ts_clock;
    ts_configuration *mreq = req->module_req;
    uint16_t clk_id = 0;

    memset(&ts_clock, 0, sizeof(ts_clock));

    for (int iport = 0; iport < MAX_PRTS; iport++) {
        port_no = iport2uport(iport);
        if (req->port_list[port_no] == 0) {
            continue;
        }
        if (!meba_ts_instance->phy_devices[iport]) {
            cli_printf(" Dev is Not Created for the port : %d\n", (iport + 1));
            return;
        }
        clk_id = mreq->eng_id;

        if (mepa_ts_rx_clock_conf_get(meba_ts_instance->phy_devices[iport], clk_id, &ts_clock) != MEPA_RC_OK) {
            cli_printf("\n ...... Failed to get TS Rx Clock Configuration on Port : %d......\n", (iport + 1));
            return;
        }

        update_ptp_clock_conf(&ts_clock);

        ts_clock.clk_mode = mreq->clk_mode;
        ts_clock.delaym_type = mreq->delay_type;
        ts_clock.enable = TRUE;

        if (MEPA_RC_OK == mepa_ts_rx_clock_conf_set(meba_ts_instance->phy_devices[iport], clk_id, &ts_clock)) {
            cli_printf("\n ...... TS Rx Clock Configuration on Port : %d......\n", (iport + 1));
        } else {
            cli_printf("\n ...... TS Rx Clock Configuration Failed on Port : %d......\n", (iport + 1));
        }
    }
    return;
}

static void cli_cmd_ts_ltc_ls(cli_req_t *req)
{
    ts_configuration *mreq = req->module_req;
    mepa_ts_ls_type_t ls_type;

    switch (mreq->action) {
    case 0:
        ls_type = MEPA_TS_CMD_LOAD;
        break;
    case 1:
        ls_type = MEPA_TS_CMD_SAVE;
        break;
    case 2:
        ls_type = MEPA_TS_CMD_DELTA;
        break;
    case 3:
        ls_type = MEPA_TS_CMD_TOD;
        break;
    default:
        cli_printf("\n Pin Action not support on port : %d\n", (req->port_no + 1));
        return;
    }

    if (MEPA_RC_OK == mepa_ts_ltc_ls_en(meba_ts_instance->phy_devices[req->port_no], ls_type)) {
        if (mreq->action == 0) {
            cli_printf("\n ...... TS LTC Action Load performed on Port : %d......\n", (req->port_no + 1));
        } else if (mreq->action == 1) {
            cli_printf("\n ...... TS LTC Action Save performed on Port : %d......\n", (req->port_no + 1));
        } else if (mreq->action == 2) {
            cli_printf("\n ...... TS LTC Action Delta performed on Port : %d......\n", (req->port_no + 1));
        } else if (mreq->action == 3) {
            cli_printf("\n ...... TS LTC Action ToD performed on Port : %d......\n", (req->port_no + 1));
        }
    } else {
        cli_printf("\n ...... TS LTC Pin Action failed on Port : %d......\n", (req->port_no + 1));
    }
    return;
}

static void cli_cmd_ts_pps_incfg(cli_req_t *req)
{
    ts_configuration *mreq = req->module_req;
    phy25g_pps_input_conf_t pin_conf;

    pin_conf.clk_select = mreq->clk_select;
    pin_conf.pin_inv_pol = LAN80XX_PTP_LSC_ACTIVE_HIGH;
    switch (mreq->pin_sync_mode) {
    case 0:
        pin_conf.pin_sync_mode = LAN80XX_PTP_ACTION_IMMEDIATE;
        break;
    case 1:
        pin_conf.pin_sync_mode = LAN80XX_PTP_ACTION_ONE_SHOT_ON_ACTIVE_EDGE;
        break;
    case 2:
        pin_conf.pin_sync_mode = LAN80XX_PTP_ACTION_CONTINUOUS;
        break;
    default:
        cli_printf("\n Pin Sync mode not supported on port : %d\n", (req->port_no + 1));
        return;
    }
    pin_conf.lsc_select = mreq->ls_ctrl_sel;

    if (MEPA_RC_OK == lan80xx_phy_ts_pps_input_confset(meba_ts_instance->phy_devices[req->port_no], req->port_no, &pin_conf)) {
        cli_printf("\n ...... TS PPS Input Configured for Port : %d......\n", (req->port_no + 1));
    } else {
        T_E("\n Error in configuring TS PPS Input for Port : %d \n", (req->port_no + 1));
        return;
    }
}

static void cli_cmd_ts_1pps_out_en(cli_req_t *req)
{
    ts_configuration *mreq = req->module_req;
    phy25g_pps_output_conf_t pin_conf;
    pin_conf.clk_select = LAN80XX_PTP_ASYNC_NO_CLOCK; /* Asyncronous 1-line 1PPS singnal */
    pin_conf.pin_inv_pol = LAN80XX_PTP_LSC_ACTIVE_HIGH;
    pin_conf.pin_sync_mode = LAN80XX_PTP_LSC_1PPS;
    pin_conf.pps_pulse_high = 1000000;
    pin_conf.pps_pulse_low = 0;
    pin_conf.lsc_select = mreq->ls_ctrl_sel;

    /* This Chip-specific API "lan80xx_phy_ts_pps_ouput_conf_set" has wider option to select Syncronous/Asyncronous PPS signal,
     * If User wants 1PPS similar to other MCHP PHYs user can use MEPA API "mepa_ts_pps_conf_set" which generates Asyncronous
     * 1PPS signal on LSC1 Pin hardcoded,as there is no option to select PIN
     */

    if (MEPA_RC_OK == lan80xx_phy_ts_pps_ouput_conf_set(meba_ts_instance->phy_devices[req->port_no], req->port_no, &pin_conf)) {
        cli_printf("\n ...... 1PPS Enabled for Port : %d......\n", (req->port_no + 1));
    } else {
        T_E("\n Error in enabling 1PPS for Port : %d \n", (req->port_no + 1));
        return;
    }
    return;
}

static void cli_cmd_ts_pps_outcfg(cli_req_t *req)
{
    ts_configuration *mreq = req->module_req;
    phy25g_pps_output_conf_t pin_conf;

    pin_conf.clk_select = mreq->clk_select;
    pin_conf.lsc_select = mreq->ls_ctrl_sel;
    pin_conf.pin_inv_pol = LAN80XX_PTP_LSC_ACTIVE_HIGH;
    pin_conf.pin_sync_mode = mreq->output_mode;
    pin_conf.pps_pulse_high = mreq->pps_width;
    pin_conf.pps_pulse_low = mreq->pps_interval;

    if (MEPA_RC_OK == lan80xx_phy_ts_pps_ouput_conf_set(meba_ts_instance->phy_devices[req->port_no], req->port_no, &pin_conf)) {
        cli_printf("\n ...... TS PPS Output Configured for Port : %d......\n", (req->port_no + 1));
    } else {
        T_E("\n Error in configuring TS PPS Output for Port : %d \n", (req->port_no + 1));
        return;
    }
}

static void cli_cmd_ts_serial_incfg(cli_req_t *req)
{
    ts_configuration *mreq = req->module_req;
    phy25g_ts_sertod_input_conf_t sertod_conf;

    sertod_conf.pin_select = mreq->pin_select;
    sertod_conf.pin_inv_pol = mreq->pin_inv_pol;
    sertod_conf.pin_sync_mode = mreq->pin_sync_mode;
    sertod_conf.load_enable = mreq->tod_load;
    sertod_conf.store_enable = mreq->tod_save;
    sertod_conf.msb_byte_first = 0;
    sertod_conf.msb_bit_first = 0;
    sertod_conf.ls_unit_sel = mreq->ls_ctrl_sel;
    sertod_conf.Ld_period_cfg.one_microsec_period_cfg = 318;
    sertod_conf.Ld_period_cfg.one_sec_wait_period_cfg = 999999;

    if (MEPA_RC_OK == lan80xx_phy_ts_sertod_input_confset(meba_ts_instance->phy_devices[req->port_no], req->port_no, &sertod_conf)) {
        cli_printf("\n ...... TS Serial ToD Input Configured for Port : %d......\n", (req->port_no + 1));
    } else {
        T_E("\n Error in configuring TS Serial ToD Input for Port : %d \n", (req->port_no + 1));
        return;
    }
}

static void cli_cmd_ts_serial_outcfg(cli_req_t *req)
{
    ts_configuration *mreq = req->module_req;
    phy25g_ts_sertod_output_conf_t sertod_conf;

    sertod_conf.pin_select = mreq->pin_select;
    sertod_conf.pin_inv_pol = mreq->pin_inv_pol;
    sertod_conf.pin_sync_mode = mreq->pin_sync_mode;
    sertod_conf.msb_byte_first = 0;
    sertod_conf.msb_bit_first = 0;
    sertod_conf.pin_wfh_period = mreq->wfh_period;
    sertod_conf.pin_wfl_period = mreq->wfl_period;

    if (MEPA_RC_OK == lan80xx_phy_ts_sertod_output_confset(meba_ts_instance->phy_devices[req->port_no], req->port_no, &sertod_conf)) {
        cli_printf("\n ...... TS Serial ToD Output Configured for Port : %d......\n", (req->port_no + 1));
    } else {
        T_E("\n Error in configuring TS Serial ToD Output for Port : %d \n", (req->port_no + 1));
        return;
    }
}

static void cli_cmd_ts_delta_adj(cli_req_t *req)
{
    ts_configuration *mreq = req->module_req;
    phy25g_ts_ltc_delta_adj_cfg phy25g_ltc_tod_adj;

    phy25g_ltc_tod_adj.nanoseconds = mreq->nanoseconds;
    phy25g_ltc_tod_adj.subnanoseconds = mreq->picoseconds;
    phy25g_ltc_tod_adj.lsc_select = mreq->ls_ctrl_sel;

    if (MEPA_RC_OK == lan80xx_phy_ts_ptptime_adj_delta(meba_ts_instance->phy_devices[req->port_no], req->port_no, &phy25g_ltc_tod_adj)) {
        cli_printf("\n ...... TS LTC Delta Adjust Configured for Port : %d......\n", (req->port_no + 1));
    } else {
        T_E("\n Error in configuring TS LTC Delta for Port : %d \n", (req->port_no + 1));
        return;
    }

}

static void cli_cmd_ts_epps_config(cli_req_t *req)
{
    ts_configuration *mreq = req->module_req;
    phy25g_ts_epps_conf_t epps_conf;

    epps_conf.clk_select = mreq->clk_select;
    epps_conf.pin_sync_mode = mreq->pin_sync_mode;
    epps_conf.epps_event_detect_adjust = mreq->epps_det_cfg;
    epps_conf.lsc_select = mreq->ls_ctrl_sel;

    if (MEPA_RC_OK == lan80xx_phy_ts_epps_conf_set(meba_ts_instance->phy_devices[req->port_no], req->port_no, &epps_conf)) {
        cli_printf("\n ...... TS EPPS Configured for Port : %d......\n", (req->port_no + 1));
    } else {
        T_E("\n Error in configuring TS EPPS for Port : %d \n", (req->port_no + 1));
        return;
    }

}

static void cli_cmd_ts_ltc_get(cli_req_t *req)
{
    mepa_timestamp_t timestamp;

    if (MEPA_RC_OK == mepa_ts_ltc_get(meba_ts_instance->phy_devices[req->port_no], &timestamp)) {
        cli_printf("\n\n ...... TS Local Time Counter on Port : %d......\n", (req->port_no + 1));
        cli_printf("seconds.high: %u\n", timestamp.seconds.high);
        cli_printf("seconds.low: %u\n", timestamp.seconds.low);
        cli_printf("nanoseconds: %u\n", timestamp.nanoseconds);
        cli_printf("picoseconds: %u\n", timestamp.picoseconds);
        cli_printf("\n");
    } else {
        T_E("\n Error in getting TS LTC on Port : %d \n", (req->port_no + 1));
        return;
    }
    return;
}


static void cli_cmd_ts_ltc_set(cli_req_t *req)
{
    ts_configuration *mreq = req->module_req;
    mepa_timestamp_t timestamp;

    timestamp.seconds.high = mreq->sec_high;
    timestamp.seconds.low = mreq->sec_low;
    timestamp.nanoseconds = mreq->nanoseconds;
    timestamp.picoseconds = mreq->picoseconds;

    if (MEPA_RC_OK == mepa_ts_ltc_set(meba_ts_instance->phy_devices[req->port_no], &timestamp)) {
        cli_printf("\n ...... TS LTC Set on Port : %d......\n", (req->port_no + 1));
    } else {
        T_E("\n Error in setting TS LTC on Port : %d \n", (req->port_no + 1));
        return;
    }
    return;
}

static void cli_cmd_ts_fifo_get(cli_req_t *req)
{
    mepa_ts_event_t status = 0;
    mepa_rc rc = MEPA_RC_ERROR;

    rc = mepa_ts_event_poll(meba_ts_instance->phy_devices[req->port_no], &status);
    if (rc != MEPA_RC_OK) {
        cli_printf("TS Event get failed\n");
        return;
    }

    if (status) {
        packet_count = 0;
        /*
         * This API "mepa_ts_fifo_empty" will read the FIFO for timestamp and PTP signature, and provide it to
         * callback function register through "mepa_ts_fifo_install()" API, in this Application function
         * "test_ts_phy_fifo_read" is registered as callback function which will print the Timestamp and signature
         */
        rc = mepa_ts_fifo_empty(meba_ts_instance->phy_devices[req->port_no]);
        if (rc != MEPA_RC_OK) {
            cli_printf ("\n Failed to Clear FIFO\n");
            return;
        }

    } else {
        cli_printf("\nNo Event Occured\n");
    }
}

static void cli_cmd_ts_sig_conf(cli_req_t *req)
{
    mepa_rc rc = MEPA_RC_ERROR;
    ts_configuration *mreq = req->module_req;
    demo_phy_info_t phy_family;
    mepa_ts_fifo_sig_mask_t phy_sig_mask = 0;

    if ((rc = phy_family_detect(meba_ts_instance, req->port_no, &phy_family)) != MEPA_RC_OK) {
        T_E("\n Error in Detecting PHY Family on Port %d\n", (req->port_no + 1));
        return;
    }

    switch (mreq->sig_mask) {
    case 1:
        phy_sig_mask = (MEPA_TS_PTP_FIFO_SIG_SEQ_ID | MEPA_TS_PTP_FIFO_SIG_MSG_TYPE | MEPA_TS_PTP_FIFO_SIG_DOMAIN_NUM);
        break;
    case 2:
        phy_sig_mask = (MEPA_TS_PTP_FIFO_SIG_SEQ_ID | MEPA_TS_PTP_FIFO_SIG_MSG_TYPE | MEPA_TS_PTP_FIFO_SIG_DOMAIN_NUM | MEPA_TS_PTP_FIFO_SIG_SOURCE_PORT_ID);
        break;
    case 3:
        phy_sig_mask = (MEPA_TS_PTP_FIFO_SIG_SEQ_ID | MEPA_TS_PTP_FIFO_SIG_MSG_TYPE | MEPA_TS_PTP_FIFO_SIG_DOMAIN_NUM | MEPA_TS_PTP_FIFO_SIG_DEST_IP | MEPA_TS_PTP_FIFO_SIG_SRC_IP);
        break;
    case 4:
        phy_sig_mask = (MEPA_TS_PTP_FIFO_SIG_SEQ_ID | MEPA_TS_PTP_FIFO_SIG_MSG_TYPE | MEPA_TS_PTP_FIFO_SIG_DOMAIN_NUM | MEPA_TS_PTP_FIFO_SIG_DEST_IP | MEPA_TS_PTP_FIFO_SIG_SRC_IP |
                    MEPA_TS_PTP_FIFO_SIG_SOURCE_PORT_ID);
        break;
    case 5:
        phy_sig_mask = (MEPA_TS_PTP_FIFO_SIG_SEQ_ID | MEPA_TS_PTP_FIFO_SIG_MSG_TYPE | MEPA_TS_PTP_FIFO_SIG_DOMAIN_NUM | MEPA_TS_PTP_FIFO_SIG_DEST_MAC);
        break;
    case 6:
        phy_sig_mask = (MEPA_TS_PTP_FIFO_SIG_SEQ_ID | MEPA_TS_PTP_FIFO_SIG_MSG_TYPE | MEPA_TS_PTP_FIFO_SIG_DOMAIN_NUM | MEPA_TS_PTP_FIFO_SIG_DEST_IP | MEPA_TS_PTP_FIFO_SIG_SRC_IP |
                    MEPA_TS_PTP_FIFO_SIG_DEST_MAC);
        break;
    case 7:
        if (phy_family.family == PHY_FAMILY_MALIBU_25G) {
            phy_sig_mask = (MEPA_TS_PTP_FIFO_SIG_SEQ_ID | MEPA_TS_PTP_FIFO_SIG_MSG_TYPE | MEPA_TS_PTP_FIFO_SIG_DOMAIN_NUM | MEPA_TS_PTP_FIFO_SIG_DEST_IP |  MEPA_TS_PTP_FIFO_SIG_SRC_IP |
                    MEPA_TS_PTP_FIFO_SIG_IPV6_DEST_IP);
        } else {
            cli_printf("\n Option Not supported by PHY at port:%d \n", req->port_no);
            return;
        }
        break;
    default:
        cli_printf("\n Invalid Signal configuration selected ");
        return;
    }


    rc = mepa_ts_fifo_signature_set(meba_ts_instance->phy_devices[req->port_no], phy_sig_mask);

    if (rc != MEPA_RC_OK) {
        cli_printf ("\n Failed to Configure PTP Signature fileds on port : %d\n", req->port_no);
        return;
    }

    return;
}

static void cli_cmd_ts_delay_set(cli_req_t *req)
{
    ts_configuration *mreq = req->module_req;
    mepa_timeinterval_t delay;
    delay = ((1 << 8) * mreq->delay);

    if (mreq->delay_mode == 0) {
        if (MEPA_RC_OK == mepa_ts_delay_asymmetry_set(meba_ts_instance->phy_devices[req->port_no], &delay)) {
            cli_printf("\n ...... TS Set Delay Asymmetry on Port : %d......\n", req->port_no);
        } else {
            T_E ("Error TS Set Delay Asymmetry on Port : %d\n", req->port_no);
            return;
        }
    } else if (mreq->delay_mode == 1) {
        if (MEPA_RC_OK == mepa_ts_path_delay_set(meba_ts_instance->phy_devices[req->port_no], &delay)) {
            cli_printf("\n ...... TS Set Path Delay on Port : %d......\n", req->port_no);
        } else {
            T_E ("Error TS Set Path Delay on Port : %d\n", req->port_no);
            return;
        }
    } else if (mreq->delay_mode == 2) {
        if (MEPA_RC_OK == mepa_ts_egress_latency_set(meba_ts_instance->phy_devices[req->port_no], &delay)) {
            cli_printf("\n ...... TS Set Egress Latency on Port : %d......\n", req->port_no);
        } else {
            T_E ("Error TS Set Egress Latency on Port : %d\n", req->port_no);
            return;
        }
    } else if (mreq->delay_mode == 3) {
        if (MEPA_RC_OK == mepa_ts_ingress_latency_set(meba_ts_instance->phy_devices[req->port_no], &delay)) {
            cli_printf("\n ...... TS Set Ingress Latency on Port : %d......\n", req->port_no);
        } else {
            T_E ("Error TS Set Ingress Latency on Port : %d\n", req->port_no);
            return;
        }
    }
}

static void cli_cmd_ts_delay_get(cli_req_t *req)
{
    ts_configuration *mreq = req->module_req;
    mepa_timeinterval_t delay;
    if (mreq->delay_mode == 0) {
        if (MEPA_RC_OK == mepa_ts_delay_asymmetry_get(meba_ts_instance->phy_devices[req->port_no], &delay)) {
            cli_printf("\n ...... TS Delay Asymmetry on Port : %d......\n", req->port_no);
        } else {
            T_E ("Error in TS Get Delay Asymmetry on Port : %d\n", req->port_no);
            return;
        }
    } else if (mreq->delay_mode == 1) {
        if (MEPA_RC_OK == mepa_ts_path_delay_get(meba_ts_instance->phy_devices[req->port_no], &delay)) {
            cli_printf("\n ...... TS Path Delay on Port : %d......\n", req->port_no);
        } else {
            T_E ("Error in TS Get Path Delay on Port : %d\n", req->port_no);
            return;
        }
    } else if (mreq->delay_mode == 2) {
        if (MEPA_RC_OK == mepa_ts_egress_latency_get(meba_ts_instance->phy_devices[req->port_no], &delay)) {
            cli_printf("\n ...... TS Egress Latency on Port : %d......\n", req->port_no);
        } else {
            T_E ("Error in TS Get Egress Latency on Port : %d\n", req->port_no);
            return;
        }
    } else if (mreq->delay_mode == 3) {
        if (MEPA_RC_OK == mepa_ts_ingress_latency_get(meba_ts_instance->phy_devices[req->port_no], &delay)) {
            cli_printf("\n ...... TS Ingress Latency on Port : %d......\n", req->port_no);
        } else {
            T_E ("Error in TS Get Ingress Latency on Port : %d\n", req->port_no);
            return;
        }
    }
    double time_in_ns = (double)delay / (1 << 8);
    cli_printf("\n Delay: %f ns (Equivalent hex value is 0x%x)\n", time_in_ns, delay );
}


static void cli_cmd_ts_flow_disable(cli_req_t *req)
{
    mepa_rc rc = MEPA_RC_ERROR;

    ts_configuration *mreq = req->module_req;
    mepa_ts_classifier_t      tx_class_conf = {0};
    mepa_ts_ptp_clock_conf_t  tx_clk_conf = {0};
    mepa_ts_classifier_t      rx_class_conf = {0};
    mepa_ts_ptp_clock_conf_t  rx_clk_conf = {0};

    u16 flow_index = (mreq->eng_id * 8);
    u16 clock_id = mreq->eng_id;

    rc = mepa_ts_tx_classifier_conf_get(meba_ts_instance->phy_devices[req->port_no], flow_index, &tx_class_conf);
    if (rc != MEPA_RC_OK) {
        cli_printf("\n Failed to Get TX Classifier conf\n");
        return;
    }

    rc = mepa_ts_tx_clock_conf_get(meba_ts_instance->phy_devices[req->port_no], clock_id, &tx_clk_conf);
    if (rc != MEPA_RC_OK) {
        cli_printf("\n Failed to Get RX Classifier conf\n");
        return;
    }

    rc = mepa_ts_rx_classifier_conf_get(meba_ts_instance->phy_devices[req->port_no], flow_index, &rx_class_conf);
    if (rc != MEPA_RC_OK) {
        cli_printf("\n Failed to Get TX Classifier conf\n");
        return;
    }

    rc = mepa_ts_rx_clock_conf_get(meba_ts_instance->phy_devices[req->port_no], clock_id, &rx_clk_conf);
    if (rc != MEPA_RC_OK) {
        cli_printf("\n Failed to Get RX Classifier conf\n");
        return;
    }

    tx_class_conf.enable = FALSE;
    rx_class_conf.enable = FALSE;
    tx_clk_conf.enable = FALSE;
    rx_clk_conf.enable = FALSE;

    rc = mepa_ts_tx_classifier_conf_set(meba_ts_instance->phy_devices[req->port_no], flow_index, &tx_class_conf);
    if (rc != MEPA_RC_OK) {
        cli_printf("\n Failed to Set TX Classifier conf\n");
        return;
    }

    rc = mepa_ts_tx_clock_conf_set(meba_ts_instance->phy_devices[req->port_no], clock_id, &tx_clk_conf);
    if (rc != MEPA_RC_OK) {
        cli_printf("\n Failed to Set RX Classifier conf\n");
        return;
    }

    rc = mepa_ts_rx_classifier_conf_set(meba_ts_instance->phy_devices[req->port_no], flow_index, &rx_class_conf);
    if (rc != MEPA_RC_OK) {
        cli_printf("\n Failed to Set TX Classifier conf\n");
        return;
    }

    rc = mepa_ts_rx_clock_conf_set(meba_ts_instance->phy_devices[req->port_no], clock_id, &rx_clk_conf);
    if (rc != MEPA_RC_OK) {
        cli_printf("\n Failed to Set RX Classifier conf\n");
        return;
    }

    return;
}

static void cli_cmd_ts_port_stati (cli_req_t *req)
{
    mepa_ts_stats_t stats;
    mepa_rc result = mepa_ts_stats_get(meba_ts_instance->phy_devices[req->port_no], &stats);

    if (result == MEPA_RC_OK) {
        cli_printf("\n Timestamp Port Statistics");
        cli_printf("\n ============================");
        cli_printf("\n Ingress Preamble Shrink Errors: %u", stats.ingr_pream_shrink_err);
        cli_printf("\n Egress Preamble Shrink Errors: %u", stats.egr_pream_shrink_err);
        cli_printf("\n Ingress FCS Errors: %u", stats.ingr_fcs_err);
        cli_printf("\n Egress FCS Errors: %u", stats.egr_fcs_err);
        cli_printf("\n Ingress Frame Modification Count: %u", stats.ingr_frm_mod_cnt);
        cli_printf("\n Egress Frame Modification Count: %u", stats.egr_frm_mod_cnt);
        cli_printf("\n Timestamp FIFO Transmit Count: %u", stats.ts_fifo_tx_cnt);
        cli_printf("\n Timestamp FIFO Drop Count: %u", stats.ts_fifo_drop_cnt);
        cli_printf("\n");
    } else {
        cli_printf("\n ...........Failed to get TS Port Stati............\n");
    }
    return;
}
static void cli_print_wrapped_json(const char *json_str, int width)
{
    char command[1024];
    snprintf(command, sizeof(command), "echo \"%s\" | fold -w %d", json_str, width);

    FILE *fp = popen(command, "r");
    if (fp == NULL) {
        perror("popen failed");
        return;
    }

    char buffer[128];
    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
        cli_printf("%s", buffer);
    }

    pclose(fp);
}

static void cli_cmd_ts_conf_get(cli_req_t *req)
{
    ts_configuration *mreq = req->module_req;
    json_rpc_req_t json_req = {};  // Initialize the JSON-RPC request structure
    json_object *jobj = NULL; // Pointer to hold the created JSON object
    uint16_t flw_idx_clk_id = 0;

    if (mreq->config == 0) { // mepa_ts_init_conf_get
        mepa_ts_init_conf_t conf;
        if (mepa_ts_init_conf_get(meba_ts_instance->phy_devices[req->port_no], &conf) == 0) {
            json_rpc_new_mepa_ts_init_conf_t(&json_req, &jobj, &conf);
        }
        cli_printf("\n---------------TS Init Configuration: Port %d-------------------------", (req->port_no + 1));
    } else if (mreq->config == 1) {
        mepa_ts_classifier_t class_conf;
        if (mepa_ts_tx_classifier_conf_get(meba_ts_instance->phy_devices[req->port_no], flw_idx_clk_id, &class_conf) == 0) {
            json_rpc_new_mepa_ts_classifier_t(&json_req, &jobj, &class_conf);
        }
        cli_printf("\n---------------TS Tx Classifier Configuration: Port %d-------------------------", (req->port_no + 1));
        cli_printf("\n flow_index: %x", flw_idx_clk_id);
    } else if (mreq->config == 2) {
        mepa_ts_ptp_clock_conf_t clk_conf;
        if (mepa_ts_tx_clock_conf_get(meba_ts_instance->phy_devices[req->port_no], flw_idx_clk_id, &clk_conf) == 0) {
            json_rpc_new_mepa_ts_ptp_clock_conf_t(&json_req, &jobj, &clk_conf);
        }
        cli_printf("\n---------------TS Tx Clock Configuration: Port %d-------------------------", (req->port_no + 1));
        cli_printf("\n clock_id: %x", flw_idx_clk_id);
    } else if (mreq->config == 3) {
        mepa_ts_classifier_t class_conf;
        if (mepa_ts_rx_classifier_conf_get(meba_ts_instance->phy_devices[req->port_no], flw_idx_clk_id, &class_conf) == 0) {
            json_rpc_new_mepa_ts_classifier_t(&json_req, &jobj, &class_conf);
        }
        cli_printf("\n---------------TS Rx Classifier Configuration: Port %d-------------------------", (req->port_no + 1));
        cli_printf("\n flow_index: %x", flw_idx_clk_id);
    } else if (mreq->config == 4) {
        mepa_ts_ptp_clock_conf_t clk_conf;
        if (mepa_ts_rx_clock_conf_get(meba_ts_instance->phy_devices[req->port_no], flw_idx_clk_id, &clk_conf) == 0) {
            json_rpc_new_mepa_ts_ptp_clock_conf_t(&json_req, &jobj, &clk_conf);
        }
        cli_printf("\n---------------TS Rx Clock Configuration: Port %d-------------------------", (req->port_no + 1));
        cli_printf("\n clock_id: %x", flw_idx_clk_id);
    } else if (mreq->config == 5) {
        u8 count = 1;
        mepa_ts_fifo_sig_mask_t sig_mask;
        demo_phy_info_t phy_family;
        if (phy_family_detect(meba_ts_instance, req->port_no, &phy_family) != MEPA_RC_OK) {
            T_E("\n Error in Detecting PHY Family on Port %d\n", (req->port_no + 1));
            return;
        }
        mepa_ts_fifo_signature_get(meba_ts_instance->phy_devices[req->port_no], &sig_mask);

        if (sig_mask == 0) {
            cli_printf("\n\n None of the PTP fields are enabled in PTP Signature on port %d\n", (req->port_no + 1));
            return;
        }  
        cli_printf("\n\n Following Fields are enabled in FIFO Signature on port :%d -----------------\n", (req->port_no + 1));

        PRINT_SIG_FIELD(MEPA_TS_PTP_FIFO_SIG_MSG_TYPE,       "Message Type");
        PRINT_SIG_FIELD(MEPA_TS_PTP_FIFO_SIG_DOMAIN_NUM,     "Domain Number");
        PRINT_SIG_FIELD(MEPA_TS_PTP_FIFO_SIG_SEQ_ID,         "Sequence ID");
        PRINT_SIG_FIELD(MEPA_TS_PTP_FIFO_SIG_SOURCE_PORT_ID, "Source Port ID");
        PRINT_SIG_FIELD(MEPA_TS_PTP_FIFO_SIG_DEST_IP,        "IPv4 Address");
        PRINT_SIG_FIELD(MEPA_TS_PTP_FIFO_SIG_DEST_MAC,       "Dest MAC Address");
        PRINT_SIG_FIELD(MEPA_TS_PTP_FIFO_SIG_IPV6_DEST_IP,   "IPv6 Address");
        return;
    }
    if (jobj != NULL) { // Check if the JSON object was created successfully
        json_object_object_foreach(jobj, key, val) {
            cli_printf("\n %s: ", key);
            const char *json_str = json_object_to_json_string_ext(val, JSON_C_TO_STRING_SPACED);
            cli_print_wrapped_json(json_str, 80);
        }
        // Free the JSON object
        json_object_put(jobj);
    } else {
        cli_printf("Failed to create JSON object.\n");
    }
}

static void cli_cmd_ts_csr_rd(cli_req_t *req)
{
    ts_configuration *mreq = req->module_req;

    mreq->csr_value = 0;
    if (MEPA_RC_OK == mepa_ts_csr_reg_read(meba_ts_instance->phy_devices[req->port_no], mreq->mmd, mreq->csr_addr, &mreq->csr_value)) {
        cli_printf("\n");
        cli_table_header("Port  MMD   Address  31******24*23*******16*15*******8***7*******0    Value");
        cli_printf("%-6u%-6u0x%04x   ", (req->port_no + 1), mreq->mmd, mreq->csr_addr);
        for (int i = 31; i >= 0; i--) {
            cli_printf("%d%s", mreq->csr_value & (1 << i) ? 1 : 0, (i % 4) || i == 0 ? "" : ". ");
        }
        cli_printf("   0x%x\n", mreq->csr_value);
    }
    else {
        cli_printf("\n TS csr Read Failed for addr:%x at Port:%d",mreq->csr_addr, req->port_no);
    }
}

static void cli_cmd_ts_csr_wr(cli_req_t *req)
{
    ts_configuration *mreq = req->module_req;

    if (MEPA_RC_OK != mepa_ts_csr_reg_write(meba_ts_instance->phy_devices[req->port_no], mreq->mmd, mreq->csr_addr, &mreq->csr_value)) {
        cli_printf("\n TS csr Write Failed for addr:%x at Port:%d", mreq->csr_addr, req->port_no);
    }
}

// static void cli_cmd_ts_clk_rateadj_set (cli_req_t *req) {
//     ts_configuration *mreq = req->module_req;
//     mepa_ts_scaled_ppb_t rate_adj;
//     rate_adj = ((1 << 16) * mreq->rateadj_ppb);

//     if(MEPA_RC_OK == mepa_ts_clock_rateadj_set(meba_ts_instance->phy_devices[req->port_no], &rate_adj)) {
//         cli_printf("\n ...... TS Clock Rate Adjust Set on Port : %d......\n", req->port_no);
//     } else {
//         T_E("\n Error in setting TS Clock Rate Adjust on Port : %d \n", req->port_no);
//     }
// }

// static void cli_cmd_ts_clk_rateadj_get (cli_req_t *req) {
//     mepa_ts_scaled_ppb_t rate_adj;

//     if(MEPA_RC_OK == mepa_ts_clock_rateadj_get(meba_ts_instance->phy_devices[req->port_no], &rate_adj)) {
//         cli_printf("\n ...... TS Clock Rate Adjust on Port : %d......\n", req->port_no);
//         double rateadj_ppb = (double)rate_adj / (1 << 16);
//         cli_printf("\n Clock Rate Adjust: %f ppb (Equivalent hex value is 0x%x)\n", rateadj_ppb, rate_adj );
//     } else {
//         T_E("\n Error in getting TS Clock Rate Adjust on Port : %d \n", req->port_no);
//     }
// }

static void cli_cmd_ts_reset (cli_req_t *req)
{
    mepa_ts_reset_conf_t tsreset = {0};
    tsreset.tsu_hard_reset = 1;

    if (MEPA_RC_OK == mepa_ts_reset(meba_ts_instance->phy_devices[req->port_no], &tsreset)) {
        cli_printf("\n ...... TS Reset on Port : %d......\n", (req->port_no + 1));
    } else {
        T_E("\n Error in Resetting TS Block on Port : %d \n", (req->port_no + 1));
    }
}

static void cli_cmd_ts_cmds()
{
    cli_printf("\n\n");
    cli_printf("\n\033[1;32m%-20s%-80s%s\033[0m\n", "  Commands", " Arguments", "  Description");
    cli_printf("\033[1;32m==================================================================================================================================\033[0m\n");
    cli_printf("\n %-20s| %-80s| %s", "exit_ts", "", "Exit from Time Stamping Application");
    cli_printf("\n %-20s| %-80s| %s", "ts_init_conf", " <port_list> clk_src <clk_src> tx_fifo <tx_fifo> tc_op <tc_op>", "Initialize TS Block");
    cli_printf("\n %-20s| %-80s| %s", "", " dly_10b <dly_10b> tx_af <tx_af> mch <mch>", "");
    cli_printf("\n %-20s| %-80s| %s", "tx_class_conf", " <port_list> eng_id <eng_idx> entype <entype>", "Configures Egress Port Classifier" );
    cli_printf("\n %-20s| %-80s| %s", "tx_clock_conf", " <port_list> eng_id <eng_idx> clk_mode <clk_mode> dly_type <dly_type>", "Configures Egress Port Clock" );
    cli_printf("\n %-20s| %-80s| %s", "rx_class_conf", " <port_list> eng_id <eng_idx> entype <entype>", "Configures Ingress Port Classifier" );
    cli_printf("\n %-20s| %-80s| %s", "rx_clock_conf", " <port_list> eng_id <eng_idx> clk_mode <clk_mode> dly_type <dly_type>", "Configures Ingress Port Clock" );
    cli_printf("\n %-20s| %-80s| %s", "ts_enable", " <port_list>", "Enables TS Block");
    cli_printf("\n %-20s| %-80s| %s", "ts_disable", " <port_list>", "Disables TS Block");
    cli_printf("\n %-20s| %-80s| %s", "ts_ltc_get", " <port_no>", "Get Local Time Counter");
    cli_printf("\n %-20s| %-80s| %s", "ts_ltc_set", " <port_no> time <sh:sl:ns:ps>", "Set Local Time Counter");
    cli_printf("\n %-20s| %-80s| %s", "ts_pps_incfg", " <port_no> clk_sel <clk_sel> sync_mode <sync_mode>", "Configure LS controller Input pps mode");
    cli_printf("\n %-20s| %-80s| %s", "", "ls_ctrl_sel <ls_ctrl_sel>", "");
    cli_printf("\n %-20s| %-80s| %s", "ts_pps_outcfg", " <port_no> clk_sel <clk_sel> out_mode <out_mode_sel>", "Configure LS controller Output pps mode");
    cli_printf("\n %-20s| %-80s| %s", "", "pps_wid <pps_wid> pps_in <pps_in> ls_ctrl_sel <ls_ctrl_sel>", "");
    cli_printf("\n %-20s| %-80s| %s", "ts_sertod_incfg", " <port_no> pin_sel <pin_sel> pol <pol>", "Configure LS controller Input Serial ToD");
    cli_printf("\n %-20s| %-80s| %s", "", " sync_mode <sync_mode> ls_ctrl_sel <ls_ctrl_sel> [load|save]", "");
    cli_printf("\n %-20s| %-80s| %s", "ts_sertod_outcfg", " <port_no> pin_sel <pin_sel> pol <pol>", "Configure LS controller output Serial ToD");
    cli_printf("\n %-20s| %-80s| %s", "", " sync_mode <sync_mode> wfh <wfh> wfl <wfl>", "");
    cli_printf("\n %-20s| %-80s| %s", "ts_delta_adj", " <port_no> adj <ns:sns> ls_ctrl_sel <ls_ctrl_sel>", "Configure LTC ToD adjustment in ns, sns");
    cli_printf("\n %-20s| %-80s| %s", "ts_epps_conf", " <port_no> clk_sel <clk_sel> sync_mode <sync_mode>", "Configure LS Controller Input EPPS mode");
    cli_printf("\n %-20s| %-80s| %s", "", " ls_ctrl_sel <ls_ctrl_sel> det_cfg <det_cfg>", "");
    cli_printf("\n %-20s| %-80s| %s", "ts_ltc_ls", " <port_no> pin_action <pin_action>", "Select LTC Operation mode");
    cli_printf("\n %-20s| %-80s| %s", "ts_fifo_get_empty", " <port_no>", "Get and clear FIFO TS Entry");
    cli_printf("\n %-20s| %-80s| %s", "ts_delay_set", " <port_no> timing_mode <timing_mode> delay <delay>", "Set Time Interval/latency in ns");
    cli_printf("\n %-20s| %-80s| %s", "ts_delay_get", " <port_no> timing_mode <timing_mode>", "Get Time Interval/latency in ns");
    cli_printf("\n %-20s| %-80s| %s", "ts_port_state", "", "Provides TS State of all Ports");
    cli_printf("\n %-20s| %-80s| %s", "ts_conf_get", " <port_no> conf_sel <conf_sel>", "Get TS Configurations");
    cli_printf("\n %-20s| %-80s| %s", "ts_port_stati", " <port_no>", "Get TS Port Statistics");
    cli_printf("\n %-20s| %-80s| %s", "ts_1pps_out_en", " <port_no> ls_ctrl_sel <ls_ctrl_sel>", "Enable 1PPS output signal");
	cli_printf("\n %-20s| %-80s| %s", "ts_sig_set", "<port_no> sig <sig_val>", "PTP FIFO Signature Configuration");
    cli_printf("\n %-20s| %-80s| %s", "ts_flow_dis", " <port_no> eng_id <eng_idx>", "Disable TX, RX Classifier and Clock config on PTP Engine");
    cli_printf("\n %-20s| %-80s| %s", "ts_csr_rd", " <port_no> mmd <dev-id> csr_addr <csraddr>", "TS CSR Register Read Command");
    cli_printf("\n %-20s| %-80s| %s", "ts_csr_wr", " <port_no> mmd <dev-id> csr_addr <csraddr> csr_value <csrvalue>", "TS CSR Register Write Command");
    cli_printf("\n\n");
    return;
}


static void cli_cmd_ts_demo(cli_req_t *req)
{
    cli_printf("\n\t\t\t\t \033[1;31m ================================================================================================================\033[0m\n");
    cli_printf("\t\t\t\t\t  \t\t\t\t \033[1;31mTIME STAMPING DEMO APPLICATION\033[0m\n");
    cli_printf("\n\t\t\t\t  \033[1;31m================================================================================================================\033[0m\n");
    cli_printf("\n\n");
    cli_cmd_ts_cmds();
    return;
}

static cli_cmd_t cli_cmd_ts_table[] = {
    {
        "exit_ts",
        "Exit Time Stamping Demo Application",
        cli_cmd_ts_demo,
    },

    {
        "?",
        "Lists the Available TS Commands",
        cli_cmd_ts_cmds,
    },

    {
        "ts_port_state",
        "Status of Time Stamping on all Ports",
        cli_cmd_ts_port_state,
    },

    {
        "ts_enable <port_list>",
        "Enable the TS Block",
        cli_cmd_ts_ena,
    },

    {
        "ts_disable <port_list>",
        "Disable the TS Block",
        cli_cmd_ts_dis,
    },

    {
        "ts_init_conf <port_list> clk_src <clk_src> tx_fifo <tx_fifo> tc_op <tc_op> dly_10b <dly_10b> tx_af <tx_af> mch <mch>",
        "Initialize TS Block",
        cli_cmd_ts_conf_init,
    },

    {
        "tx_class_conf <port_list> eng_id <eng_idx> entype <entype>",
        "Configures Tx classifier",
        cli_cmd_ts_tx_class_conf,
    },

    {
        "tx_clock_conf <port_list> eng_id <eng_idx> clk_mode <clk_mode> dly_type <dly_type>",
        "Configures Tx clock",
        cli_cmd_ts_tx_clock_conf,
    },

    {
        "rx_class_conf <port_list> eng_id <eng_idx> entype <entype>",
        "Configures Rx classifier",
        cli_cmd_ts_rx_class_conf,
    },

    {
        "rx_clock_conf <port_list> eng_id <eng_idx> clk_mode <clk_mode> dly_type <dly_type>",
        "Configures Rx clock",
        cli_cmd_ts_rx_clock_conf,
    },

    {
        "ts_ltc_ls_en <port_no> pin_action <pin_action>",
        "Configure Local Time Counter",
        cli_cmd_ts_ltc_ls,
    },

    {
        "ts_pps_incfg <port_no> clk_sel <clk_sel> sync_mode <sync_mode> ls_ctrl_sel <ls_ctrl_sel>",
        "Configure LSC pin for LS controller Input",
        cli_cmd_ts_pps_incfg,
    },

    {
        "ts_1pps_out_en <port_no> ls_ctrl_sel <ls_ctrl_sel>",
        " Enable 1PPS output on selected LSC Pin",
        cli_cmd_ts_1pps_out_en,
    },

    {
        "ts_pps_outcfg <port_no> clk_sel <clk_sel> out_mode <out_mode_sel> pps_wid <pps_wid> pps_in <pps_in> ls_ctrl_sel <ls_ctrl_sel>",
        "Configure LSC pin for LS controller Output",
        cli_cmd_ts_pps_outcfg,
    },

    {
        "ts_sertod_incfg <port_no> pin_sel <pin_sel> pol <pol> sync_mode <sync_mode> ls_ctrl_sel <ls_ctrl_sel> [load|save]",
        "Configure Serial ToD input",
        cli_cmd_ts_serial_incfg,
    },

    {
        "ts_sertod_outcfg <port_no> pin_sel <pin_sel> pol <pol> sync_mode <sync_mode> wfh <wfh> wfl <wfl>",
        "Configure Serial ToD output",
        cli_cmd_ts_serial_outcfg,
    },

    {
        "ts_delta_adj <port_no> adj <ns:sns> ls_ctrl_sel <ls_ctrl_sel>",
        "Configure Delta",
        cli_cmd_ts_delta_adj,
    },

    {
        "ts_epps_conf <port_no> clk_sel <clk_sel> sync_mode <sync_mode> ls_ctrl_sel <ls_ctrl_sel> det_cfg <det_cfg>",
        "Configure EPPS",
        cli_cmd_ts_epps_config,
    },

    {
        "ts_ltc_get <port_no> ",
        "Get Local Time Counter",
        cli_cmd_ts_ltc_get,
    },

    {
        "ts_ltc_set <port_no> time <sh:sl:ns:ps>",
        "Set Local Time Counter",
        cli_cmd_ts_ltc_set,
    },

    {
        "ts_fifo_get_empty <port_no>",
        "Get FIFO entries",
        cli_cmd_ts_fifo_get,
    },

    {
        "ts_delay_set <port_no> timing_mode <timing_mode> delay <delay>",
        "Set Time Interval",
        cli_cmd_ts_delay_set,
    },

    {
        "ts_delay_get <port_no> timing_mode <timing_mode>",
        "Get Time Interval",
        cli_cmd_ts_delay_get,
    },

    {
        "ts_port_stati <port_no>",
        "Get TS Port Statistics",
        cli_cmd_ts_port_stati,
    },
    {
        "ts_conf_get <port_no> conf_sel <conf_sel>",
        "Get TS Configurations",
        cli_cmd_ts_conf_get,
    },

    {
        "ts_sig_set <port_no> sig <sig_val>",
        "Configure TS FIFO Signature fields",
        cli_cmd_ts_sig_conf,
    },
    // {
    //     "ts_clk_rateadj_set <port_no> rateadj <rateadj>",
    //     "Set Clock frequency ratio scaled PartsPerBillion",
    //     cli_cmd_ts_clk_rateadj_set,
    // },
    // {
    //     "ts_clk_rateadj_get <port_no>",
    //     "Get Clock frequency ratio in scaled PartsPerBillion",
    //     cli_cmd_ts_clk_rateadj_get,
    // },
    {
        "ts_reset <port_no>",
        "Hard reset TS Block",
        cli_cmd_ts_reset,
    },
    {
        "ts_flow_dis <port_no> eng_id <eng_idx>",
        "Disable Tx, RX Classifier and Clock configuration",
        cli_cmd_ts_flow_disable,
    },
    {
        "ts_csr_rd <port_no> mmd <dev-id> csr_addr <csraddr>",
        "Read TS Block CSR Registers",
        cli_cmd_ts_csr_rd,
    },
    {
        "ts_csr_wr <port_no> mmd <dev-id> csr_addr <csraddr> csr_value <csrvalue>",
        "Write TS Block CSR Registers",
        cli_cmd_ts_csr_wr,
    },

};

#if 0
static cli_cmd_t cli_cmd_table[] = {
    {
        "ts demo",
        "Time Stamping Operational Demo",
        cli_cmd_ts_demo,
    },
    {
        "ts init <port_list>",
        "TS init configuration",
        cli_cmd_ts_init_f,
    },
    {
        "ts tx_class <port_list>",
        "Configures Tx classifier",
        cli_cmd_ts_tx_class_f,
    },
    {
        "ts tx_clock <port_list>",
        "Configures Tx clock",
        cli_cmd_ts_tx_clock_f,
    },
    {
        "ts rx_class <port_list>",
        "Configures Tx classifier",
        cli_cmd_ts_rx_class_f,
    },
    {
        "ts rx_clock <port_list>",
        "Configures Tx clock",
        cli_cmd_ts_rx_clock_f,
    },
    {
        "ts enable <port_list>",
        "Enable the TS Block",
        cli_cmd_ts_ena,
    },

    {
        "ts disable <port_list>",
        "Disable the TS Block",
        cli_cmd_ts_dis,
    },
};
#endif

static cli_cmd_t cli_cmd_table[] = {
    {
        "ts",
        "Time Stamping Operational Demo",
        cli_cmd_ts_demo,
    },
};

static cli_parm_t cli_parm_table[] = {
    {
        "clk_src",
        "Clock Source",
        CLI_PARM_FLAG_NO_TXT,
        cli_cmd_parse_keyword,
    },
    {
        "<clk_src>",
        "0 : Internal, 1 : Rx Port 0, 2 : Rx Port 1, 3 : Rx Port 2, 4 : Rx Port 3, 5 : External",
        CLI_PARM_FLAG_SET,
        cli_cmd_parse_clksrc,

    },
    {
        "tx_fifo",
        "Tx FIFO Mode",
        CLI_PARM_FLAG_NO_TXT,
        cli_cmd_parse_keyword,
    },
    {
        "<tx_fifo>",
        "0 - Normal, 1 - SPI",
        CLI_PARM_FLAG_SET,
        cli_cmd_parse_tx_fifo,

    },
    {
        "tc_op",
        "Tc Op Mode",
        CLI_PARM_FLAG_NO_TXT,
        cli_cmd_parse_keyword,
    },
    {
        "<tc_op>",
        "0 - Mode A, 1 - Mode B, 2 - Mode C",
        CLI_PARM_FLAG_SET,
        cli_cmd_parse_tc_op,

    },
    {
        "dly_10b",
        "Delay request message",
        CLI_PARM_FLAG_NO_TXT,
        cli_cmd_parse_keyword,
    },
    {
        "<dly_10b>",
        "0 - Disable, 1 - Store 10-byte ingress timestamp for delay request message",
        CLI_PARM_FLAG_NONE,
        cli_cmd_parse_boolean,
    },
    {
        "tx_af",
        "",
        CLI_PARM_FLAG_NO_TXT,
        cli_cmd_parse_keyword,
    },
    {
        "<tx_af>",
        "0 - Disable, 1 - Enable",
        CLI_PARM_FLAG_NONE,
        cli_cmd_parse_boolean,
    },
    {
        "mch",
        "",
        CLI_PARM_FLAG_NO_TXT,
        cli_cmd_parse_keyword,
    },
    {
        "<mch>",
        "0 - Disable, 1 - Enable",
        CLI_PARM_FLAG_NONE,
        cli_cmd_parse_boolean,
    },
    {
        "eng_id",
        "",
        CLI_PARM_FLAG_NO_TXT,
        cli_cmd_parse_keyword,
    },
    {
        "<eng_idx>",
        "PTP Engine Number (Engine 0 or 1 or 2)",
        CLI_PARM_FLAG_NONE,
        cli_cmd_parse_u16_param,
    },
    {
        "entype",
        "",
        CLI_PARM_FLAG_NO_TXT,
        cli_cmd_parse_keyword,
    },
    {
        "<entype>",
        "0 : None, 1 : Eth PTP, 2 : Eth IP PTP, 3 : Eth IP IP PTP, 4 : Eth Eth PTP, 5 : Eth Eth IP PTP,\n \
        6 : Eth MPLS IP PTP, 7 : Eth MPLS Eth PTP, 8 : Eth MPLS Eth IP PTP",
        CLI_PARM_FLAG_SET,
        cli_cmd_parse_encap,
    },
    {
        "clk_mode",
        "",
        CLI_PARM_FLAG_NO_TXT,
        cli_cmd_parse_keyword,
    },
    {
        "<clk_mode>",
        "0 - None, 1 - BC1STEP, 2 - BC2STEP, 3 - TC1STEP, 4 - TC2STEP",
        CLI_PARM_FLAG_NONE,
        cli_cmd_parse_u8_param,
    },
    {
        "dly_type",
        "",
        CLI_PARM_FLAG_NO_TXT,
        cli_cmd_parse_keyword,
    },
    {
        "<dly_type>",
        "0 - Peer-to-Peer, 1- End-to-End",
        CLI_PARM_FLAG_NONE,
        cli_cmd_parse_u8_param,
    },
    {
        "load|save",
        "\n\t load    : Load Local Time Clock\n"
        "\t save    : Save Local Time Clock \n",
        CLI_PARM_FLAG_NONE,
        cli_cmd_parse_boolean,
    },
    {
        "pin_action",
        "",
        CLI_PARM_FLAG_NO_TXT,
        cli_cmd_parse_keyword,
    },
    {
        "<pin_action>",
        "0 - Load, 1 - Save, 2 - Delta,  3 - Time of Day",
        CLI_PARM_FLAG_NONE,
        cli_cmd_parse_u8_param,
    },
    {
        "ls_ctrl_sel",
        "",
        CLI_PARM_FLAG_NO_TXT,
        cli_cmd_parse_keyword,
    },
    {
        "<ls_ctrl_sel>",
        "0 - LS0, 1 - LS1, 2 - LS2, 3 - LS3",
        CLI_PARM_FLAG_NONE,
        cli_cmd_parse_u8_param,
    },
    {
        "ns_en",
        "",
        CLI_PARM_FLAG_NO_TXT,
        cli_cmd_parse_keyword,
    },
    {
        "<ns_en>",
        "Nanosecond bitout Enable",
        CLI_PARM_FLAG_NONE,
        cli_cmd_parse_boolean,
    },
    {
        "pps_wid",
        "",
        CLI_PARM_FLAG_NO_TXT,
        cli_cmd_parse_keyword,
    },
    {
        "<pps_wid>",
        "PPS Pulse High Width",
        CLI_PARM_FLAG_NONE,
        cli_cmd_parse_u32_param,
    },
    {
        "out_mode",
        "",
        CLI_PARM_FLAG_NO_TXT,
        cli_cmd_parse_keyword,
    },
    {
        "<out_mode_sel>",
        "0 - Square Wave,  1 - 1PPS, 2 - NSEC Bit Out",
        CLI_PARM_FLAG_NONE,
        cli_cmd_parse_u8_param,
    },
    {
        "pps_in",
        "",
        CLI_PARM_FLAG_NO_TXT,
        cli_cmd_parse_keyword,
    },
    {
        "<pps_in>",
        "Waveform low width or 1PPS Pulse start boundary in NSEC",
        CLI_PARM_FLAG_NONE,
        cli_cmd_parse_u32_param,
    },
    {
        "wfh",
        "",
        CLI_PARM_FLAG_NO_TXT,
        cli_cmd_parse_keyword,
    },
    {
        "<wfh>",
        "Waveform High Period",
        CLI_PARM_FLAG_NONE,
        cli_cmd_parse_u32_param,
    },
    {
        "wfl",
        "",
        CLI_PARM_FLAG_NO_TXT,
        cli_cmd_parse_keyword,
    },
    {
        "<wfl>",
        "Waveform Low Period",
        CLI_PARM_FLAG_NONE,
        cli_cmd_parse_u32_param,
    },
    {
        "adj",
        "",
        CLI_PARM_FLAG_NO_TXT,
        cli_cmd_parse_keyword,
    },
    {
        "<ns:sns>",
        "ns - nanoseconds, sns - sub nanoseconds\n",
        CLI_PARM_FLAG_NONE,
        cli_cmd_parse_ltc_time,
    },
    {
        "time",
        "",
        CLI_PARM_FLAG_NO_TXT,
        cli_cmd_parse_keyword,
    },
    {
        "<sh:sl:ns:ps>",
        "sh - seconds high, sl - seconds low, ns - nanoseconds, ps - picoseconds\n",
        CLI_PARM_FLAG_NONE,
        cli_cmd_parse_ltc_time,
    },
    {
        "sig",
        "",
        CLI_PARM_FLAG_NO_TXT,
        cli_cmd_parse_keyword,
    },
    {
        "<sig_val>",
        "1 - Seq ID, Message type, Domain Number \n \
         2 - Seq ID, Message type, Domain Number, Source Port ID \n \
         3 - Seq ID, Message type, Domain Number, IPv4 Address \n \
         4 - Seq ID, Message type, Domain Number, Source Port ID, IPv4 Address \n \
         5 - Seq ID, Message type, Domain Number, DMAC Address \n \
         6 - Seq ID, Message type, Domain Number, DMAC Address, IPv4 Address \n \
         7 - Seq ID, Message type, Domain Number, IPv4 Address, IPv6 Address \n",
        CLI_PARM_FLAG_NONE,
        cli_cmd_parse_u16_param,
    },
    {
        "timing_mode",
        "",
        CLI_PARM_FLAG_NO_TXT,
        cli_cmd_parse_keyword,
    },
    {
        "<timing_mode>",
        "0 - Delay Asymmetry, 1 - Path Delay, 2 - Egress Latency, 3 - Ingress Latency\n",
        CLI_PARM_FLAG_NONE,
        cli_cmd_parse_u8_param,
    },
    {
        "delay",
        "",
        CLI_PARM_FLAG_NO_TXT,
        cli_cmd_parse_keyword,
    },
    {
        "<delay>",
        "Time Interval in ns\n",
        CLI_PARM_FLAG_NONE,
        cli_cmd_parse_floating_point,
    },
    {
        "conf_sel",
        "",
        CLI_PARM_FLAG_NO_TXT,
        cli_cmd_parse_keyword,
    },
    {
        "<conf_sel>",
        "0 - Init Config, 1 - Tx Classifier Config, 2 - Tx Clock Config, \n \
        3 - Rx Classifier Config, 4 - Rx Clock Config, 5 - Signature Field\n",
        CLI_PARM_FLAG_NONE,
        cli_cmd_parse_u8_param,
    },
    {
        "det_cfg",
        "",
        CLI_PARM_FLAG_NO_TXT,
        cli_cmd_parse_keyword,
    },
    {
        "<det_cfg>",
        "ePPS event cycle detection adjustment",
        CLI_PARM_FLAG_NONE,
        cli_cmd_parse_u8_param,
    },
    {
        "rateadj",
        "",
        CLI_PARM_FLAG_NO_TXT,
        cli_cmd_parse_keyword,
    },
    {
        "<rateadj>",
        "Clock Rate adjust in ppb",
        CLI_PARM_FLAG_NONE,
        cli_cmd_parse_floating_point,
    },
    {
        "clk_sel",
        "",
        CLI_PARM_FLAG_NO_TXT,
        cli_cmd_parse_keyword,
    },
    {
        "<clk_sel>",
        "0 - PTP_LSC_PIN[0], 1 - PTP_LSC_PIN[1], 2 - PTP_LSC_PIN[2], 3 - PTP_LSC_PIN[3], \n\
         4 - No Clock(Asyncronous mode), 5 - L/S reference clock, 6 - L/S reference clock div2, 7 - L/S reference clock div4 \n\
         8 - L/R reference clock div5, 9 - L/R reference clock div8",
        CLI_PARM_FLAG_NONE,
        cli_cmd_parse_u8_param,
    },
    {
        "pin_sel",
        "",
        CLI_PARM_FLAG_NO_TXT,
        cli_cmd_parse_keyword,
    },
    {
        "<pin_sel>",
        "0 - PTP_LSC_PIN[0], 1 - PTP_LSC_PIN[1], 2 - PTP_LSC_PIN[2], 3 - PTP_LSC_PIN[3]",
        CLI_PARM_FLAG_NONE,
        cli_cmd_parse_u8_param,
    },
    {
        "pol",
        "",
        CLI_PARM_FLAG_NO_TXT,
        cli_cmd_parse_keyword,
    },
    {
        "<pol>",
        "0 - Active High, 1 - Active Low",
        CLI_PARM_FLAG_NONE,
        cli_cmd_parse_boolean,
    },
    {
        "sync_mode",
        "",
        CLI_PARM_FLAG_NO_TXT,
        cli_cmd_parse_keyword,
    },
    {
        "<sync_mode>",
        "0 - Immediate, 1 - One Shot, 2 - Continuous",
        CLI_PARM_FLAG_NONE,
        cli_cmd_parse_u8_param,
    },
    {
        "mmd",
        "MMD ID for TS Block Registers",
        CLI_PARM_FLAG_NO_TXT,
        cli_cmd_parse_keyword,
    },
    {
        "<dev-id>",
        "",
        CLI_PARM_FLAG_NONE,
        cli_cmd_parse_u16_param,
    },
    {
        "csr_addr",
        "CSR Register Address for TS Block",
        CLI_PARM_FLAG_NO_TXT,
        cli_cmd_parse_keyword,
    },
    {
        "<csraddr>",
        "",
        CLI_PARM_FLAG_NONE,
        cli_cmd_parse_u16_param,
    },
    {
        "csr_value",
        "",
        CLI_PARM_FLAG_NO_TXT,
        cli_cmd_parse_keyword,
    },
    {
        "<csrvalue>",
        "",
        CLI_PARM_FLAG_NONE,
        cli_cmd_parse_u32_param,
    },

};


static void phy_cli_init(void)
{
    int i;
    /* Register CLI Commands */
    for (i = 0; i < sizeof(cli_cmd_table) / sizeof(cli_cmd_t); i++) {
        mscc_appl_cli_cmd_reg(&cli_cmd_table[i]);
    }

    /* Register TS Commands */
    for (i = 0; i < sizeof(cli_cmd_ts_table) / sizeof(cli_cmd_t); i++) {
        mscc_appl_ts_cli_cmd_reg(&cli_cmd_ts_table[i]);
    }

    /* Register CLI Params */
    for (i = 0; i < sizeof(cli_parm_table) / sizeof(cli_parm_t); i++) {
        mscc_appl_cli_parm_reg(&cli_parm_table[i]);
    }

}

void mepa_demo_appl_ts_demo(mscc_appl_init_t *init)
{
    meba_ts_instance = init->board_inst;
    switch (init->cmd) {
    case MSCC_INIT_CMD_INIT:
        phy_cli_init();
        break;
    default:
        break;
    }
}
