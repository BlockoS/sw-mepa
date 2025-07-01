// Copyright (c) 2004-2020 Microchip Technology Inc. and its subsidiaries.
// SPDX-License-Identifier: MIT

#include <stdio.h>
#include <ctype.h>
#include "microchip/ethernet/switch/api.h"
#include "microchip/ethernet/board/api.h"
#include "main.h"
#include "mesa-rpc.h"
#include "trace.h"
#include "cli.h"
#include "port.h"
#include "phy_demo_apps.h"
#include "lan80xx.h"
#include <unistd.h>
#include "lan80xx_mcu.h"
#include "phy_kr_aneg_demo.h"


meba_inst_t meba_phy_kr_inst;

static mscc_appl_trace_module_t trace_module = {
    .name = "phy_kr_aneg"
};

enum {
    TRACE_GROUP_DEFAULT,
    TRACE_GROUP_CNT
};

static mscc_appl_trace_group_t trace_groups[TRACE_GROUP_CNT] = {
    // TRACE_GROUP_DEFAULT
    {
        .name = "default",
        .level = MESA_TRACE_LEVEL_ERROR
    },
};

typedef struct {
    mesa_bool_t adv1g;
    mesa_bool_t adv10g;
    mesa_bool_t adv25g_kr;
    mesa_bool_t adv25g_krs;
    mesa_bool_t rfec_10g;
    mesa_bool_t rfec_25g;
    mesa_bool_t rsfec_25g;
    mesa_bool_t np;
    mesa_bool_t np_rfec;
    mesa_bool_t np_rsfec;
    mesa_bool_t fw_res;
    mesa_bool_t train;
    uint32_t    value;
    mesa_bool_t dis;
} phy_kr_cli_req_t;

typedef struct phy_sd_cli_req {
    // 0 - 1G, 1 - 10G, 2 - 25G
    uint8_t    speed_idx;

} phy_sd_cli_req_t;

typedef struct phy_kr_log_sel {
    // Fields for KR Log state
    mesa_bool_t krlog_enable;

    // Fields for Port Select
    mesa_bool_t host_port;
    mesa_bool_t line_port;

    // Fields for Log Select
    mesa_bool_t aneg;
    mesa_bool_t eq;
    mesa_bool_t ber;
    mesa_bool_t irq;
    mesa_bool_t all;
    mesa_bool_t clr;
} phy_kr_log_sel_t;

////////////////////////////////////////////////////////////
/////////////////   CLI Command functions   ////////////////
////////////////////////////////////////////////////////////

static mepa_rc Islinkup(int port_no, mepa_adv_side_t port_type, u8 *speed)
{
    mepa_rc rc = MESA_RC_ERROR;
    uint32_t value = 0, mmd = 0;

    if (port_type == MEPA_ADV_SIDE_LINE) {
        mmd = 0x07;
    } else if (port_type == MEPA_ADV_SIDE_HOST) {
        mmd = 0x0F;
    } else {
        return rc;
    }

    if ((rc = lan80xx_phy_csr_read(meba_phy_kr_inst->phy_devices[port_no], port_no, mmd, 0x01, &value)) != MEPA_RC_OK) {
        T_E("\n Error Reading a Register on port : %d \n", (port_no));
        return rc;
    }
    cli_printf ("AN_STS0: 0x%02X\n", value);
    if (value & 0x20) {
        cli_printf("ANEG completed\n");
    } else {
        cli_printf ("Link is not up\n");
        return MESA_RC_ERROR;
    }

    if ((rc = lan80xx_phy_csr_read(meba_phy_kr_inst->phy_devices[port_no], port_no, mmd, 0x8032, &value)) != MEPA_RC_OK) {
        T_E("\n Error Reading a Register on port : %d \n", (port_no));
        return MESA_RC_ERROR;
    }
    cli_printf ("AN_STS1: 0x%02X\n", value);
    *speed = value & 0x0f;

    rc = MESA_RC_OK;
    return rc;
}

void dump_sd_cfg(__SERDES_CONFIG_T cfg, eSERDES_CFG_T cfgType)
{
    switch (cfgType) {
    case eTX_EQ_CFG:
        printf("TX EQ Config:\n");
        printf("  Tap_dly: 0x%02X\n", cfg.sTx_eq_cfg.Tap_dly);
        printf("  Tap_main: 0x%02X\n", cfg.sTx_eq_cfg.Tap_main);
        printf("  Tap_adv: 0x%02X\n", cfg.sTx_eq_cfg.Tap_adv);
        printf("  En_main: 0x%02X\n", cfg.sTx_eq_cfg.En_main);
        printf("  En_adv: 0x%02X\n", cfg.sTx_eq_cfg.En_adv);
        printf("  En_dly: 0x%02X\n", cfg.sTx_eq_cfg.En_dly);
        break;
    case eCDR_CFG:
        printf("CDR Config:\n");
        printf("  Cdr_m: 0x%02X\n", cfg.sCdr_cfg.Cdr_m);
        printf("  Alos_thr: 0x%02X\n", cfg.sCdr_cfg.Alos_thr);
        break;

    case eSPEED_CHANGE_CFG:
        printf("Speed Change Config:\n");
        printf("  L0_cfg_tx_reserve_15_8: 0x%02X\n", cfg.sSpeed_change_cfg.L0_cfg_tx_reserve_15_8);
        printf("  L0_cfg_tx_reserve_7_0: 0x%02X\n", cfg.sSpeed_change_cfg.L0_cfg_tx_reserve_7_0);
        printf("  Ln_cfg_tx_reserve_15_8: 0x%02X\n", cfg.sSpeed_change_cfg.Ln_cfg_tx_reserve_15_8);
        printf("  Ln_cfg_tx_reserve_7_0: 0x%02X\n", cfg.sSpeed_change_cfg.Ln_cfg_tx_reserve_7_0);
        break;

    case eRX_EQ_CFG:
        printf("RX EQ Config:\n");
        printf("  Ln_cfg_vga_ctrl_byp: 0x%02X\n", cfg.sRx_eq_cfg.Ln_cfg_vga_ctrl_byp);
        printf("  Ln_cfg_vga_byp: 0x%02X\n", cfg.sRx_eq_cfg.Ln_cfg_vga_byp);
        printf("  Ln_cfg_eqr_force: 0x%02X\n", cfg.sRx_eq_cfg.Ln_cfg_eqr_force);
        printf("  Ln_cfg_eqc_force: 0x%02X\n", cfg.sRx_eq_cfg.Ln_cfg_eqc_force);
        break;

    case eDFE_CFG:
        printf("DFE Config:\n");
        printf("  Ln_cfg_dfeck_en: 0x%02X\n", cfg.sDfe_cfg.Ln_cfg_dfeck_en);
        printf("  Ln_cfg_dfe_pd: 0x%02X\n", cfg.sDfe_cfg.Ln_cfg_dfe_pd);
        printf("  Ln_cfg_dfemx_pd: 0x%02X\n", cfg.sDfe_cfg.Ln_cfg_dfemx_pd);
        printf("  Ln_cfg_dfe_dmux_pd: 0x%02X\n", cfg.sDfe_cfg.Ln_cfg_dfe_dmux_pd);
        printf("  Ln_cfg_pi_dfe_en: 0x%02X\n", cfg.sDfe_cfg.Ln_cfg_pi_dfe_en);
        printf("  Ln_cfg_dfedig_m: 0x%02X\n", cfg.sDfe_cfg.Ln_cfg_dfedig_m);
        printf("  Ln_cfg_dfetap_en: 0x%02X\n", cfg.sDfe_cfg.Ln_cfg_dfetap_en);
        printf("  Ln_cfg_en_dfedig: 0x%02X\n", cfg.sDfe_cfg.Ln_cfg_en_dfedig);
        break;

    case ePOL_SQ_CFG:
        printf("Polarity and Signal Quality Config:\n");
        printf("  Tx_pol_inv: 0x%02X\n", cfg.sPol_sq_cfg.Tx_pol_inv);
        printf("  Rx_pol_inv: 0x%02X\n", cfg.sPol_sq_cfg.Rx_pol_inv);
        printf("  Ln_cfg_dis_sq: 0x%02X\n", cfg.sPol_sq_cfg.Ln_cfg_dis_sq);
        printf("  Ln_cfg_pd_sq: 0x%02X\n", cfg.sPol_sq_cfg.Ln_cfg_pd_sq);
        break;

    case eTX_SWING_CFG:
        printf("TX Swing Config:\n");
        printf("  Itx_ipdriver_base: 0x%02X\n", cfg.sTx_swing_cfg.Itx_ipdriver_base);
        break;

    case eMISC_CFG:
        printf("Misc Config:\n");
        printf("  Cfg_common_reserve: 0x%02X\n", cfg.sMisc_cfg.Cfg_common_reserve);
        printf("  Cfg_jc_byp: 0x%02X\n", cfg.sMisc_cfg.Cfg_jc_byp);
        printf("  Cfg_pll_lol_set: 0x%02X\n", cfg.sMisc_cfg.Cfg_pll_lol_set);
        printf("  Cfg_en_dummy: 0x%02X\n", cfg.sMisc_cfg.Cfg_en_dummy);
        printf("  Cfg_pll_reserve: 0x%02X\n", cfg.sMisc_cfg.Cfg_pll_reserve);
        printf("  R_dwidthctrl_from_hwt: 0x%02X\n", cfg.sMisc_cfg.R_dwidthctrl_from_hwt);
        printf("  R_reg_manual: 0x%02X\n", cfg.sMisc_cfg.R_reg_manual);
        printf("  Vco_div_mode: 0x%02X\n", cfg.sMisc_cfg.Vco_div_mode);
        printf("  Pre_divsel: 0x%02X\n", cfg.sMisc_cfg.Pre_divsel);
        printf("  Cfg_seldiv: 0x%02X\n", cfg.sMisc_cfg.Cfg_seldiv);
        printf("  Data_width_sel: 0x%02X\n", cfg.sMisc_cfg.Data_width_sel);
        printf("  Txfifo_ck_div: 0x%02X\n", cfg.sMisc_cfg.Txfifo_ck_div);
        printf("  Rxfifo_ck_div: 0x%02X\n", cfg.sMisc_cfg.Rxfifo_ck_div);
        printf("  Pma_txck_sel: 0x%02X\n", cfg.sMisc_cfg.Pma_txck_sel);
        printf("  Tx_prediv: 0x%02X\n", cfg.sMisc_cfg.Tx_prediv);
        printf("  Rxdiv_sel: 0x%02X\n", cfg.sMisc_cfg.Rxdiv_sel);
        printf("  Txrate_sel: 0x%02X\n", cfg.sMisc_cfg.Txrate_sel);
        printf("  Rxrate_sel: 0x%02X\n", cfg.sMisc_cfg.Rxrate_sel);
        printf("  Ln_cfg_cdrck_en: 0x%02X\n", cfg.sMisc_cfg.Ln_cfg_cdrck_en);
        printf("  Ln_cfg_dmux_pd: 0x%02X\n", cfg.sMisc_cfg.Ln_cfg_dmux_pd);
        printf("  Ln_cfg_dmux_clk_pd: 0x%02X\n", cfg.sMisc_cfg.Ln_cfg_dmux_clk_pd);
        printf("  Ln_cfg_erramp_pd: 0x%02X\n", cfg.sMisc_cfg.Ln_cfg_erramp_pd);
        printf("  Ln_cfg_pi_en: 0x%02X\n", cfg.sMisc_cfg.Ln_cfg_pi_en);
        printf("  Ln_cfg_pd_ctle: 0x%02X\n", cfg.sMisc_cfg.Ln_cfg_pd_ctle);
        printf("  Ln_cfg_summer_en: 0x%02X\n", cfg.sMisc_cfg.Ln_cfg_summer_en);
        printf("  Ln_cfg_pmad_ck_pd: 0x%02X\n", cfg.sMisc_cfg.Ln_cfg_pmad_ck_pd);
        printf("  Ln_cfg_pd_clk: 0x%02X\n", cfg.sMisc_cfg.Ln_cfg_pd_clk);
        printf("  Ln_cfg_pd_cml: 0x%02X\n", cfg.sMisc_cfg.Ln_cfg_pd_cml);
        printf("  Ln_cfg_pd_driver: 0x%02X\n", cfg.sMisc_cfg.Ln_cfg_pd_driver);
        printf("  Ln_cfg_rx_reg_pu: 0x%02X\n", cfg.sMisc_cfg.Ln_cfg_rx_reg_pu);
        printf("  Ln_cfg_pd_rms_det: 0x%02X\n", cfg.sMisc_cfg.Ln_cfg_pd_rms_det);
        printf("  Ln_cfg_dcdr_pd: 0x%02X\n", cfg.sMisc_cfg.Ln_cfg_dcdr_pd);
        printf("  Ln_cfg_ecdr_pd: 0x%02X\n", cfg.sMisc_cfg.Ln_cfg_ecdr_pd);
        printf("  L0_cfg_bw: 0x%02X\n", cfg.sMisc_cfg.L0_cfg_bw);
        printf("  L0_cfg_txcal_en: 0x%02X\n", cfg.sMisc_cfg.L0_cfg_txcal_en);
        printf("  Ln_cfg_bw: 0x%02X\n", cfg.sMisc_cfg.Ln_cfg_bw);
        printf("  Ln_cfg_txcal_man_en: 0x%02X\n", cfg.sMisc_cfg.Ln_cfg_txcal_man_en);
        printf("  Ln_cfg_phase_man: 0x%02X\n", cfg.sMisc_cfg.Ln_cfg_phase_man);
        printf("  Ln_cfg_quad_man: 0x%02X\n", cfg.sMisc_cfg.Ln_cfg_quad_man);
        printf("  Ln_cfg_txcal_shift_code: 0x%02X\n", cfg.sMisc_cfg.Ln_cfg_txcal_shift_code);
        printf("  Ln_cfg_txcal_valid_sel: 0x%02X\n", cfg.sMisc_cfg.Ln_cfg_txcal_valid_sel);
        printf("  Ln_cfg_cdr_kf: 0x%02X\n", cfg.sMisc_cfg.Ln_cfg_cdr_kf);
        printf("  Ln_cfg_pi_bw: 0x%02X\n", cfg.sMisc_cfg.Ln_cfg_pi_bw);
        printf("  Ln_cfg_pi_steps: 0x%02X\n", cfg.sMisc_cfg.Ln_cfg_pi_steps);
        printf("  Ln_cfg_dis_2ndorder: 0x%02X\n", cfg.sMisc_cfg.Ln_cfg_dis_2ndorder);
        printf("  Ln_cfg_rx_reserve_7_0: 0x%02X\n", cfg.sMisc_cfg.Ln_cfg_rx_reserve_7_0);
        printf("  Ln_cfg_rx_reserve_15_8: 0x%02X\n", cfg.sMisc_cfg.Ln_cfg_rx_reserve_15_8);
        printf("  Ln_cfg_rx_term: 0x%02X\n", cfg.sMisc_cfg.Ln_cfg_rx_term);
        printf("  Ln_cfg_rx_sp_ctle: 0x%02X\n", cfg.sMisc_cfg.Ln_cfg_rx_sp_ctle);
        printf("  Ln_cfg_isel_ctle: 0x%02X\n", cfg.sMisc_cfg.Ln_cfg_isel_ctle);
        printf("  Ln_cfg_eqr_byp: 0x%02X\n", cfg.sMisc_cfg.Ln_cfg_eqr_byp);
        printf("  Ln_cfg_agc_adpt_byp: 0x%02X\n", cfg.sMisc_cfg.Ln_cfg_agc_adpt_byp);
        printf("  Ln_cfg_sum_setcm_en: 0x%02X\n", cfg.sMisc_cfg.Ln_cfg_sum_setcm_en);
        printf("  Ln_cfg_init_pos_iscan: 0x%02X\n", cfg.sMisc_cfg.Ln_cfg_init_pos_iscan);
        printf("  Ln_cfg_init_pos_iPI: 0x%02X\n", cfg.sMisc_cfg.Ln_cfg_init_pos_iPI);
        printf("  Cfg_i_vco: 0x%02X\n", cfg.sMisc_cfg.Cfg_i_vco);
        printf("  Icp_base_sel: 0x%02X\n", cfg.sMisc_cfg.Icp_base_sel);
        printf("  Icp_sel: 0x%02X\n", cfg.sMisc_cfg.Icp_sel);
        printf("  Cfg_rsel: 0x%02X\n", cfg.sMisc_cfg.Cfg_rsel);
        printf("  Ln_cfg_iscan_en: 0x%02X\n", cfg.sMisc_cfg.Ln_cfg_iscan_en);
        printf("  Ln_cfg_en_fast_iscan: 0x%02X\n", cfg.sMisc_cfg.Ln_cfg_en_fast_iscan);
        printf("  Ln_cfg_filter2nd_yz_6_0: 0x%02X\n", cfg.sMisc_cfg.Ln_cfg_filter2nd_yz_6_0);
        printf("  Ln_r_alos_en: 0x%02X\n", cfg.sMisc_cfg.Ln_r_alos_en);
        break;

    case eALL_CFG:
        dump_sd_cfg(cfg, eTX_EQ_CFG);
        dump_sd_cfg(cfg, eCDR_CFG);
        dump_sd_cfg(cfg, eSPEED_CHANGE_CFG);
        dump_sd_cfg(cfg, eRX_EQ_CFG);
        dump_sd_cfg(cfg, eDFE_CFG);
        dump_sd_cfg(cfg, ePOL_SQ_CFG);
        dump_sd_cfg(cfg, eTX_SWING_CFG);
        dump_sd_cfg(cfg, eMISC_CFG);
        break;

    case eUNKNOWN_CFG:
    default:
        break;
    }
}

static int write_serdes_config(const char *filename, const __SERDES_CONFIG_T *cfg)
{
    FILE *f = fopen(filename, "w");
    if (!f) {
        return -1;
    }

    // tx_eq_cfg
    fprintf(f, "sTx_eq_cfg.Tap_dly=0x%x\n", cfg->sTx_eq_cfg.Tap_dly);
    fprintf(f, "sTx_eq_cfg.Tap_main=0x%x\n", cfg->sTx_eq_cfg.Tap_main);
    fprintf(f, "sTx_eq_cfg.Tap_adv=0x%x\n", cfg->sTx_eq_cfg.Tap_adv);
    fprintf(f, "sTx_eq_cfg.En_main=0x%x\n", cfg->sTx_eq_cfg.En_main);
    fprintf(f, "sTx_eq_cfg.En_adv=0x%x\n", cfg->sTx_eq_cfg.En_adv);
    fprintf(f, "sTx_eq_cfg.En_dly=0x%x\n", cfg->sTx_eq_cfg.En_dly);

    // cdr_cfg
    fprintf(f, "sCdr_cfg.Cdr_m=0x%x\n", cfg->sCdr_cfg.Cdr_m);
    fprintf(f, "sCdr_cfg.Alos_thr=0x%x\n", cfg->sCdr_cfg.Alos_thr);

    // speed_change_cfg
    fprintf(f, "sSpeed_change_cfg.L0_cfg_tx_reserve_15_8=0x%x\n", cfg->sSpeed_change_cfg.L0_cfg_tx_reserve_15_8);
    fprintf(f, "sSpeed_change_cfg.L0_cfg_tx_reserve_7_0=0x%x\n", cfg->sSpeed_change_cfg.L0_cfg_tx_reserve_7_0);
    fprintf(f, "sSpeed_change_cfg.Ln_cfg_tx_reserve_15_8=0x%x\n", cfg->sSpeed_change_cfg.Ln_cfg_tx_reserve_15_8);
    fprintf(f, "sSpeed_change_cfg.Ln_cfg_tx_reserve_7_0=0x%x\n", cfg->sSpeed_change_cfg.Ln_cfg_tx_reserve_7_0);

    // rx_eq_cfg
    fprintf(f, "sRx_eq_cfg.Ln_cfg_vga_ctrl_byp=0x%x\n", cfg->sRx_eq_cfg.Ln_cfg_vga_ctrl_byp);
    fprintf(f, "sRx_eq_cfg.Ln_cfg_vga_byp=0x%x\n", cfg->sRx_eq_cfg.Ln_cfg_vga_byp);
    fprintf(f, "sRx_eq_cfg.Ln_cfg_eqr_force=0x%x\n", cfg->sRx_eq_cfg.Ln_cfg_eqr_force);
    fprintf(f, "sRx_eq_cfg.Ln_cfg_eqc_force=0x%x\n", cfg->sRx_eq_cfg.Ln_cfg_eqc_force);

    // dfe_cfg
    fprintf(f, "sDfe_cfg.Ln_cfg_dfeck_en=0x%x\n", cfg->sDfe_cfg.Ln_cfg_dfeck_en);
    fprintf(f, "sDfe_cfg.Ln_cfg_dfe_pd=0x%x\n", cfg->sDfe_cfg.Ln_cfg_dfe_pd);
    fprintf(f, "sDfe_cfg.Ln_cfg_dfemx_pd=0x%x\n", cfg->sDfe_cfg.Ln_cfg_dfemx_pd);
    fprintf(f, "sDfe_cfg.Ln_cfg_dfe_dmux_pd=0x%x\n", cfg->sDfe_cfg.Ln_cfg_dfe_dmux_pd);
    fprintf(f, "sDfe_cfg.Ln_cfg_pi_dfe_en=0x%x\n", cfg->sDfe_cfg.Ln_cfg_pi_dfe_en);
    fprintf(f, "sDfe_cfg.Ln_cfg_dfedig_m=0x%x\n", cfg->sDfe_cfg.Ln_cfg_dfedig_m);
    fprintf(f, "sDfe_cfg.Ln_cfg_dfetap_en=0x%x\n", cfg->sDfe_cfg.Ln_cfg_dfetap_en);
    fprintf(f, "sDfe_cfg.Ln_cfg_en_dfedig=0x%x\n", cfg->sDfe_cfg.Ln_cfg_en_dfedig);

    // pol_sq_cfg
    fprintf(f, "sPol_sq_cfg.Tx_pol_inv=0x%x\n", cfg->sPol_sq_cfg.Tx_pol_inv);
    fprintf(f, "sPol_sq_cfg.Rx_pol_inv=0x%x\n", cfg->sPol_sq_cfg.Rx_pol_inv);
    fprintf(f, "sPol_sq_cfg.Ln_cfg_dis_sq=0x%x\n", cfg->sPol_sq_cfg.Ln_cfg_dis_sq);
    fprintf(f, "sPol_sq_cfg.Ln_cfg_pd_sq=0x%x\n", cfg->sPol_sq_cfg.Ln_cfg_pd_sq);

    // tx_swing_cfg
    fprintf(f, "sTx_swing_cfg.Itx_ipdriver_base=0x%x\n", cfg->sTx_swing_cfg.Itx_ipdriver_base);

    // misc_cfg
    fprintf(f, "sMisc_cfg.Cfg_common_reserve=0x%x\n", cfg->sMisc_cfg.Cfg_common_reserve);
    fprintf(f, "sMisc_cfg.Cfg_jc_byp=0x%x\n", cfg->sMisc_cfg.Cfg_jc_byp);
    fprintf(f, "sMisc_cfg.Cfg_pll_lol_set=0x%x\n", cfg->sMisc_cfg.Cfg_pll_lol_set);
    fprintf(f, "sMisc_cfg.Cfg_en_dummy=0x%x\n", cfg->sMisc_cfg.Cfg_en_dummy);
    fprintf(f, "sMisc_cfg.Cfg_pll_reserve=0x%x\n", cfg->sMisc_cfg.Cfg_pll_reserve);
    fprintf(f, "sMisc_cfg.R_dwidthctrl_from_hwt=0x%x\n", cfg->sMisc_cfg.R_dwidthctrl_from_hwt);
    fprintf(f, "sMisc_cfg.R_reg_manual=0x%x\n", cfg->sMisc_cfg.R_reg_manual);
    fprintf(f, "sMisc_cfg.Vco_div_mode=0x%x\n", cfg->sMisc_cfg.Vco_div_mode);
    fprintf(f, "sMisc_cfg.Pre_divsel=0x%x\n", cfg->sMisc_cfg.Pre_divsel);
    fprintf(f, "sMisc_cfg.Cfg_seldiv=0x%x\n", cfg->sMisc_cfg.Cfg_seldiv);
    fprintf(f, "sMisc_cfg.Data_width_sel=0x%x\n", cfg->sMisc_cfg.Data_width_sel);
    fprintf(f, "sMisc_cfg.Txfifo_ck_div=0x%x\n", cfg->sMisc_cfg.Txfifo_ck_div);
    fprintf(f, "sMisc_cfg.Rxfifo_ck_div=0x%x\n", cfg->sMisc_cfg.Rxfifo_ck_div);
    fprintf(f, "sMisc_cfg.Pma_txck_sel=0x%x\n", cfg->sMisc_cfg.Pma_txck_sel);
    fprintf(f, "sMisc_cfg.Tx_prediv=0x%x\n", cfg->sMisc_cfg.Tx_prediv);
    fprintf(f, "sMisc_cfg.Rxdiv_sel=0x%x\n", cfg->sMisc_cfg.Rxdiv_sel);
    fprintf(f, "sMisc_cfg.Txrate_sel=0x%x\n", cfg->sMisc_cfg.Txrate_sel);
    fprintf(f, "sMisc_cfg.Rxrate_sel=0x%x\n", cfg->sMisc_cfg.Rxrate_sel);
    fprintf(f, "sMisc_cfg.Ln_cfg_cdrck_en=0x%x\n", cfg->sMisc_cfg.Ln_cfg_cdrck_en);
    fprintf(f, "sMisc_cfg.Ln_cfg_dmux_pd=0x%x\n", cfg->sMisc_cfg.Ln_cfg_dmux_pd);
    fprintf(f, "sMisc_cfg.Ln_cfg_dmux_clk_pd=0x%x\n", cfg->sMisc_cfg.Ln_cfg_dmux_clk_pd);
    fprintf(f, "sMisc_cfg.Ln_cfg_erramp_pd=0x%x\n", cfg->sMisc_cfg.Ln_cfg_erramp_pd);
    fprintf(f, "sMisc_cfg.Ln_cfg_pi_en=0x%x\n", cfg->sMisc_cfg.Ln_cfg_pi_en);
    fprintf(f, "sMisc_cfg.Ln_cfg_pd_ctle=0x%x\n", cfg->sMisc_cfg.Ln_cfg_pd_ctle);
    fprintf(f, "sMisc_cfg.Ln_cfg_summer_en=0x%x\n", cfg->sMisc_cfg.Ln_cfg_summer_en);
    fprintf(f, "sMisc_cfg.Ln_cfg_pmad_ck_pd=0x%x\n", cfg->sMisc_cfg.Ln_cfg_pmad_ck_pd);
    fprintf(f, "sMisc_cfg.Ln_cfg_pd_clk=0x%x\n", cfg->sMisc_cfg.Ln_cfg_pd_clk);
    fprintf(f, "sMisc_cfg.Ln_cfg_pd_cml=0x%x\n", cfg->sMisc_cfg.Ln_cfg_pd_cml);
    fprintf(f, "sMisc_cfg.Ln_cfg_pd_driver=0x%x\n", cfg->sMisc_cfg.Ln_cfg_pd_driver);
    fprintf(f, "sMisc_cfg.Ln_cfg_rx_reg_pu=0x%x\n", cfg->sMisc_cfg.Ln_cfg_rx_reg_pu);
    fprintf(f, "sMisc_cfg.Ln_cfg_pd_rms_det=0x%x\n", cfg->sMisc_cfg.Ln_cfg_pd_rms_det);
    fprintf(f, "sMisc_cfg.Ln_cfg_dcdr_pd=0x%x\n", cfg->sMisc_cfg.Ln_cfg_dcdr_pd);
    fprintf(f, "sMisc_cfg.Ln_cfg_ecdr_pd=0x%x\n", cfg->sMisc_cfg.Ln_cfg_ecdr_pd);
    fprintf(f, "sMisc_cfg.L0_cfg_bw=0x%x\n", cfg->sMisc_cfg.L0_cfg_bw);
    fprintf(f, "sMisc_cfg.L0_cfg_txcal_en=0x%x\n", cfg->sMisc_cfg.L0_cfg_txcal_en);
    fprintf(f, "sMisc_cfg.Ln_cfg_bw=0x%x\n", cfg->sMisc_cfg.Ln_cfg_bw);
    fprintf(f, "sMisc_cfg.Ln_cfg_txcal_man_en=0x%x\n", cfg->sMisc_cfg.Ln_cfg_txcal_man_en);
    fprintf(f, "sMisc_cfg.Ln_cfg_phase_man=0x%x\n", cfg->sMisc_cfg.Ln_cfg_phase_man);
    fprintf(f, "sMisc_cfg.Ln_cfg_quad_man=0x%x\n", cfg->sMisc_cfg.Ln_cfg_quad_man);
    fprintf(f, "sMisc_cfg.Ln_cfg_txcal_shift_code=0x%x\n", cfg->sMisc_cfg.Ln_cfg_txcal_shift_code);
    fprintf(f, "sMisc_cfg.Ln_cfg_txcal_valid_sel=0x%x\n", cfg->sMisc_cfg.Ln_cfg_txcal_valid_sel);
    fprintf(f, "sMisc_cfg.Ln_cfg_cdr_kf=0x%x\n", cfg->sMisc_cfg.Ln_cfg_cdr_kf);
    fprintf(f, "sMisc_cfg.Ln_cfg_pi_bw=0x%x\n", cfg->sMisc_cfg.Ln_cfg_pi_bw);
    fprintf(f, "sMisc_cfg.Ln_cfg_pi_steps=0x%x\n", cfg->sMisc_cfg.Ln_cfg_pi_steps);
    fprintf(f, "sMisc_cfg.Ln_cfg_dis_2ndorder=0x%x\n", cfg->sMisc_cfg.Ln_cfg_dis_2ndorder);
    fprintf(f, "sMisc_cfg.Ln_cfg_rx_reserve_7_0=0x%x\n", cfg->sMisc_cfg.Ln_cfg_rx_reserve_7_0);
    fprintf(f, "sMisc_cfg.Ln_cfg_rx_reserve_15_8=0x%x\n", cfg->sMisc_cfg.Ln_cfg_rx_reserve_15_8);
    fprintf(f, "sMisc_cfg.Ln_cfg_rx_term=0x%x\n", cfg->sMisc_cfg.Ln_cfg_rx_term);
    fprintf(f, "sMisc_cfg.Ln_cfg_rx_sp_ctle=0x%x\n", cfg->sMisc_cfg.Ln_cfg_rx_sp_ctle);
    fprintf(f, "sMisc_cfg.Ln_cfg_isel_ctle=0x%x\n", cfg->sMisc_cfg.Ln_cfg_isel_ctle);
    fprintf(f, "sMisc_cfg.Ln_cfg_eqr_byp=0x%x\n", cfg->sMisc_cfg.Ln_cfg_eqr_byp);
    fprintf(f, "sMisc_cfg.Ln_cfg_agc_adpt_byp=0x%x\n", cfg->sMisc_cfg.Ln_cfg_agc_adpt_byp);
    fprintf(f, "sMisc_cfg.Ln_cfg_sum_setcm_en=0x%x\n", cfg->sMisc_cfg.Ln_cfg_sum_setcm_en);
    fprintf(f, "sMisc_cfg.Ln_cfg_init_pos_iscan=0x%x\n", cfg->sMisc_cfg.Ln_cfg_init_pos_iscan);
    fprintf(f, "sMisc_cfg.Ln_cfg_init_pos_iPI=0x%x\n", cfg->sMisc_cfg.Ln_cfg_init_pos_iPI);
    fprintf(f, "sMisc_cfg.Cfg_i_vco=0x%x\n", cfg->sMisc_cfg.Cfg_i_vco);
    fprintf(f, "sMisc_cfg.Icp_base_sel=0x%x\n", cfg->sMisc_cfg.Icp_base_sel);
    fprintf(f, "sMisc_cfg.Icp_sel=0x%x\n", cfg->sMisc_cfg.Icp_sel);
    fprintf(f, "sMisc_cfg.Cfg_rsel=0x%x\n", cfg->sMisc_cfg.Cfg_rsel);
    fprintf(f, "sMisc_cfg.Ln_cfg_iscan_en=0x%x\n", cfg->sMisc_cfg.Ln_cfg_iscan_en);
    fprintf(f, "sMisc_cfg.Ln_cfg_en_fast_iscan=0x%x\n", cfg->sMisc_cfg.Ln_cfg_en_fast_iscan);
    fprintf(f, "sMisc_cfg.Ln_cfg_filter2nd_yz_6_0=0x%x\n", cfg->sMisc_cfg.Ln_cfg_filter2nd_yz_6_0);
    fprintf(f, "sMisc_cfg.Ln_r_alos_en=0x%x\n", cfg->sMisc_cfg.Ln_r_alos_en);

    fclose(f);
    return 0;
}

// Read config from file in Tag=Value format
static int read_serdes_config(const char *filename, __SERDES_CONFIG_T *cfg)
{
    FILE *f = fopen(filename, "r");
    if (!f) {
        return -1;
    }

    char line[128];
    char tag[64];
    unsigned int value;
    while (fgets(line, sizeof(line), f)) {
        // Accept both 0x and decimal, but expect 0x
        if (sscanf(line, "%63[^=]=0x%x", tag, &value) != 2) {
            continue;
        }

        // tx_eq_cfg
        if (strcmp(tag, "sTx_eq_cfg.Tap_dly") == 0) {
            cfg->sTx_eq_cfg.Tap_dly = (u8)value;
        } else if (strcmp(tag, "sTx_eq_cfg.Tap_main") == 0) {
            cfg->sTx_eq_cfg.Tap_main = (u8)value;
        } else if (strcmp(tag, "sTx_eq_cfg.Tap_adv") == 0) {
            cfg->sTx_eq_cfg.Tap_adv = (u8)value;
        } else if (strcmp(tag, "sTx_eq_cfg.En_main") == 0) {
            cfg->sTx_eq_cfg.En_main = (u8)value;
        } else if (strcmp(tag, "sTx_eq_cfg.En_adv") == 0) {
            cfg->sTx_eq_cfg.En_adv = (u8)value;
        } else if (strcmp(tag, "sTx_eq_cfg.En_dly") == 0) {
            cfg->sTx_eq_cfg.En_dly = (u8)value;
        }

        // cdr_cfg
        else if (strcmp(tag, "sCdr_cfg.Cdr_m") == 0) {
            cfg->sCdr_cfg.Cdr_m = (u8)value;
        } else if (strcmp(tag, "sCdr_cfg.Alos_thr") == 0) {
            cfg->sCdr_cfg.Alos_thr = (u8)value;
        }

        // speed_change_cfg
        else if (strcmp(tag, "sSpeed_change_cfg.L0_cfg_tx_reserve_15_8") == 0) {
            cfg->sSpeed_change_cfg.L0_cfg_tx_reserve_15_8 = (u8)value;
        } else if (strcmp(tag, "sSpeed_change_cfg.L0_cfg_tx_reserve_7_0") == 0) {
            cfg->sSpeed_change_cfg.L0_cfg_tx_reserve_7_0 = (u8)value;
        } else if (strcmp(tag, "sSpeed_change_cfg.Ln_cfg_tx_reserve_15_8") == 0) {
            cfg->sSpeed_change_cfg.Ln_cfg_tx_reserve_15_8 = (u8)value;
        } else if (strcmp(tag, "sSpeed_change_cfg.Ln_cfg_tx_reserve_7_0") == 0) {
            cfg->sSpeed_change_cfg.Ln_cfg_tx_reserve_7_0 = (u8)value;
        }

        // rx_eq_cfg
        else if (strcmp(tag, "sRx_eq_cfg.Ln_cfg_vga_ctrl_byp") == 0) {
            cfg->sRx_eq_cfg.Ln_cfg_vga_ctrl_byp = (u8)value;
        } else if (strcmp(tag, "sRx_eq_cfg.Ln_cfg_vga_byp") == 0) {
            cfg->sRx_eq_cfg.Ln_cfg_vga_byp = (u8)value;
        } else if (strcmp(tag, "sRx_eq_cfg.Ln_cfg_eqr_force") == 0) {
            cfg->sRx_eq_cfg.Ln_cfg_eqr_force = (u8)value;
        } else if (strcmp(tag, "sRx_eq_cfg.Ln_cfg_eqc_force") == 0) {
            cfg->sRx_eq_cfg.Ln_cfg_eqc_force = (u8)value;
        }

        // dfe_cfg
        else if (strcmp(tag, "sDfe_cfg.Ln_cfg_dfeck_en") == 0) {
            cfg->sDfe_cfg.Ln_cfg_dfeck_en = (u8)value;
        } else if (strcmp(tag, "sDfe_cfg.Ln_cfg_dfe_pd") == 0) {
            cfg->sDfe_cfg.Ln_cfg_dfe_pd = (u8)value;
        } else if (strcmp(tag, "sDfe_cfg.Ln_cfg_dfemx_pd") == 0) {
            cfg->sDfe_cfg.Ln_cfg_dfemx_pd = (u8)value;
        } else if (strcmp(tag, "sDfe_cfg.Ln_cfg_dfe_dmux_pd") == 0) {
            cfg->sDfe_cfg.Ln_cfg_dfe_dmux_pd = (u8)value;
        } else if (strcmp(tag, "sDfe_cfg.Ln_cfg_pi_dfe_en") == 0) {
            cfg->sDfe_cfg.Ln_cfg_pi_dfe_en = (u8)value;
        } else if (strcmp(tag, "sDfe_cfg.Ln_cfg_dfedig_m") == 0) {
            cfg->sDfe_cfg.Ln_cfg_dfedig_m = (u8)value;
        } else if (strcmp(tag, "sDfe_cfg.Ln_cfg_dfetap_en") == 0) {
            cfg->sDfe_cfg.Ln_cfg_dfetap_en = (u8)value;
        } else if (strcmp(tag, "sDfe_cfg.Ln_cfg_en_dfedig") == 0) {
            cfg->sDfe_cfg.Ln_cfg_en_dfedig = (u8)value;
        }

        // pol_sq_cfg
        else if (strcmp(tag, "sPol_sq_cfg.Tx_pol_inv") == 0) {
            cfg->sPol_sq_cfg.Tx_pol_inv = (u8)value;
        } else if (strcmp(tag, "sPol_sq_cfg.Rx_pol_inv") == 0) {
            cfg->sPol_sq_cfg.Rx_pol_inv = (u8)value;
        } else if (strcmp(tag, "sPol_sq_cfg.Ln_cfg_dis_sq") == 0) {
            cfg->sPol_sq_cfg.Ln_cfg_dis_sq = (u8)value;
        } else if (strcmp(tag, "sPol_sq_cfg.Ln_cfg_pd_sq") == 0) {
            cfg->sPol_sq_cfg.Ln_cfg_pd_sq = (u8)value;
        }

        // tx_swing_cfg
        else if (strcmp(tag, "sTx_swing_cfg.Itx_ipdriver_base") == 0) {
            cfg->sTx_swing_cfg.Itx_ipdriver_base = (u8)value;
        }

        // misc_cfg
        else if (strcmp(tag, "sMisc_cfg.Cfg_common_reserve") == 0) {
            cfg->sMisc_cfg.Cfg_common_reserve = (u8)value;
        } else if (strcmp(tag, "sMisc_cfg.Cfg_jc_byp") == 0) {
            cfg->sMisc_cfg.Cfg_jc_byp = (u8)value;
        } else if (strcmp(tag, "sMisc_cfg.Cfg_pll_lol_set") == 0) {
            cfg->sMisc_cfg.Cfg_pll_lol_set = (u8)value;
        } else if (strcmp(tag, "sMisc_cfg.Cfg_en_dummy") == 0) {
            cfg->sMisc_cfg.Cfg_en_dummy = (u8)value;
        } else if (strcmp(tag, "sMisc_cfg.Cfg_pll_reserve") == 0) {
            cfg->sMisc_cfg.Cfg_pll_reserve = (u8)value;
        } else if (strcmp(tag, "sMisc_cfg.R_dwidthctrl_from_hwt") == 0) {
            cfg->sMisc_cfg.R_dwidthctrl_from_hwt = (u8)value;
        } else if (strcmp(tag, "sMisc_cfg.R_reg_manual") == 0) {
            cfg->sMisc_cfg.R_reg_manual = (u8)value;
        } else if (strcmp(tag, "sMisc_cfg.Vco_div_mode") == 0) {
            cfg->sMisc_cfg.Vco_div_mode = (u8)value;
        } else if (strcmp(tag, "sMisc_cfg.Pre_divsel") == 0) {
            cfg->sMisc_cfg.Pre_divsel = (u8)value;
        } else if (strcmp(tag, "sMisc_cfg.Cfg_seldiv") == 0) {
            cfg->sMisc_cfg.Cfg_seldiv = (u8)value;
        } else if (strcmp(tag, "sMisc_cfg.Data_width_sel") == 0) {
            cfg->sMisc_cfg.Data_width_sel = (u8)value;
        } else if (strcmp(tag, "sMisc_cfg.Txfifo_ck_div") == 0) {
            cfg->sMisc_cfg.Txfifo_ck_div = (u8)value;
        } else if (strcmp(tag, "sMisc_cfg.Rxfifo_ck_div") == 0) {
            cfg->sMisc_cfg.Rxfifo_ck_div = (u8)value;
        } else if (strcmp(tag, "sMisc_cfg.Pma_txck_sel") == 0) {
            cfg->sMisc_cfg.Pma_txck_sel = (u8)value;
        } else if (strcmp(tag, "sMisc_cfg.Tx_prediv") == 0) {
            cfg->sMisc_cfg.Tx_prediv = (u8)value;
        } else if (strcmp(tag, "sMisc_cfg.Rxdiv_sel") == 0) {
            cfg->sMisc_cfg.Rxdiv_sel = (u8)value;
        } else if (strcmp(tag, "sMisc_cfg.Txrate_sel") == 0) {
            cfg->sMisc_cfg.Txrate_sel = (u8)value;
        } else if (strcmp(tag, "sMisc_cfg.Rxrate_sel") == 0) {
            cfg->sMisc_cfg.Rxrate_sel = (u8)value;
        } else if (strcmp(tag, "sMisc_cfg.Ln_cfg_cdrck_en") == 0) {
            cfg->sMisc_cfg.Ln_cfg_cdrck_en = (u8)value;
        } else if (strcmp(tag, "sMisc_cfg.Ln_cfg_dmux_pd") == 0) {
            cfg->sMisc_cfg.Ln_cfg_dmux_pd = (u8)value;
        } else if (strcmp(tag, "sMisc_cfg.Ln_cfg_dmux_clk_pd") == 0) {
            cfg->sMisc_cfg.Ln_cfg_dmux_clk_pd = (u8)value;
        } else if (strcmp(tag, "sMisc_cfg.Ln_cfg_erramp_pd") == 0) {
            cfg->sMisc_cfg.Ln_cfg_erramp_pd = (u8)value;
        } else if (strcmp(tag, "sMisc_cfg.Ln_cfg_pi_en") == 0) {
            cfg->sMisc_cfg.Ln_cfg_pi_en = (u8)value;
        } else if (strcmp(tag, "sMisc_cfg.Ln_cfg_pd_ctle") == 0) {
            cfg->sMisc_cfg.Ln_cfg_pd_ctle = (u8)value;
        } else if (strcmp(tag, "sMisc_cfg.Ln_cfg_summer_en") == 0) {
            cfg->sMisc_cfg.Ln_cfg_summer_en = (u8)value;
        } else if (strcmp(tag, "sMisc_cfg.Ln_cfg_pmad_ck_pd") == 0) {
            cfg->sMisc_cfg.Ln_cfg_pmad_ck_pd = (u8)value;
        } else if (strcmp(tag, "sMisc_cfg.Ln_cfg_pd_clk") == 0) {
            cfg->sMisc_cfg.Ln_cfg_pd_clk = (u8)value;
        } else if (strcmp(tag, "sMisc_cfg.Ln_cfg_pd_cml") == 0) {
            cfg->sMisc_cfg.Ln_cfg_pd_cml = (u8)value;
        } else if (strcmp(tag, "sMisc_cfg.Ln_cfg_pd_driver") == 0) {
            cfg->sMisc_cfg.Ln_cfg_pd_driver = (u8)value;
        } else if (strcmp(tag, "sMisc_cfg.Ln_cfg_rx_reg_pu") == 0) {
            cfg->sMisc_cfg.Ln_cfg_rx_reg_pu = (u8)value;
        } else if (strcmp(tag, "sMisc_cfg.Ln_cfg_pd_rms_det") == 0) {
            cfg->sMisc_cfg.Ln_cfg_pd_rms_det = (u8)value;
        } else if (strcmp(tag, "sMisc_cfg.Ln_cfg_dcdr_pd") == 0) {
            cfg->sMisc_cfg.Ln_cfg_dcdr_pd = (u8)value;
        } else if (strcmp(tag, "sMisc_cfg.Ln_cfg_ecdr_pd") == 0) {
            cfg->sMisc_cfg.Ln_cfg_ecdr_pd = (u8)value;
        } else if (strcmp(tag, "sMisc_cfg.L0_cfg_bw") == 0) {
            cfg->sMisc_cfg.L0_cfg_bw = (u8)value;
        } else if (strcmp(tag, "sMisc_cfg.L0_cfg_txcal_en") == 0) {
            cfg->sMisc_cfg.L0_cfg_txcal_en = (u8)value;
        } else if (strcmp(tag, "sMisc_cfg.Ln_cfg_bw") == 0) {
            cfg->sMisc_cfg.Ln_cfg_bw = (u8)value;
        } else if (strcmp(tag, "sMisc_cfg.Ln_cfg_txcal_man_en") == 0) {
            cfg->sMisc_cfg.Ln_cfg_txcal_man_en = (u8)value;
        } else if (strcmp(tag, "sMisc_cfg.Ln_cfg_phase_man") == 0) {
            cfg->sMisc_cfg.Ln_cfg_phase_man = (u8)value;
        } else if (strcmp(tag, "sMisc_cfg.Ln_cfg_quad_man") == 0) {
            cfg->sMisc_cfg.Ln_cfg_quad_man = (u8)value;
        } else if (strcmp(tag, "sMisc_cfg.Ln_cfg_txcal_shift_code") == 0) {
            cfg->sMisc_cfg.Ln_cfg_txcal_shift_code = (u8)value;
        } else if (strcmp(tag, "sMisc_cfg.Ln_cfg_txcal_valid_sel") == 0) {
            cfg->sMisc_cfg.Ln_cfg_txcal_valid_sel = (u8)value;
        } else if (strcmp(tag, "sMisc_cfg.Ln_cfg_cdr_kf") == 0) {
            cfg->sMisc_cfg.Ln_cfg_cdr_kf = (u8)value;
        } else if (strcmp(tag, "sMisc_cfg.Ln_cfg_pi_bw") == 0) {
            cfg->sMisc_cfg.Ln_cfg_pi_bw = (u8)value;
        } else if (strcmp(tag, "sMisc_cfg.Ln_cfg_pi_steps") == 0) {
            cfg->sMisc_cfg.Ln_cfg_pi_steps = (u8)value;
        } else if (strcmp(tag, "sMisc_cfg.Ln_cfg_dis_2ndorder") == 0) {
            cfg->sMisc_cfg.Ln_cfg_dis_2ndorder = (u8)value;
        } else if (strcmp(tag, "sMisc_cfg.Ln_cfg_rx_reserve_7_0") == 0) {
            cfg->sMisc_cfg.Ln_cfg_rx_reserve_7_0 = (u8)value;
        } else if (strcmp(tag, "sMisc_cfg.Ln_cfg_rx_reserve_15_8") == 0) {
            cfg->sMisc_cfg.Ln_cfg_rx_reserve_15_8 = (u8)value;
        } else if (strcmp(tag, "sMisc_cfg.Ln_cfg_rx_term") == 0) {
            cfg->sMisc_cfg.Ln_cfg_rx_term = (u8)value;
        } else if (strcmp(tag, "sMisc_cfg.Ln_cfg_rx_sp_ctle") == 0) {
            cfg->sMisc_cfg.Ln_cfg_rx_sp_ctle = (u8)value;
        } else if (strcmp(tag, "sMisc_cfg.Ln_cfg_isel_ctle") == 0) {
            cfg->sMisc_cfg.Ln_cfg_isel_ctle = (u8)value;
        } else if (strcmp(tag, "sMisc_cfg.Ln_cfg_eqr_byp") == 0) {
            cfg->sMisc_cfg.Ln_cfg_eqr_byp = (u8)value;
        } else if (strcmp(tag, "sMisc_cfg.Ln_cfg_agc_adpt_byp") == 0) {
            cfg->sMisc_cfg.Ln_cfg_agc_adpt_byp = (u8)value;
        } else if (strcmp(tag, "sMisc_cfg.Ln_cfg_sum_setcm_en") == 0) {
            cfg->sMisc_cfg.Ln_cfg_sum_setcm_en = (u8)value;
        } else if (strcmp(tag, "sMisc_cfg.Ln_cfg_init_pos_iscan") == 0) {
            cfg->sMisc_cfg.Ln_cfg_init_pos_iscan = (u8)value;
        } else if (strcmp(tag, "sMisc_cfg.Ln_cfg_init_pos_iPI") == 0) {
            cfg->sMisc_cfg.Ln_cfg_init_pos_iPI = (u8)value;
        } else if (strcmp(tag, "sMisc_cfg.Cfg_i_vco") == 0) {
            cfg->sMisc_cfg.Cfg_i_vco = (u8)value;
        } else if (strcmp(tag, "sMisc_cfg.Icp_base_sel") == 0) {
            cfg->sMisc_cfg.Icp_base_sel = (u8)value;
        } else if (strcmp(tag, "sMisc_cfg.Icp_sel") == 0) {
            cfg->sMisc_cfg.Icp_sel = (u8)value;
        } else if (strcmp(tag, "sMisc_cfg.Cfg_rsel") == 0) {
            cfg->sMisc_cfg.Cfg_rsel = (u8)value;
        } else if (strcmp(tag, "sMisc_cfg.Ln_cfg_iscan_en") == 0) {
            cfg->sMisc_cfg.Ln_cfg_iscan_en = (u8)value;
        } else if (strcmp(tag, "sMisc_cfg.Ln_cfg_en_fast_iscan") == 0) {
            cfg->sMisc_cfg.Ln_cfg_en_fast_iscan = (u8)value;
        } else if (strcmp(tag, "sMisc_cfg.Ln_cfg_filter2nd_yz_6_0") == 0) {
            cfg->sMisc_cfg.Ln_cfg_filter2nd_yz_6_0 = (u8)value;
        } else if (strcmp(tag, "sMisc_cfg.Ln_r_alos_en") == 0) {
            cfg->sMisc_cfg.Ln_r_alos_en = (u8)value;
        }
    }
    fclose(f);
    dump_sd_cfg(*cfg, eALL_CFG);
    return 0;
}

static void cli_cmd_phy_serdes_get(cli_req_t *req)
{
    mepa_rc rc;
    SD_CFG_SPEED_IDX_t speed;
    __SERDES_CONFIG_T sd_cfg = {0};
    phy_sd_cli_req_t *mreq = req->module_req;

    for (speed = 0; speed < SD_UNKNOWN_SPEED; speed++) {
        if (speed != mreq->speed_idx) {
            continue;
        }
        printf ("SERDES CONFIG GET - %d\n", speed);
        rc = lan80xx_get_serdes_config(meba_phy_kr_inst->phy_devices[0], speed, eALL_CFG, &sd_cfg);
        if (rc != MESA_RC_OK) {
            T_E(MEPA_TRACE_GRP_GEN, "serdes config get failed\n");
            break;
        }
        printf ("#####################\n");
        dump_sd_cfg(sd_cfg, eALL_CFG);
        write_serdes_config("/tmp/serdes_rd.cfg", &sd_cfg);
    }
}

static void cli_cmd_phy_serdes_set(cli_req_t *req)
{
    mepa_rc rc;
    SD_CFG_SPEED_IDX_t speed;
    __SERDES_CONFIG_T cfg = {0};
    phy_sd_cli_req_t *mreq = req->module_req;

    if (read_serdes_config("/tmp/serdes_wr.cfg", &cfg) != 0) {
        printf ("File (%s) does not exist!! Create a file with presets to load\n", "/tmp/serdes_wr.cfg");
        return;
    }

    for (speed = 0; speed < SD_UNKNOWN_SPEED; speed++) {
        if (speed != mreq->speed_idx) {
            continue;
        }
        rc = lan80xx_set_serdes_config(meba_phy_kr_inst->phy_devices[0], speed, eALL_CFG, &cfg);
        if (rc != MESA_RC_OK) {
            T_E(MEPA_TRACE_GRP_GEN, "serdes config set failed\n");
            break;
        }
    }
}

static void cli_cmd_phy_kr(cli_req_t *req)
{
    mepa_rc rc;
    demo_phy_info_t phy_family = {0};
    phy_kr_cli_req_t *mreq = req->module_req;
    mepa_port_no_t  port_no;
    mepa_conf_t conf = {0};
    const uint8_t u8linkup_time = 3;
    uint8_t LinkSpeed = 0;

    /* loop till phy_device_cnt - 1, as last port is management port */
    for (int iport = 0; iport < meba_phy_kr_inst->phy_device_cnt - 1; iport++) {
        port_no = iport2uport(iport);
        if (req->port_list[port_no] == 0) {
            continue;
        }
        if (meba_phy_kr_inst->phy_devices[iport] == NULL) {
            continue;
        }
        if ((rc = phy_family_detect(meba_phy_kr_inst, iport, &phy_family)) != MEPA_RC_OK) {
            cli_printf ("\nError in Detecting PHY Family on Port %d\n", iport);
            continue;
        }
        if (req->set) {
            if (phy_family.family == PHY_FAMILY_MALIBU_25G) {
                /* Get old config to retain mepa related context */
                if ((rc = mepa_conf_get(meba_phy_kr_inst->phy_devices[iport], &conf)) != MESA_RC_OK) {
                    T_E("\n mepa_conf_get failed on port %d\n", iport);
                    continue;
                }
                if (mreq->dis == TRUE) {
                    conf.speed = MESA_SPEED_AUTO;
                    /* with out aneg, no training */
                    conf.conf_25g.kr_train_enable = 0;
                    conf.adv_dis = TRUE;
                } else {
                    conf.adv_dis = FALSE;
                    conf.speed = MESA_SPEED_AUTO;
                    conf.conf_25g.kr_train_enable = mreq->train;
                    conf.aneg.speed_10g_fdx = mreq->adv10g;
                    conf.aneg.speed_1g_fdx = mreq->adv1g;
                    conf.aneg.speed_25g_fdx = mreq->adv25g_kr;
                    conf.aneg.speed_25g_kr_s_fdx = mreq->adv25g_krs;
                    conf.aneg.next_page_enable = mreq->np;
                    conf.aneg.advertise_dir = MEPA_ADV_SIDE_LINE;
                    conf.conf_25g.base_r_10gfec = mreq->rfec_10g;
                    conf.conf_25g.base_r_25gfec = mreq->rfec_25g;
                    conf.conf_25g.rs_fec_25g = mreq->rsfec_25g;
                    conf.conf_25g.np_base_r_fec = mreq->np_rfec;
                    conf.conf_25g.np_rs_fec = mreq->np_rsfec;
                    conf.conf_25g.fw_resolve = mreq->fw_res;
                }
                /* set new config */
                if ((rc = mepa_conf_set(meba_phy_kr_inst->phy_devices[iport], &conf)) != MESA_RC_OK) {
                    T_E("\n mepa_conf_set failed on port %d\n", iport);
                    continue;
                }
            }
        } else {
            if ((rc = mepa_conf_get(meba_phy_kr_inst->phy_devices[iport], &conf)) != MESA_RC_OK) {
                T_E("\n mepa_conf_get failed on port %d\n", iport);
                continue;
            }
            cli_printf("Port: %d\n", iport);
            cli_printf("  KR aneg: %s\n", (conf.speed == MESA_SPEED_AUTO) ? "Enabled" : "Disabled");
            cli_printf("  KR training: %s\n", conf.conf_25g.kr_train_enable ? "Enabled" : "Disabled");
        }
    }
    /*
     * The below logic is specific to M25G
     * line side ports configured with the requested cofnig
     * then wait for 3sec (shall be modified through command)
     * check the linkup status on LINE SIDE ports
     * Link up - get (LINE linkup speed) and configure HOST side
     * Link fail - disable aneg
     */
    if (req->set && (phy_family.family == PHY_FAMILY_MALIBU_25G)) {
        /* Wait for the LINE link up */
        sleep(u8linkup_time);
        for (int iport = 0; iport < meba_phy_kr_inst->phy_device_cnt - 1; iport++) {
            port_no = iport2uport(iport);
            if (req->port_list[port_no] == 0) {
                continue;
            }
            LinkSpeed = MESA_SPEED_UNDEFINED;
            /* check the linkup state for line side */
            rc = Islinkup(iport, MEPA_ADV_SIDE_LINE, &LinkSpeed);
            /* Get the link up speed */
            if (rc == MESA_RC_OK && mreq->dis == FALSE) {
                if (LinkSpeed == 7) {
                    cli_printf("Line side link up speed: 25G kr\n");
                }
                if (LinkSpeed == 8) {
                    cli_printf("Line side link up speed: 25G kr-s\n");
                }
                if (LinkSpeed == 9) {
                    cli_printf("Line side link up speed: 10G\n");
                }
                if (LinkSpeed == 13) {
                    cli_printf("Line side link up speed: 1G\n");
                }
                if ((rc = mepa_conf_get(meba_phy_kr_inst->phy_devices[iport], &conf)) != MESA_RC_OK) {
                    T_E("\n mepa_conf_get failed on port %d\n", iport);
                    continue;
                }
                /* Send the command to M25g for host side */
                conf.adv_dis = FALSE;
                conf.speed = MESA_SPEED_AUTO;
                conf.aneg.speed_25g_fdx = (LinkSpeed == 7) ? 1 : 0;
                conf.aneg.speed_25g_kr_s_fdx = (LinkSpeed == 8) ? 1 : 0;
                conf.aneg.speed_10g_fdx = (LinkSpeed == 9) ? 1 : 0;
                conf.aneg.speed_1g_fdx = (LinkSpeed == 13) ? 1 : 0;
                conf.aneg.advertise_dir = MEPA_ADV_SIDE_HOST;
            } else {
                /* Configure fixed 25g speed */
                conf.speed = MESA_SPEED_AUTO;
                conf.conf_25g.kr_train_enable = 0;
                conf.adv_dis = TRUE;
            }

            if ((rc = mepa_conf_set(meba_phy_kr_inst->phy_devices[iport], &conf)) != MESA_RC_OK) {
                T_E("\n mepa_conf_set failed on port %d\n", iport);
                continue;
            }
            /*
             * Return from here!!
             * don't wait for Host side link up
             * As the HOST is connected to EDSX serdes
             * EDSX serdes must run ANEG
             * So, return from here, so EDSx AN SM starts
             */
        }
    }
    return;
}
static void phy_kr_log_enable_disable (cli_req_t *req)
{
    phy_kr_log_sel_t *mreq = req->module_req;
    mepa_rc rc = MEPA_RC_OK;
    mepa_port_no_t  port_no = 0;

    for (int iport = 0; iport < meba_phy_kr_inst->phy_device_cnt - 1; iport++) {
        port_no = iport2uport(iport);
        if (req->port_list[port_no] == 0) {
            continue;
        }
        if (!meba_phy_kr_inst->phy_devices[iport]) {
            cli_printf(" Dev is Not Created for the port : %d\n", iport);
            return;
        }

        if ((rc = lan80xx_KRLog_Enable(meba_phy_kr_inst->phy_devices[iport], mreq->krlog_enable, mreq->line_port, mreq->host_port)) != MEPA_RC_OK) {
            if (mreq->line_port) {
                if (mreq->krlog_enable) {
                    T_E("KR Log Enable Failed for Line side Port %d\n", iport);
                } else {
                    T_E("KR Log Disable Failed for Line side Port %d\n", iport);
                }
            }
            if (mreq->host_port) {
                if (mreq->krlog_enable) {
                    T_E("KR Log Enable Failed for host side Port %d\n", iport);
                } else {
                    T_E("KR Log Disable for host side Port %d\n", iport);
                }
            }
            return;
        } else {
            if (mreq->line_port) {
                if (mreq->krlog_enable) {
                    cli_printf("\n KR Log Enable Success for Line side Port %d\n", iport);
                } else {
                    cli_printf("\n KR Log Disable Success for Line side Port %d\n", iport);
                }
            }
            if (mreq->host_port) {
                if (mreq->krlog_enable) {
                    cli_printf("\n KR Log Enable Success for host side Port %d\n", iport);
                } else {
                    cli_printf("\n KR Log Disable Success for host side Port %d\n", iport);
                }

            }
        }
    }
    return;

}
static void cli_cmd_phy_kr_log_disable(cli_req_t *req)
{
    phy_kr_log_sel_t *mreq = req->module_req;
    mreq->krlog_enable = 0;
    phy_kr_log_enable_disable(req);
}
static void cli_cmd_phy_kr_log_enable(cli_req_t *req)
{
    phy_kr_log_sel_t *mreq = req->module_req;
    mreq->krlog_enable = 1;
    phy_kr_log_enable_disable(req);

}
char *converttoenum(const char *input[], int val)
{
    char *output = (char *)malloc(256);
    if (output == NULL) {
        // Handle memory allocation failure
        return NULL;
    }

    // Copy the selected string into the output buffer
    strncpy(output, input[val], 256 - 1);
    output[256 - 1] = '\0'; // Ensure null termination

    return output;
}

void appendIRQString(char *strValue, uint32_t dummyValue, uint32_t irqMask, int enumIndex)
{
    if (dummyValue & irqMask) {
        char *sdummyString = converttoenum(enum_irq_string, enumIndex);
        if (sdummyString != NULL) {
            if (strValue[0] == '\0') {
                // strValue is empty, just copy
                strcat(strValue, sdummyString);
            } else {
                // strValue is not empty, append separator and string
                strcat(strValue, "/");
                strcat(strValue, sdummyString);
            }
            free(sdummyString); // Free the allocated memory
        }
    }
}

char *getIRQ(uint32_t dwIRQ)
{
    char *strValue = (char *)malloc(256);
    if (strValue == NULL) {
        // Handle memory allocation failure
        return NULL;
    }
    strValue[0] = '\0'; // Initialize the string

    uint32_t dummyValue = (dwIRQ & 0xFFFFFFFF);
    appendIRQString(strValue, dummyValue, IRQ_VEC0_AN_RATE, 0);
    appendIRQString(strValue, dummyValue, IRQ_VEC0_GEN1_DONE, 1);
    appendIRQString(strValue, dummyValue, IRQ_VEC0_GEN0_DONE, 2);
    appendIRQString(strValue, dummyValue, IRQ_VEC0_INCP_LINK, 3);
    appendIRQString(strValue, dummyValue, IRQ_VEC0_NP_RX, 4);
    appendIRQString(strValue, dummyValue, IRQ_VEC0_NP_FAIL, 5);
    appendIRQString(strValue, dummyValue, IRQ_VEC0_ACK_FAIL, 6);
    appendIRQString(strValue, dummyValue, IRQ_VEC0_ABD_FAIL, 7);
    appendIRQString(strValue, dummyValue, IRQ_VEC0_LINK_FAIL, 8);
    appendIRQString(strValue, dummyValue, IRQ_VEC0_AN_GOOD, 9);
    appendIRQString(strValue, dummyValue, IRQ_VEC0_CMPL_ACK, 10);
    appendIRQString(strValue, dummyValue, IRQ_VEC0_AN_RATE_DET, 11);
    appendIRQString(strValue, dummyValue, IRQ_VEC0_AN_TRAIN, 12);
    appendIRQString(strValue, dummyValue, IRQ_VEC1_AN_XMIT_DISABLE, 13);
    appendIRQString(strValue, dummyValue, IRQ_VEC1_DME_VIOL_1, 14);
    appendIRQString(strValue, dummyValue, IRQ_VEC1_DME_VIOL_0, 15);
    appendIRQString(strValue, dummyValue, IRQ_VEC1_FRLOCK_1, 16);
    appendIRQString(strValue, dummyValue, IRQ_VEC1_FRLOCK_0, 17);
    appendIRQString(strValue, dummyValue, IRQ_VEC1_REM_RDY_1, 18);
    appendIRQString(strValue, dummyValue, IRQ_VEC1_REM_RDY_0, 19);
    appendIRQString(strValue, dummyValue, IRQ_VEC1_BER_BUSY_1, 20);
    appendIRQString(strValue, dummyValue, IRQ_VEC1_BER_BUSY_0, 21);
    appendIRQString(strValue, dummyValue, IRQ_VEC1_MW_DONE, 22);
    appendIRQString(strValue, dummyValue, IRQ_VEC1_WT_DONE, 23);
    appendIRQString(strValue, dummyValue, IRQ_VEC1_LPCVALID, 24);
    appendIRQString(strValue, dummyValue, IRQ_VEC1_LPSVALID, 25);
    appendIRQString(strValue, dummyValue, IRQ_VEC1_KR_ACTV, 26);
    appendIRQString(strValue, dummyValue, IRQ_VEC1_ACK_FIN, 27);
    appendIRQString(strValue, dummyValue, IRQ_VEC1_NP_REQ, 28);

    return strValue;
}


char *DecodeBasePage0(uint16_t u16Lp_Bp)
{
    static char str[100]; // Static buffer to hold the decoded string
    char sdummyString[20]; // Temporary buffer for each string component

    str[0] = '\0'; // Initialize the output string

    int matched = 0; // Flag to check if any condition matched
    if (u16Lp_Bp & AN_BP0_Selector) {
        //strcpy(sdummyString, "Selector");
        strcpy(sdummyString, "S");
        if (strlen(str) == 0) {
            strcat(str, sdummyString);
        } else {
            strcat(str, "/");
            strcat(str, sdummyString);
        }
        matched = 1;
    }
    if (u16Lp_Bp & AN_BP0_Echoed_Nonce) {
        //strcpy(sdummyString, "Echoed Nonce");
        strcpy(sdummyString, "EN");
        if (strlen(str) == 0) {
            strcat(str, sdummyString);
        } else {
            strcat(str, "/");
            strcat(str, sdummyString);
        }
        matched = 1;
    }
    if (u16Lp_Bp & AN_BP0_Pause_Ability) {
        //strcpy(sdummyString, "Pause Ability");
        strcpy(sdummyString, "PA");
        if (strlen(str) == 0) {
            strcat(str, sdummyString);
        } else {
            strcat(str, "/");
            strcat(str, sdummyString);
        }
        matched = 1;
    }
    if (u16Lp_Bp & AN_BP0_RF) {
        strcpy(sdummyString, "RF");
        if (strlen(str) == 0) {
            strcat(str, sdummyString);
        } else {
            strcat(str, "/");
            strcat(str, sdummyString);
        }
        matched = 1;
    }
    if (u16Lp_Bp & AN_BP0_Ack) {
        strcpy(sdummyString, "ACK");
        if (strlen(str) == 0) {
            strcat(str, sdummyString);
        } else {
            strcat(str, "/");
            strcat(str, sdummyString);
        }
        matched = 1;
    }
    if (u16Lp_Bp & AN_BP0_NP) {
        strcpy(sdummyString, "NP");
        if (strlen(str) == 0) {
            strcat(str, sdummyString);
        } else {
            strcat(str, "/");
            strcat(str, sdummyString);
        }
        matched = 1;
    }

    if (!matched) {
        sprintf(str, "%X", u16Lp_Bp);
    }
    return str;
}

char *DecodeBasePage1(uint16_t u16Lp_Bp)
{
    static char str[100]; // Static buffer to hold the decoded string
    char sdummyString[20]; // Temporary buffer for each string component

    str[0] = '\0'; // Initialize the output string

    int matched = 0; // Flag to check if any condition matched
    if (u16Lp_Bp & AN_BP1_Transmitted_Nonce) {
        //strcpy(sdummyString, "Transmitted Nonce");
        strcpy(sdummyString, "TN");
        if (strlen(str) == 0) {
            strcat(str, sdummyString);
        } else {
            strcat(str, "/");
            strcat(str, sdummyString);
        }
        matched = 1;
    }
    if (u16Lp_Bp & AN_BP1_TA_1G) {
        strcpy(sdummyString, "1G");
        if (strlen(str) == 0) {
            strcat(str, sdummyString);
        } else {
            strcat(str, "/");
            strcat(str, sdummyString);
        }
        matched = 1;
    }
    if (u16Lp_Bp & AN_BP1_TA_10G) {
        strcpy(sdummyString, "10G");
        if (strlen(str) == 0) {
            strcat(str, sdummyString);
        } else {
            strcat(str, "/");
            strcat(str, sdummyString);
        }
        matched = 1;
    }
    if (u16Lp_Bp & AN_BP1_TA_25G) {
        strcpy(sdummyString, "25G");
        if (strlen(str) == 0) {
            strcat(str, sdummyString);
        } else {
            strcat(str, "/");
            strcat(str, sdummyString);
        }
        matched = 1;
    }
    if (!matched) {
        sprintf(str, "%X", u16Lp_Bp);
    }
    return str;
}

char *DecodeBasePage2(uint16_t u16Lp_Bp)
{
    static char str[100]; // Static buffer to hold the decoded string
    char sdummyString[20]; // Temporary buffer for each string component

    str[0] = '\0'; // Initialize the output string

    int matched = 0; // Flag to check if any condition matched

    if (u16Lp_Bp & AN_BP2_FEC_Capability) {
        //strcpy(sdummyString, "FEC Capability");
        strcpy(sdummyString, "FC");
        if (strlen(str) == 0) {
            strcat(str, sdummyString);
        } else {
            strcat(str, "/");
            strcat(str, sdummyString);
        }
        matched = 1;
    }
    if (!matched) {
        sprintf(str, "%X", u16Lp_Bp);
    }
    return str;
}

char *DecodeNextPage1(uint16_t u16Lp_Bp)
{
    static char str[100]; // Static buffer to hold the decoded string
    char sdummyString[20]; // Temporary buffer for each string component

    str[0] = '\0'; // Initialize the output string

    int matched = 0; // Flag to check if any condition matched

    if (u16Lp_Bp & AN_NP1_25GKR) {
        strcpy(sdummyString, "25GKR");
        if (strlen(str) == 0) {
            strcat(str, sdummyString);
        } else {
            strcat(str, "/");
            strcat(str, sdummyString);
        }
        matched = 1;
    }
    if (u16Lp_Bp & AN_NP1_25GCR) {
        strcpy(sdummyString, "25GCR");
        if (strlen(str) == 0) {
            strcat(str, sdummyString);
        } else {
            strcat(str, "/");
            strcat(str, sdummyString);
        }
        matched = 1;
    }
    if (!matched) {
        sprintf(str, "%X", u16Lp_Bp);
    }
    return str;
}

char *DecodeNextPage2(uint16_t u16Lp_Bp)
{
    static char str[100]; // Static buffer to hold the decoded string
    char sdummyString[20]; // Temporary buffer for each string component

    str[0] = '\0'; // Initialize the output string

    int matched = 0; // Flag to check if any condition matched

    if (u16Lp_Bp & AN_NP2_F1) {
        strcpy(sdummyString, "F1_FEC");
        if (strlen(str) == 0) {
            strcat(str, sdummyString);
        } else {
            strcat(str, "/");
            strcat(str, sdummyString);
        }
        matched = 1;
    }
    if (u16Lp_Bp & AN_NP2_F2) {
        strcpy(sdummyString, "F2_FEC");
        if (strlen(str) == 0) {
            strcat(str, sdummyString);
        } else {
            strcat(str, "/");
            strcat(str, sdummyString);
        }
        matched = 1;
    }
    if (u16Lp_Bp & AN_NP2_F3) {
        strcpy(sdummyString, "F3_FEC");
        if (strlen(str) == 0) {
            strcat(str, sdummyString);
        } else {
            strcat(str, "/");
            strcat(str, sdummyString);
        }
        matched = 1;
    }
    if (u16Lp_Bp & AN_NP2_F4) {
        strcpy(sdummyString, "F4_FEC");
        if (strlen(str) == 0) {
            strcat(str, sdummyString);
        } else {
            strcat(str, "/");
            strcat(str, sdummyString);
        }
        matched = 1;
    }
    if (!matched) {
        sprintf(str, "%X", u16Lp_Bp);
    }
    return str;
}
char *getTap(uint32_t u32LD_TAPRecieved)
{

    switch (u32LD_TAPRecieved) {
    case eKR_COEF_CM1:
        return "CM1";
    case eKR_COEF_C0:
        return "C0";
    case eKR_COEF_CP1:
        return "CP1";
    case eKR_COEF_INIT:
        return "INIT";
    default:
        return "PRESET";
    }
}

char *getStatusReportSentToLP(uint32_t u32StatusReportSentToLP)
{
    switch (u32StatusReportSentToLP) {
    case KR_COEF_NOT_UPDATED:
        return "KRCNU";
    case KR_COEF_UPDATED:
        return "KRCU";
    case KR_COEF_MINIMUM:
        return "KRCM";
    case KR_COEF_MAXIMUM:
        return "KRCX ";
    default:
        return "UNKNOWN";
    }
}

char *getCommandReceived(KR_Log *log)
{
    if (log->LD_TAPRecieved == eKR_COEF_INIT) {
        return "INIT";
    }

    switch (log->LD_CommandReceived) {
    case eKR_COEF_INCR:
        return "INCR";
    case eKR_COEF_DECR:
        return "DECR";
    case eKR_COEF_HOLD:
        return "HOLD";
    default:
        return "UNKNOWN";
    }
}

char *getCommandBER(uint32_t u32BER_STATE)
{
    switch (u32BER_STATE) {
    case BER_GO_TO_MIN:
        return "BGT_MIN";
    case BER_CALCULATE_BER:
        return "BER_CAL";
    case BER_MOVE_TO_MID_MARK:
        return "BMT_MM";
    default:
        return "BLRXT";
    }
}
void LogEntryAll(FILE *file, int rowNo, KR_Log *log, unsigned int delta)
{

    fprintf(file, "%-5d %-10u %-10d %-30s %-30s %-10s %-10s %-10d %-10d %-10d %-20s %-20s %-20u %-20u %-20u %-20u\n",
            rowNo,
            log->timestamp,
            delta,
            getIRQ(log->IRQ),
            converttoenum(enum_state_machine_string, log->Statemachine),
            getTap(log->LD_TAPRecieved),
            getCommandReceived(log),
            log->CM1,
            log->C0,
            log->CP1,
            getCommandBER(log->BER_STATE),
            getStatusReportSentToLP(log->StatusReportSentToLP),
            log->EYE_HIGHT,
            log->CommandSentToLP,
            log->TAPSentToLP,
            log->StatusReportReceivedFromLP);
}
void LogEntryBER(FILE *file, int rowNo, KR_Log *log)
{
    // Print to console
    cli_printf("%-5d %-20s %-20s %-20s %-20s %-20u %-20s\n",
               rowNo,
               getTap(log->LD_TAPRecieved),
               getStatusReportSentToLP(log->StatusReportSentToLP),
               getCommandReceived(log),
               getCommandBER(log->BER_STATE),
               log->timestamp,
               getIRQ(log->IRQ));

    // Write to file
    fprintf(file, "%-5d %-20s %-20s %-30s %-20s\n",
            rowNo,
            getTap(log->LD_TAPRecieved),
            getStatusReportSentToLP(log->StatusReportSentToLP),
            getCommandReceived(log),
            getIRQ(log->IRQ));
}
void LogEntryIRQ(FILE *file, int rowNo, KR_Log *log, unsigned int delta)
{
    // Print to console
    cli_printf("%-5d %-20u %-20d %-30s %-20s\n",
               rowNo,
               log->timestamp,
               delta,
               getIRQ(log->IRQ),
               converttoenum(enum_state_machine_string, log->Statemachine));

    // Write to file
    fprintf(file, "%-5d %-20u %-20d %-30s %-20s\n",
            rowNo,
            log->timestamp,
            delta,
            getIRQ(log->IRQ),
            converttoenum(enum_state_machine_string, log->Statemachine));
}

void LogEntryEQ(FILE *file, int rowNo, KR_Log *log)
{
    // Print to console
    cli_printf("%-5d %-20s %-20s %-20d %-20d %-20d %-20s %-20u\n",
               rowNo,
               getTap(log->LD_TAPRecieved),
               getCommandReceived(log),
               log->CM1,
               log->C0,
               log->CP1,
               getStatusReportSentToLP(log->StatusReportSentToLP),
               log->timestamp
              );

    // Write to file
    fprintf(file, "%-5d %-20s %-20s %-20d %-20d %-20d %-20s %-20u\n",
            rowNo,
            getTap(log->LD_TAPRecieved),
            getCommandReceived(log),
            log->CM1,
            log->C0,
            log->CP1,
            getStatusReportSentToLP(log->StatusReportSentToLP),
            log->timestamp);
}

void LogEntryANEG(FILE *file, int rowNo, ANEG_Log *log, unsigned int delta)
{
    // Print to console
    cli_printf("%-3d %-10u %-10u %-20s %-20s %-10s %-10s %-10s %-10s %-10s %-10s\n",
               rowNo,
               log->timestamp,
               delta,
               getIRQ(log->IRQ),
               converttoenum(enum_state_machine_string, log->Statemachine),
               DecodeBasePage0(log->LPage_BP0),
               DecodeBasePage1(log->LPage_BP1),
               DecodeBasePage2(log->LPage_BP2),
               DecodeNextPage1(log->LPage_NP0),
               DecodeNextPage2(log->LPage_NP1),
               DecodeNextPage2(log->LPage_NP2));


    // Write to file
    fprintf(file, "%-3d %-10u %-10u %-20s %-20s %-10s %-10s %-10s %-10s %-10s %-10s\n",
            rowNo,
            log->timestamp,
            delta,
            getIRQ(log->IRQ),
            converttoenum(enum_state_machine_string, log->Statemachine),
            DecodeBasePage0(log->LPage_BP0),
            DecodeBasePage1(log->LPage_BP1),
            DecodeBasePage2(log->LPage_BP2),
            DecodeNextPage1(log->LPage_NP0),
            DecodeNextPage2(log->LPage_NP1),
            DecodeNextPage2(log->LPage_NP2));
}

char *HexToStr(const uint8_t *bytes, int length)
{
    int bufferSize = length * 3 + 1; // Calculate required buffer size
    char *hexStr = (char *)malloc(bufferSize);
    if (hexStr == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        return NULL;
    }

    char *ptr = hexStr;
    for (int i = 0; i < length; i++) {
        ptr += sprintf(ptr, "%02X ", bytes[i]);
    }

    if (length > 0) {
        *(ptr - 1) = '\0'; // Null-terminate the string
    }

    return hexStr;
}


void SaveMemoryBytesToFile(FILE *file, uint8_t *bytes, uint16_t wLen, uint32_t memAddr)
{
    char szMessage[4096];
    uint8_t abyNewLine[2] = { 0x0D, 0x0A };

    // Write bytes to file as 16-byte chunks with address value for each chunk
    int i = 0, j = 16;
    while (wLen > i) {
        if ((wLen - i) < 16) {
            j = wLen - i;
        }

        // Convert bytes to hex string
        char *hexString = HexToStr(&bytes[i], j);
        if (hexString == NULL) {
            fprintf(stderr, "Failed to convert bytes to hex string\n");
            return; // Handle error appropriately
        }

        snprintf(szMessage, sizeof(szMessage), "0x%08x : %s", memAddr + i, hexString);

        fwrite(szMessage, 1, strlen(szMessage), file);
        fwrite(abyNewLine, 1, sizeof(abyNewLine), file);

        // Free the allocated memory for hexString
        free(hexString);

        i += 16;
    }
}
void little_to_big_endian(uint8_t *buffer, int size)
{
    for (int i = 0; i < size / 2; i++) {
        uint8_t temp = buffer[i];
        buffer[i] = buffer[size - i - 1];
        buffer[size - i - 1] = temp;
    }
}
int findMax(int arr[], int n)
{
    int max = arr[0];
    for (int i = 1; i < n; i++)
        if (arr[i] > max) {
            max = arr[i];
        }
    return max;
}

int findMin(int arr[], int n)
{
    int min = arr[0];
    for (int i = 1; i < n; i++)
        if (arr[i] < min) {
            min = arr[i];
        }
    return min;
}
/*
u8Line_host - 0 - line, 1 - host
*/
void processKrLogging(struct mepa_device *dev, phy_kr_log_sel_t *mreq, mepa_port_no_t port_no, uint8_t u8Line_host, uint32_t u32MemReadAddress, uint16_t u16DataLength)
{

    KR_Log asKRLog;
    memset(&asKRLog, 0, sizeof(KR_Log));

    KR_Summary_Log asKRSummaryLog;
    memset(&asKRSummaryLog, 0, sizeof(KR_Summary_Log));

    ANEG_Log asANEGLog;
    memset(&asANEGLog, 0, sizeof(ANEG_Log));

    mepa_rc rc = MESA_RC_OK;

    char szFileNamePortKRStatus[256];
    char szFileNamePortKRStatusAll[256];
    FILE *fileHandlePortKRAll = NULL;

    const char *side = (u8Line_host == 0) ? "LINE" : "HOST";
    if (mreq->eq) {
        snprintf(szFileNamePortKRStatus, sizeof(szFileNamePortKRStatus), "/root/mepa_scripts/Port_KR_Status_EQ_%s_PORT%d.log", side, port_no);
    } else if (mreq->ber) {
        snprintf(szFileNamePortKRStatus, sizeof(szFileNamePortKRStatus), "/root/mepa_scripts/Port_KR_Status_BER_%s_PORT%d.log", side, port_no);
    } else if (mreq->irq) {
        snprintf(szFileNamePortKRStatus, sizeof(szFileNamePortKRStatus), "/root/mepa_scripts/Port_KR_Status_IRQ_%s_PORT%d.log", side, port_no);
    } else if (mreq->all) {
        snprintf(szFileNamePortKRStatus, sizeof(szFileNamePortKRStatus), "/root/mepa_scripts/Port_KR_Status_Summary_%s_PORT%d.log", side, port_no);
        snprintf(szFileNamePortKRStatusAll, sizeof(szFileNamePortKRStatusAll), "/root/mepa_scripts/Port_KR_Status_All_%s_PORT%d.log", side, port_no);
    } else if (mreq->aneg) {
        snprintf(szFileNamePortKRStatus, sizeof(szFileNamePortKRStatus), "/root/mepa_scripts/Port_KR_Status_ANEG_%s_PORT%d.log", side, port_no);
    }


    int rowNo = 0;
    unsigned int Delta = 0;
    int isCP1 = 0;
    int isC0  = 0;
    int prev_Timestamp_value = 0;
    int arrcm1[3001] = { 0 };
    int arrc0[3001] = {0};
    int arrcp1[3001] = {0};
    int aEYEHTCM1[3001] = {0};
    int aEYEHTCP1[3001] = { 0 };
    int aEYEHTC0[3001] = {0};
    int ixe = 0, iye = 0, ize = 0;
    uint8_t isLogAvailable = 0;
    uint8_t istimestampAvailable = 0;
    int timestamp0 = 0, timestamp1 = 0;

    // Read from memory
    uint16_t u16DataLen = 0;
    uint8_t u8DataBuffer[MAX_MEMRW_DATA_LEN];

    uint8_t pu8DataBuffer[u16DataLength];

    uint32_t u32MemAddr = u32MemReadAddress;
    u16DataLen = MAX_MEMRW_DATA_LEN;
    uint16_t u16Temp = u16DataLength;
    int i = 0;
    while (u16Temp) {
        rc = lan80xx_memory_read(dev, u32MemAddr, u8DataBuffer, u16DataLen);
        if (rc != MESA_RC_OK) {
            T_E(" Read Failed Address: 0x%x, Length: 0x%x  \n", u32MemAddr, u16DataLen);
            break;
        }

        memcpy(&pu8DataBuffer[i], u8DataBuffer, u16DataLen);

        u32MemAddr += MAX_MEMRW_DATA_LEN;
        i += u16DataLen;

        u16Temp = u16Temp - u16DataLen;
        if (u16Temp < MAX_MEMRW_DATA_LEN) {
            u16DataLen = u16Temp;
        }
    }

    char szFileNamePortKRLogMemoryDump[256];

    // Format the file name
    snprintf(szFileNamePortKRLogMemoryDump, sizeof(szFileNamePortKRLogMemoryDump), "/root/mepa_scripts/Port_KR_Log_Memory_Dump_%s_PORT%d.log", side, port_no);

    // Open the file for writing
    FILE *fileHandlePortKRLogMemoryDump = fopen(szFileNamePortKRLogMemoryDump, "wb");
    if (fileHandlePortKRLogMemoryDump == NULL) {
        // Handle error if file cannot be opened
        perror("Failed to open file");
        return;
    }

    // Save memory bytes to the file
    SaveMemoryBytesToFile(fileHandlePortKRLogMemoryDump, pu8DataBuffer, u16DataLength, u32MemReadAddress);

    // Close the file
    fclose(fileHandlePortKRLogMemoryDump);

    FILE *file = fopen(szFileNamePortKRStatus, "w");
    if (file == NULL) {
        T_E("Error in file open\n");
        return;
    }

    mepa_bool_t bHeaderUpdate = 0;
    int ischecktime = 0;
    if (mreq->aneg) {
        for (int j = 0; j < (u16DataLength);) {
            memcpy((uint32_t *)&asANEGLog, &pu8DataBuffer[j], sizeof(ANEG_Log));
            if (asANEGLog.IRQ & 0xC0017FCF) {
                //No Operation
            } else {
                j += sizeof(KR_Log);
                continue;
            }
            Delta = (j == 0) ? 0 : (asANEGLog.timestamp - prev_Timestamp_value);
            prev_Timestamp_value = asANEGLog.timestamp;

            if (!bHeaderUpdate) {
                cli_printf("\nPort : %d %s\n", port_no, side);
                cli_printf("\nS-Selector, EN-Echoed Nonce, PA-Pause Ability, TN - Transmitted nonce, FC - FEC Capability\n\n");
                cli_printf("\n%-3s %-10s %-10s %-20s %-20s %-10s %-10s %-10s %-10s %-10s %-10s\n",
                           "S.No", "TimeStamp", "Delta", "IRQ", "Statemachine",
                           "LP_BP0", "LP_BP1", "LP_BP2", "LP_NP0", "LP_NP1", "LP_NP2");
                cli_printf("-----------------------------------------------------------------------------------------------------------------------------------\n");

                fprintf(file, "S-Selector, EN-Echoed Nonce, PA-Pause Ability, TN - Transmitted nonce, FC - FEC Capability\n\n");
                fprintf(file, "%-3s %-10s %-10s %-20s %-20s %-10s %-10s %-10s %-10s %-10s %-10s\n",
                        "S.No", "TimeStamp", "Delta", "IRQ", "Statemachine",
                        "LP_BP0", "LP_BP1", "LP_BP2", "LP_NP0", "LP_NP1", "LP_NP2");
                fprintf(file, "-----------------------------------------------------------------------------------------------------------------------------------\n");
                bHeaderUpdate = 1;
            }
            LogEntryANEG(file, rowNo, &asANEGLog, Delta);

            if (asANEGLog.IRQ & 0xC0017FCF) {
                j += sizeof(ANEG_Log);
            } else if (asANEGLog.IRQ & 0x18E00000) {
                j += sizeof(KR_Log);
            }
            if (asANEGLog.IRQ & IRQ_VEC0_AN_GOOD) {
                break;
            }


            rowNo++;
        }

    } else {
        for (int j = 0; j < (u16DataLength);) {
            memcpy((uint32_t *)&asKRLog, &pu8DataBuffer[j], sizeof(KR_Log));

            if ((asKRLog.IRQ & 0x18E00000)) {
                if (!ischecktime) {
                    timestamp0 = asKRLog.timestamp;
                    ischecktime = 1;
                }
            } else {
                memcpy((uint32_t *)&asANEGLog, &pu8DataBuffer[j], sizeof(ANEG_Log));
                if (asANEGLog.IRQ & IRQ_VEC0_AN_GOOD) {
                    timestamp1 = asANEGLog.timestamp;
                    break;
                }
                j += sizeof(ANEG_Log);
                continue;
            }

            if (asKRLog.timestamp == 0 && istimestampAvailable == 0) {
                isLogAvailable++;
                if (isLogAvailable == 10) {
                    cli_printf("No Logs Available\n");
                    T_E("No Logs available\n");
                    fprintf(file, "No Logs available\n");
                    fclose(file);
                    return;
                }
                continue;
            }

            if ((asKRLog.LD_CommandReceived == 7) && (asKRLog.LD_TAPRecieved == 7)) {
                j += sizeof(KR_Log);
                continue;
            }

            Delta = (j == 0) ? 0 : (asKRLog.timestamp - prev_Timestamp_value);
            prev_Timestamp_value = asKRLog.timestamp;

            Delta = (j == 0) ? 0 : (asKRLog.timestamp - prev_Timestamp_value);
            prev_Timestamp_value = asKRLog.timestamp;

            if (asKRLog.LD_TAPRecieved == eKR_COEF_CM1 && isCP1) {
                asKRLog.LD_TAPRecieved = eKR_COEF_CP1;
            }
            if (asKRLog.LD_TAPRecieved == eKR_COEF_CM1 && isC0) {
                asKRLog.LD_TAPRecieved = eKR_COEF_C0;
            }
            if (asKRLog.LD_TAPRecieved == eKR_COEF_CP1) {
                isCP1 = 1;
            }
            if (asKRLog.LD_TAPRecieved == eKR_COEF_C0) {
                isC0 = 1;
                isCP1 = 0;
            }

            istimestampAvailable = 1;
            arrcm1[rowNo] = asKRLog.CM1;
            arrc0[rowNo] = asKRLog.C0;
            arrcp1[rowNo] = asKRLog.CP1;

            if (asKRLog.BER_STATE == BER_LOCAL_RX_TRAINED) {
                asKRSummaryLog.wAneg_status |= 0x80;
            }

            if ((asKRLog.LD_TAPRecieved == eKR_COEF_CM1) && (asKRLog.IRQ & IRQ_VEC1_LPCVALID)) {
                aEYEHTCM1[ixe++] = asKRLog.EYE_HIGHT;
            } else if ((asKRLog.LD_TAPRecieved == eKR_COEF_CP1) && (asKRLog.IRQ & IRQ_VEC1_LPCVALID)) {
                aEYEHTCP1[iye++] = asKRLog.EYE_HIGHT;
            } else if ((asKRLog.LD_TAPRecieved == eKR_COEF_C0) && (asKRLog.IRQ & IRQ_VEC1_LPCVALID)) {
                aEYEHTC0[ize++] = asKRLog.EYE_HIGHT;
            }

            // Format and print the log entry
            if (mreq->irq) {
                if (!bHeaderUpdate) {
                    cli_printf("\nPort : %d %s\n", port_no, side);
                    cli_printf("\n%-5s %-20s %-20s %-30s %-20s\n", "S.No", "TimeStamp", "Delta", "IRQ", "Statemachine");
                    cli_printf("---------------------------------------------------------------------------------------------\n");

                    fprintf(file, "%-5s %-20s %-20s %-30s %-20s\n", "S.No", "TimeStamp", "Delta", "IRQ", "Statemachine");
                    fprintf(file, "----------------------------------------------------------------------------------------\n");

                    bHeaderUpdate = 1;
                }
                LogEntryIRQ(file, rowNo, &asKRLog, Delta);
            }
            if (mreq->ber) {
                if (!bHeaderUpdate) {
                    cli_printf("\nPort : %d %s\n", port_no, side);
                    cli_printf("\nRx LPS: KRCNU - KR_COEF_NOT_UPDATED, KRCU - KR_COEF_UPDATED, KRCM - KR_COEF_MINIMUM, KRCX - KR_COEF_MAXIMUM\n\n");
                    cli_printf("\n%-5s %-20s %-20s %-20s %-20s %-20s %-20s\n", "S.No", "RxTAP", "RxLPS", "TxLPC", "BER State", "Timestamp", "IRQ's");
                    cli_printf("----------------------------------------------------------------------------------------------------------------------------\n");

                    fprintf(file, "\nRx LPS: KRCNU - KR_COEF_NOT_UPDATED, KRCU - KR_COEF_UPDATED, KRCM - KR_COEF_MINIMUM, KRCX - KR_COEF_MAXIMUM\n");
                    fprintf(file, "\n%-5s %-20s %-20s %-20s %-20s %-20s %-20s\n", "S.No", "RxTAP", "RxLPS", "TxLPC", "BER State", "Timestamp", "IRQ's");
                    fprintf(file, "--------------------------------------------------------------------------------------------------------------------------\n");
                    bHeaderUpdate = 1;
                }
                LogEntryBER(file, rowNo, &asKRLog);
            }
            if (mreq->eq) {
                if (!bHeaderUpdate) {
                    cli_printf("\nPort : %d %s\n", port_no, side);
                    cli_printf("\n%-5s %-20s %-20s %-20s %-20s %-20s %-20s %-20s\n", "S.No", "TAP(LP)", "CMD(LP)", "CM1(LD)", "Ampl(LD)", "CP1(LD)", "Status(LD)", "TimeStamp");
                    cli_printf("----------------------------------------------------------------------------------------------------------------------------------------------\n");

                    fprintf(file, "%-5s %-20s %-20s %-20s %-20s %-20s %-20s %-20s\n", "S.No", "TAP(LP)", "CMD(LP)", "CM1(LD)", "Ampl(LD)", "CP1(LD)", "Status(LD)", "TimeStamp");
                    fprintf(file, "--------------------------------------------------------------------------------------------------------------------------------------------\n");

                    bHeaderUpdate = 1;
                }
                LogEntryEQ(file, rowNo, &asKRLog);
            }
            if (mreq->all) {
                if (!bHeaderUpdate) {
                    cli_printf("\nPort : %d %s\n", port_no, side);
                    fileHandlePortKRAll = fopen(szFileNamePortKRStatusAll, "w");
                    if (fileHandlePortKRAll == NULL) {
                        T_E("Error in file open\n");
                        return;
                    }
                    fprintf(fileHandlePortKRAll, "BER State: BGT_MIN - BER_GO_TO_MIN, BER_CALC - BER_CALCULATE_BER, BMT_MM - BER_MOVE_TO_MID_MARK, BLRXT - BER_LOCAL_RX_TRAINED\n");
                    fprintf(fileHandlePortKRAll, "Rx LPS: KRCNU - KR_COEF_NOT_UPDATED, KRCU - KR_COEF_UPDATED, KRCM - KR_COEF_MINIMUM, KRCX - KR_COEF_MAXIMUM\n");

                    fprintf(fileHandlePortKRAll, "%-5s %-10s %-10s %-30s %-30s %-10s %-10s %-10s %-10s %-10s %-20s %-20s %-20s %-20s %-20s %-20s\n",
                            "S.No", "TimeStamp", "Delta", "IRQ", "Statemachine", "TAP", "Tx LPC", "CM1", "CM0", "CP1", "BER State", "Rx LPS", "Eye Height", "CMD Sent To LP", "TAP Sent To LP", "Status Received from LP");
                    fprintf(fileHandlePortKRAll, "----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------\n");

                    bHeaderUpdate = 1;
                }
                LogEntryAll(fileHandlePortKRAll, rowNo, &asKRLog, Delta);
            }

            j += sizeof(KR_Log);
            rowNo++;
        }

    }


    if (mreq->all) {
        if (fileHandlePortKRAll != NULL) {
            fclose(fileHandlePortKRAll);
            fileHandlePortKRAll = NULL; // Optional: avoid double close
        }

        char *PortKRStatusbuffer = NULL;
        PortKRStatusbuffer = (char *)calloc(8192, sizeof(char));

        if (PortKRStatusbuffer == NULL) {
            T_E("Memory allocation failed\n");
            return;
        }

        uint8_t bydatabuf[5] = { 0 };
        lan80xx_memory_read(dev, 0x40040048, bydatabuf, 4);
        little_to_big_endian(bydatabuf, 4);

        uint32_t dwANstatus;
        if (u8Line_host == 0) {
            lan80xx_phy_csr_read(dev, port_no, LINE_KR_ANEG_DEVICE, AN_STS0, &dwANstatus);
        } else {
            lan80xx_phy_csr_read(dev, port_no, HOST_KR_ANEG_DEVICE, AN_STS0, &dwANstatus);
        }

        //speed and rfec values required for Port KR Status
        uint32_t u32EthSts;
        mesa_bool_t bSpeed25G, bSpeed25GKRS, bSpeed10G, bRfec, bRsfec, bSpeed1G;
        if (dwANstatus & AN_COMPLETE) {
            asKRSummaryLog.wAneg_status |= 0x01;
        } else {
            asKRSummaryLog.wAneg_status |= 0x0;
        }
        if (u8Line_host == 0) {
            lan80xx_phy_csr_read(dev, port_no, LINE_KR_ANEG_DEVICE, ETH_STS, &u32EthSts);
        } else {
            lan80xx_phy_csr_read(dev, port_no, HOST_KR_ANEG_DEVICE, ETH_STS, &u32EthSts);
        }
        bSpeed25G = (u32EthSts & AN_BP_ETH_STS_NEG_25G_KR) ? TRUE : FALSE;
        bSpeed25GKRS = (u32EthSts & AN_BP_ETH_STS_NEG_25G_KR_S) ? TRUE : FALSE;
        bSpeed10G = (u32EthSts & AN_BP_ETH_STS_NEG_10G_KR) ? TRUE : FALSE;
        bRfec = (u32EthSts & AN_BP_ETH_STS_NEG_R_FEC) ? TRUE : FALSE;
        bRsfec = (u32EthSts & AN_BP_ETH_STS_NEG_RS_FEC) ? TRUE : FALSE;
        bSpeed1G = (u32EthSts & AN_BP_ETH_STS_NEG_1G_KX) ? TRUE : FALSE;

        if (bSpeed25G | bSpeed25GKRS) {
            asKRSummaryLog.wAneg_status |= 0x08;
        } else if (bSpeed10G) {
            asKRSummaryLog.wAneg_status |= 0x04;
        } else if (bSpeed1G) {
            asKRSummaryLog.wAneg_status |= 0x02;
        }

        if (bRfec) {
            asKRSummaryLog.wAneg_status |= 0x20;
        }
        if (bRsfec) {
            asKRSummaryLog.wAneg_status |= 0x40;
        }

        asKRSummaryLog.LP_CM1_MAX = findMax(arrcm1, rowNo);
        asKRSummaryLog.LP_CM1_END = findMin(arrcm1, rowNo);
        asKRSummaryLog.LP_C0_MAX = findMax(arrc0, rowNo);
        asKRSummaryLog.LP_C0_END = findMin(arrc0, rowNo);
        asKRSummaryLog.LP_CP1_MAX = findMax(arrcp1, rowNo);
        asKRSummaryLog.LP_CP1_END = findMin(arrcp1, rowNo);

        /*LD VGA, LD EDC, LDEQR*/
        uint32_t u32ldvga = 0;
        uint32_t u32ldedc = 0;
        uint32_t u32ldEQR = 0;
        if (u8Line_host == 0) {
            lan80xx_phy_csr_read(dev, port_no, KR_LINEID, GRP0_LANE_21, &u32ldvga);
            lan80xx_phy_csr_read(dev, port_no, KR_LINEID, GRP1_LANE_DD, &u32ldedc);
            lan80xx_phy_csr_read(dev, port_no, KR_LINEID, GRP0_LANE_22, &u32ldEQR);
        } else {
            lan80xx_phy_csr_read(dev, port_no, KR_HOSTID, GRP0_LANE_21, &u32ldvga);
            lan80xx_phy_csr_read(dev, port_no, KR_HOSTID, GRP1_LANE_DD, &u32ldedc);
            lan80xx_phy_csr_read(dev, port_no, KR_HOSTID, GRP0_LANE_22, &u32ldEQR);
        }

        asKRSummaryLog.LD_VGA = u32ldvga & 0x1F;
        asKRSummaryLog.LD_EDC = u32ldedc & 0x0F;
        asKRSummaryLog.LD_EQR = (u32ldEQR & 0xF0) >> 4;

        asKRSummaryLog.CURR_EYE_HT = findMax(aEYEHTC0, rowNo);
        asKRSummaryLog.TRAINING_TIME_MS = timestamp1 - timestamp0;

        for (int ii = 0; ii < 64; ii++) {
            asKRSummaryLog.EYE_HT_CM1[ii] = aEYEHTCM1[ii];
            asKRSummaryLog.EYE_HT_CP1[ii] = aEYEHTCP1[ii];
            asKRSummaryLog.EYE_HT_C0[ii] = aEYEHTC0[ii];
        }

        /* LD_CM1, LD_C0, LD_CP */
        asKRSummaryLog.LD_CM1 = asKRLog.CM1;
        asKRSummaryLog.LD_CP = asKRLog.CP1;
        asKRSummaryLog.LD_C0 = asKRLog.C0;

        /*FEC Corrected Error*/
        uint32_t dwPCS25G_FEC74_CERR_CNT_L = 0, dwPCS25G_FEC74_CERR_CNT_H = 0;
        if (u8Line_host == 0) {
            lan80xx_phy_csr_read(dev, port_no, LINE_PCS_CFG, 0xF3, &dwPCS25G_FEC74_CERR_CNT_L);
            lan80xx_phy_csr_read(dev, port_no, LINE_PCS_CFG, 0xF4, &dwPCS25G_FEC74_CERR_CNT_H);
        } else {
            lan80xx_phy_csr_read(dev, port_no, HOST_PCS_CFG, 0xF3, &dwPCS25G_FEC74_CERR_CNT_L);
            lan80xx_phy_csr_read(dev, port_no, HOST_PCS_CFG, 0xF4, &dwPCS25G_FEC74_CERR_CNT_H);
        }
        asKRSummaryLog.FEC_CORRECTION = MAKEDWORD(dwPCS25G_FEC74_CERR_CNT_L, dwPCS25G_FEC74_CERR_CNT_H);


        /*FEC UnCorrected Error*/
        uint32_t dwPCS25G_FEC74_NCERR_CNT_L = 0, dwPCS25G_FEC74_NCERR_CNT_H = 0;
        if (u8Line_host == 0) {
            lan80xx_phy_csr_read(dev, port_no, LINE_PCS_CFG, 0xF5, &dwPCS25G_FEC74_NCERR_CNT_L);
            lan80xx_phy_csr_read(dev, port_no, LINE_PCS_CFG, 0xF6, &dwPCS25G_FEC74_NCERR_CNT_H);
        } else {
            lan80xx_phy_csr_read(dev, port_no, HOST_PCS_CFG, 0xF5, &dwPCS25G_FEC74_NCERR_CNT_L);
            lan80xx_phy_csr_read(dev, port_no, HOST_PCS_CFG, 0xF6, &dwPCS25G_FEC74_NCERR_CNT_H);
        }
        asKRSummaryLog.FEC_UNCORRECTION = MAKEDWORD(dwPCS25G_FEC74_NCERR_CNT_L, dwPCS25G_FEC74_NCERR_CNT_H);

        sprintf(PortKRStatusbuffer,
                "ANEG Completed : %s \n"
                "Speed : %s \n"
                "R-FEC (CL-74) : %s \n"
                "RS-FEC (CL-108) : %s \n\n"
                "Training Results: \n"
                "LP CM1 MAX/END : %d/%d \n"
                "LP C0  MAX/END : %d/%d \n"
                "LP CP1 MAX/END : %d/%d \n"
                "Eye Height CM1 : %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d \n"
                "Eye Height C0 : %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d \n"
                "Eye Height CP1 : %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d \n\n"
                "LD CM (tap_dly) : %d \n"
                "LD C0 (amplitude) : %d \n"
                "LD CP (tap_adv) : %d \n\n"
                "LD VGA : %d \n"
                "LD EDC : %d \n"
                "LD EQR : %d \n\n"
                "FEC Corr./Uncor.  : %d/%d \n\n"
                "Current eye height : %d \n"
                "Training time : %d micro seconds\n"
                "Training status : %s\n"
                "DME_Violation_Count: %s\n",
                ((asKRSummaryLog.wAneg_status & 0x01) == 1) ? "Yes" : "No",
                ((asKRSummaryLog.wAneg_status & 0x02) == 0x02) ? "1G" :
                ((asKRSummaryLog.wAneg_status & 0x04) == 0x04) ? "10G" :
                ((asKRSummaryLog.wAneg_status & 0x08) == 0x08) ? "25G" : "Undefined",
                ((asKRSummaryLog.wAneg_status & 0x20) == 0x20) ? "Enabled" : "Disabled",
                ((asKRSummaryLog.wAneg_status & 0x40) == 0x40) ? "Enabled" : "Disabled",
                asKRSummaryLog.LP_CM1_MAX, asKRSummaryLog.LP_CM1_END,
                asKRSummaryLog.LP_C0_MAX, asKRSummaryLog.LP_C0_END,
                asKRSummaryLog.LP_CP1_MAX, asKRSummaryLog.LP_CP1_END,
                asKRSummaryLog.EYE_HT_CM1[0], asKRSummaryLog.EYE_HT_CM1[1], asKRSummaryLog.EYE_HT_CM1[2],
                asKRSummaryLog.EYE_HT_CM1[3], asKRSummaryLog.EYE_HT_CM1[4], asKRSummaryLog.EYE_HT_CM1[5],
                asKRSummaryLog.EYE_HT_CM1[6], asKRSummaryLog.EYE_HT_CM1[7], asKRSummaryLog.EYE_HT_CM1[8],
                asKRSummaryLog.EYE_HT_CM1[9], asKRSummaryLog.EYE_HT_CM1[10], asKRSummaryLog.EYE_HT_CM1[11],
                asKRSummaryLog.EYE_HT_CM1[12], asKRSummaryLog.EYE_HT_CM1[13], asKRSummaryLog.EYE_HT_CM1[14],
                asKRSummaryLog.EYE_HT_CM1[15], asKRSummaryLog.EYE_HT_CM1[16], asKRSummaryLog.EYE_HT_CM1[17],
                asKRSummaryLog.EYE_HT_CM1[18], asKRSummaryLog.EYE_HT_CM1[19], asKRSummaryLog.EYE_HT_CM1[20],
                asKRSummaryLog.EYE_HT_CM1[21], asKRSummaryLog.EYE_HT_CM1[22], asKRSummaryLog.EYE_HT_CM1[23],
                asKRSummaryLog.EYE_HT_CM1[24], asKRSummaryLog.EYE_HT_CM1[25], asKRSummaryLog.EYE_HT_CM1[26],
                asKRSummaryLog.EYE_HT_CM1[27], asKRSummaryLog.EYE_HT_CM1[28], asKRSummaryLog.EYE_HT_CM1[29],
                asKRSummaryLog.EYE_HT_CM1[30], asKRSummaryLog.EYE_HT_CM1[31], asKRSummaryLog.EYE_HT_CM1[32],
                asKRSummaryLog.EYE_HT_CM1[33], asKRSummaryLog.EYE_HT_CM1[34], asKRSummaryLog.EYE_HT_CM1[35],
                asKRSummaryLog.EYE_HT_CM1[36], asKRSummaryLog.EYE_HT_CM1[37], asKRSummaryLog.EYE_HT_CM1[38],
                asKRSummaryLog.EYE_HT_CM1[39], asKRSummaryLog.EYE_HT_CM1[40], asKRSummaryLog.EYE_HT_CM1[41],
                asKRSummaryLog.EYE_HT_CM1[42], asKRSummaryLog.EYE_HT_CM1[43], asKRSummaryLog.EYE_HT_CM1[44],
                asKRSummaryLog.EYE_HT_CM1[45], asKRSummaryLog.EYE_HT_CM1[46], asKRSummaryLog.EYE_HT_CM1[47],
                asKRSummaryLog.EYE_HT_CM1[48], asKRSummaryLog.EYE_HT_CM1[49], asKRSummaryLog.EYE_HT_CM1[50],
                asKRSummaryLog.EYE_HT_CM1[51], asKRSummaryLog.EYE_HT_CM1[52], asKRSummaryLog.EYE_HT_CM1[53],
                asKRSummaryLog.EYE_HT_CM1[54], asKRSummaryLog.EYE_HT_CM1[55], asKRSummaryLog.EYE_HT_CM1[56],
                asKRSummaryLog.EYE_HT_CM1[57], asKRSummaryLog.EYE_HT_CM1[58], asKRSummaryLog.EYE_HT_CM1[59],
                asKRSummaryLog.EYE_HT_CM1[60], asKRSummaryLog.EYE_HT_CM1[61], asKRSummaryLog.EYE_HT_CM1[62],
                asKRSummaryLog.EYE_HT_CM1[63], asKRSummaryLog.EYE_HT_C0[0], asKRSummaryLog.EYE_HT_C0[1],
                asKRSummaryLog.EYE_HT_C0[2], asKRSummaryLog.EYE_HT_C0[3], asKRSummaryLog.EYE_HT_C0[4],
                asKRSummaryLog.EYE_HT_C0[5], asKRSummaryLog.EYE_HT_C0[6], asKRSummaryLog.EYE_HT_C0[7],
                asKRSummaryLog.EYE_HT_C0[8], asKRSummaryLog.EYE_HT_C0[9], asKRSummaryLog.EYE_HT_C0[10],
                asKRSummaryLog.EYE_HT_C0[11], asKRSummaryLog.EYE_HT_C0[12], asKRSummaryLog.EYE_HT_C0[13],
                asKRSummaryLog.EYE_HT_C0[14], asKRSummaryLog.EYE_HT_C0[15], asKRSummaryLog.EYE_HT_C0[16],
                asKRSummaryLog.EYE_HT_C0[17], asKRSummaryLog.EYE_HT_C0[18], asKRSummaryLog.EYE_HT_C0[19],
                asKRSummaryLog.EYE_HT_C0[20], asKRSummaryLog.EYE_HT_C0[21], asKRSummaryLog.EYE_HT_C0[22],
                asKRSummaryLog.EYE_HT_C0[23], asKRSummaryLog.EYE_HT_C0[24], asKRSummaryLog.EYE_HT_C0[25],
                asKRSummaryLog.EYE_HT_C0[26], asKRSummaryLog.EYE_HT_C0[27], asKRSummaryLog.EYE_HT_C0[28],
                asKRSummaryLog.EYE_HT_C0[29], asKRSummaryLog.EYE_HT_C0[30], asKRSummaryLog.EYE_HT_C0[31],
                asKRSummaryLog.EYE_HT_C0[32], asKRSummaryLog.EYE_HT_C0[33], asKRSummaryLog.EYE_HT_C0[34],
                asKRSummaryLog.EYE_HT_C0[35], asKRSummaryLog.EYE_HT_C0[36], asKRSummaryLog.EYE_HT_C0[37],
                asKRSummaryLog.EYE_HT_C0[38], asKRSummaryLog.EYE_HT_C0[39], asKRSummaryLog.EYE_HT_C0[40],
                asKRSummaryLog.EYE_HT_C0[41], asKRSummaryLog.EYE_HT_C0[42], asKRSummaryLog.EYE_HT_C0[43],
                asKRSummaryLog.EYE_HT_C0[44], asKRSummaryLog.EYE_HT_C0[45], asKRSummaryLog.EYE_HT_C0[46],
                asKRSummaryLog.EYE_HT_C0[47], asKRSummaryLog.EYE_HT_C0[48], asKRSummaryLog.EYE_HT_C0[49],
                asKRSummaryLog.EYE_HT_C0[50], asKRSummaryLog.EYE_HT_C0[51], asKRSummaryLog.EYE_HT_C0[52],
                asKRSummaryLog.EYE_HT_C0[53], asKRSummaryLog.EYE_HT_C0[54], asKRSummaryLog.EYE_HT_C0[55],
                asKRSummaryLog.EYE_HT_C0[56], asKRSummaryLog.EYE_HT_C0[57], asKRSummaryLog.EYE_HT_C0[58],
                asKRSummaryLog.EYE_HT_C0[59], asKRSummaryLog.EYE_HT_C0[60], asKRSummaryLog.EYE_HT_C0[61],
                asKRSummaryLog.EYE_HT_C0[62], asKRSummaryLog.EYE_HT_C0[63], asKRSummaryLog.EYE_HT_CP1[0],
                asKRSummaryLog.EYE_HT_CP1[1], asKRSummaryLog.EYE_HT_CP1[2], asKRSummaryLog.EYE_HT_CP1[3],
                asKRSummaryLog.EYE_HT_CP1[4], asKRSummaryLog.EYE_HT_CP1[5], asKRSummaryLog.EYE_HT_CP1[6],
                asKRSummaryLog.EYE_HT_CP1[7], asKRSummaryLog.EYE_HT_CP1[8], asKRSummaryLog.EYE_HT_CP1[9],
                asKRSummaryLog.EYE_HT_CP1[10], asKRSummaryLog.EYE_HT_CP1[11], asKRSummaryLog.EYE_HT_CP1[12],
                asKRSummaryLog.EYE_HT_CP1[13], asKRSummaryLog.EYE_HT_CP1[14], asKRSummaryLog.EYE_HT_CP1[15],
                asKRSummaryLog.EYE_HT_CP1[16], asKRSummaryLog.EYE_HT_CP1[17], asKRSummaryLog.EYE_HT_CP1[18],
                asKRSummaryLog.EYE_HT_CP1[19], asKRSummaryLog.EYE_HT_CP1[20], asKRSummaryLog.EYE_HT_CP1[21],
                asKRSummaryLog.EYE_HT_CP1[22], asKRSummaryLog.EYE_HT_CP1[23], asKRSummaryLog.EYE_HT_CP1[24],
                asKRSummaryLog.EYE_HT_CP1[25], asKRSummaryLog.EYE_HT_CP1[26], asKRSummaryLog.EYE_HT_CP1[27],
                asKRSummaryLog.EYE_HT_CP1[28], asKRSummaryLog.EYE_HT_CP1[29], asKRSummaryLog.EYE_HT_CP1[30],
                asKRSummaryLog.EYE_HT_CP1[31], asKRSummaryLog.EYE_HT_CP1[32], asKRSummaryLog.EYE_HT_CP1[33],
                asKRSummaryLog.EYE_HT_CP1[34], asKRSummaryLog.EYE_HT_CP1[35], asKRSummaryLog.EYE_HT_CP1[36],
                asKRSummaryLog.EYE_HT_CP1[37], asKRSummaryLog.EYE_HT_CP1[38], asKRSummaryLog.EYE_HT_CP1[39],
                asKRSummaryLog.EYE_HT_CP1[40], asKRSummaryLog.EYE_HT_CP1[41], asKRSummaryLog.EYE_HT_CP1[42],
                asKRSummaryLog.EYE_HT_CP1[43], asKRSummaryLog.EYE_HT_CP1[44], asKRSummaryLog.EYE_HT_CP1[45],
                asKRSummaryLog.EYE_HT_CP1[46], asKRSummaryLog.EYE_HT_CP1[47], asKRSummaryLog.EYE_HT_CP1[48],
                asKRSummaryLog.EYE_HT_CP1[49], asKRSummaryLog.EYE_HT_CP1[50], asKRSummaryLog.EYE_HT_CP1[51],
                asKRSummaryLog.EYE_HT_CP1[52], asKRSummaryLog.EYE_HT_CP1[53], asKRSummaryLog.EYE_HT_CP1[54],
                asKRSummaryLog.EYE_HT_CP1[55], asKRSummaryLog.EYE_HT_CP1[56], asKRSummaryLog.EYE_HT_CP1[57],
                asKRSummaryLog.EYE_HT_CP1[58], asKRSummaryLog.EYE_HT_CP1[59], asKRSummaryLog.EYE_HT_CP1[60],
                asKRSummaryLog.EYE_HT_CP1[61], asKRSummaryLog.EYE_HT_CP1[62], asKRSummaryLog.EYE_HT_CP1[63],
                asKRSummaryLog.LD_CM1, asKRSummaryLog.LD_C0, asKRSummaryLog.LD_CP, asKRSummaryLog.LD_VGA,
                asKRSummaryLog.LD_EDC, asKRSummaryLog.LD_EQR, asKRSummaryLog.FEC_CORRECTION,
                asKRSummaryLog.FEC_UNCORRECTION, asKRSummaryLog.CURR_EYE_HT, asKRSummaryLog.TRAINING_TIME_MS,
                ((asKRSummaryLog.wAneg_status & 0x80) == 0x80) ? "Yes" : (((asKRSummaryLog.wAneg_status & 0x80) != 0x80) ? "NO" : "NA"),
                HexToStr(bydatabuf, 4));
        cli_printf("%s\n", PortKRStatusbuffer);
        fprintf(file, "%s\n", PortKRStatusbuffer);
    }
    fclose(file);
}

static void cli_cmd_phy_kr_logging(cli_req_t *req)
{

    phy_kr_log_sel_t *mreq = req->module_req;

    mepa_rc rc = MEPA_RC_OK;
    mepa_port_no_t  port_no = 0;
    uint8_t u8KRportsToEnable, u8KRLogEnabledPorts = 0xFF, u8KRNOfports = 0;
    uint16_t u16DataLength = 0;
    int lineMemory[4] = {0, 0, 0, 0}, hostMemory[4] = {0, 0, 0, 0};


    // Step 1: Find Requested port
    for (int iport = 0; iport < meba_phy_kr_inst->phy_device_cnt - 1; iport++) {
        port_no = iport2uport(iport);

        if (req->port_list[port_no] == 0) {
            continue;
        }

        if (!meba_phy_kr_inst->phy_devices[iport]) {
            cli_printf(" Dev is Not Created for the port : %d\n", iport);
            return;
        }
        phy25g_phy_state_t *data = (phy25g_phy_state_t *) meba_phy_kr_inst->phy_devices[iport]->data;
        mepa_device_t *base_dev = (mepa_device_t *)data->base_dev;
        phy25g_phy_state_t *base_data = (phy25g_phy_state_t *)base_dev->data;

        if (base_data->krlog_en_ports != u8KRLogEnabledPorts) {
            lineMemory[0] = 0;
            lineMemory[1] = 0;
            lineMemory[2] = 0;
            lineMemory[3] = 0;
            hostMemory[0] = 0;
            hostMemory[1] = 0;
            hostMemory[2] = 0;
            hostMemory[3] = 0;

            u8KRLogEnabledPorts = base_data->krlog_en_ports;

            u8KRportsToEnable = u8KRLogEnabledPorts;

            while (u8KRportsToEnable != 0) {
                u8KRportsToEnable = u8KRportsToEnable & (u8KRportsToEnable - 1);
                u8KRNOfports++;
            }

            switch (u8KRNOfports) {
            case 0x04:
                u16DataLength = 0x2400; //9 * 1024
                break;
            case 0x03:
                u16DataLength = 0x3000; //12 * 1024
                break;
            case 0x02:
                u16DataLength = 0x4800; //18 * 1024
                break;
            case 0x01:
                u16DataLength = 0x9000; //36 * 1024
                break;
            default:
                break;
            }

            uint8_t byPartitionCount = 0;
            for (int byIndex = 0; byIndex < MAX_PORTS; byIndex++) {
                if (u8KRLogEnabledPorts & (1 << byIndex)) {
                    lineMemory[base_data->channel_id - byIndex] = KR_LOG_BASE_SUMMARY_PORT0 + (u16DataLength * byPartitionCount);
                    byPartitionCount += 1;
                }
            }

            for (int byIndex = 0; byIndex < MAX_PORTS; byIndex++) {
                if (u8KRLogEnabledPorts & (1 << (byIndex + 4))) {
                    hostMemory[base_data->channel_id - byIndex] = KR_LOG_BASE_SUMMARY_PORT0 + (u16DataLength * byPartitionCount);
                    byPartitionCount += 1;
                }
            }
        }

        // Check if logging is enabled for the line side of the port
        if (mreq->line_port == 1) {
            if (u8KRLogEnabledPorts & (1 << data->channel_id)) {
                if (mreq->clr) {
                    if ((rc = lan80xx_KRLog_Reset(meba_phy_kr_inst->phy_devices[iport], lineMemory[iport], u16DataLength )) != MEPA_RC_OK) {
                        T_E("Failed to Reset KR Log Memory Address: %x, length: %x\n", lineMemory[iport], u16DataLength);
                    } else {
                        cli_printf("Reset KR Log for Memory Address: %x, length: %x\n", lineMemory[iport], u16DataLength);
                    }

                } else {
                    processKrLogging(meba_phy_kr_inst->phy_devices[iport], mreq, iport, 0, lineMemory[iport], u16DataLength );
                }
            } else {
                T_E("Logging is not enabled for line side of port %d\n", iport);
            }
        }

        if (mreq->host_port == 1) {
            if (u8KRLogEnabledPorts & (1 << (data->channel_id + 4))) {
                if (mreq->clr) {
                    if ((rc = lan80xx_KRLog_Reset(meba_phy_kr_inst->phy_devices[iport], hostMemory[iport], u16DataLength )) != MEPA_RC_OK) {
                        T_E("Failed to Reset KR Log Memory Address: %x, length: %x\n", hostMemory[iport], u16DataLength);
                    } else {
                        cli_printf("Reset KR Log for Memory Address: %x, length: %x\n", hostMemory[iport], u16DataLength);
                    }
                } else {
                    processKrLogging(meba_phy_kr_inst->phy_devices[iport], mreq, iport, 1, hostMemory[iport], u16DataLength);
                }
            } else {
                T_E("Logging is not enabled for host side of port %d\n", iport);
            }
        }
    }
}

////////////////////////////////////////////////////////////
/////////////////   CLI Parameter functions ////////////////
////////////////////////////////////////////////////////////

static int cli_kr_parm_keyword(cli_req_t *req)
{
    const char     *found;
    phy_kr_cli_req_t *mreq = req->module_req;

    if ((found = cli_parse_find(req->cmd, req->stx)) == NULL) {
        return 1;
    }

    //cli_printf ("M25G KR ANEG cli param\n");
    if (!strncasecmp(found, "adv-1g", 6)) {
        mreq->adv1g = 1;
    } else if (!strncasecmp(found, "adv-10g", 7)) {
        mreq->adv10g = 1;
    } else if (!strncasecmp(found, "adv-25g", 7)) {
        mreq->adv25g_kr = 1;
    } else if (!strncasecmp(found, "adv-25g-krs", 11)) {
        mreq->adv25g_krs = 1;
    } else if (!strncasecmp(found, "rfec-10g", 8)) {
        mreq->rfec_10g = 1;
    } else if (!strncasecmp(found, "rfec-25g", 8)) {
        mreq->rfec_25g = 1;
    } else if (!strncasecmp(found, "rsfec", 5)) {
        mreq->rsfec_25g = 1;
    } else if (!strncasecmp(found, "np", 2)) {
        mreq->np = 1;
    } else if (!strncasecmp(found, "np-rfec", 7)) {
        mreq->np_rfec = 1;
    } else if (!strncasecmp(found, "np-rsfec", 8)) {
        mreq->np_rsfec = 1;
    } else if (!strncasecmp(found, "train", 5)) {
        mreq->train = 1;
    } else if (!strncasecmp(found, "disable", 7)) {
        mreq->dis = 1;
    } else if (!strncasecmp(found, "fw-res", 6)) {
        mreq->fw_res = 1;
    } else {
        cli_printf("no match: %s\n", found);
    }

    return 0;
}

static int cli_parm_sd_speed_idx (cli_req_t *req)
{
    phy_sd_cli_req_t *mreq = req->module_req;

    return cli_parm_u8(req, &mreq->speed_idx, 0, 3);
}

static int cli_parm_krlog_port_sel(cli_req_t *req)
{
    phy_kr_log_sel_t *mreq = req->module_req;

    if (!strncasecmp(req->cmd, "host", strlen(req->cmd))) {
        mreq->host_port = 1;
    } else if (!strncasecmp(req->cmd, "line", strlen(req->cmd))) {
        mreq->line_port = 1;
    } else {
        return 1;
    }
    return 0;
}

static int cli_parm_krlog_status_sel(cli_req_t *req)
{
    phy_kr_log_sel_t *mreq = req->module_req;

    mreq->eq = 0;
    mreq->ber = 0;
    mreq->irq = 0;
    mreq->all = 0;

    if (!strncasecmp(req->cmd, "eq", strlen(req->cmd))) {
        mreq->eq = 1;
    } else if (!strncasecmp(req->cmd, "ber", strlen(req->cmd))) {
        mreq->ber = 1;
    } else if (!strncasecmp(req->cmd, "irq", strlen(req->cmd))) {
        mreq->irq = 1;
    } else if (!strncasecmp(req->cmd, "all", strlen(req->cmd))) {
        mreq->all = 1;
    } else if (!strncasecmp(req->cmd, "clr", strlen(req->cmd))) {
        mreq->clr = 1;
    } else if (!strncasecmp(req->cmd, "aneg", strlen(req->cmd))) {
        mreq->aneg = 1;
    } else {
        return 1;
    }
    return 0;
}

static cli_cmd_t cli_cmd_table[] = {
    {
        "Phy KR aneg [<port_list>] [adv-1g] [adv-10g] [adv-25g] [adv-25g-krs] [rfec-10g] [rfec-25g] [rsfec] [np] [np-rfec] [np-rsfec] [fw-res] [train] [disable]",
        "Set or show kr",
        cli_cmd_phy_kr
    },
    {
        "Phy serdes get <speed_idx>",
        "Get phy serdes configuration",
        cli_cmd_phy_serdes_get
    },
    {
        "Phy serdes set <speed_idx>",
        "Set phy serdes configuration",
        cli_cmd_phy_serdes_set
    },
    {
        "Phy kr_log enable <port_list> [host] [line]",
        "Enable KR Log",
        cli_cmd_phy_kr_log_enable
    },
    {
        "Phy kr_log disable <port_list> [host] [line]",
        "Disable KR Log",
        cli_cmd_phy_kr_log_disable
    },
    {
        "Phy kr_log status <port_list> [host] [line] [aneg|eq|ber|irq|all|clr]",
        "Show KR Log",
        cli_cmd_phy_kr_logging
    },
};

static cli_parm_t cli_parm_table[] = {
    {
        "adv-1g",
        "adv-1g: advertise 1g",
        CLI_PARM_FLAG_NO_TXT | CLI_PARM_FLAG_SET,
        cli_kr_parm_keyword,
        cli_cmd_phy_kr
    },
    {
        "adv-10g",
        "adv-10g: advertise 10g",
        CLI_PARM_FLAG_NO_TXT | CLI_PARM_FLAG_SET,
        cli_kr_parm_keyword,
        cli_cmd_phy_kr
    },
    {
        "adv-25g",
        "adv-25g: advertise 25g kr",
        CLI_PARM_FLAG_NO_TXT | CLI_PARM_FLAG_SET,
        cli_kr_parm_keyword,
        cli_cmd_phy_kr
    },
    {
        "adv-25g-krs",
        "adv-25g-krs: advertise 25g krs",
        CLI_PARM_FLAG_NO_TXT | CLI_PARM_FLAG_SET,
        cli_kr_parm_keyword,
        cli_cmd_phy_kr
    },

    {
        "rfec-10g",
        "rfec: advertise 10g r-fec capability",
        CLI_PARM_FLAG_NO_TXT | CLI_PARM_FLAG_SET,
        cli_kr_parm_keyword,
        cli_cmd_phy_kr
    },
    {
        "rfec-25g",
        "rfec: advertise 25g r-fec capability",
        CLI_PARM_FLAG_NO_TXT | CLI_PARM_FLAG_SET,
        cli_kr_parm_keyword,
        cli_cmd_phy_kr
    },

    {
        "rsfec",
        "rs-fec: advertise 25g rs-fec capability",
        CLI_PARM_FLAG_NO_TXT | CLI_PARM_FLAG_SET,
        cli_kr_parm_keyword,
        cli_cmd_phy_kr
    },
    {
        "np",
        "np: use next page for advertise",
        CLI_PARM_FLAG_NO_TXT | CLI_PARM_FLAG_SET,
        cli_kr_parm_keyword,
        cli_cmd_phy_kr
    },
    {
        "np-rfec",
        "np r-fec: advertise next page r-fec capability",
        CLI_PARM_FLAG_NO_TXT | CLI_PARM_FLAG_SET,
        cli_kr_parm_keyword,
        cli_cmd_phy_kr
    },
    {
        "np-rsfec",
        "np rs-fec: advertise next page rs-fec capability",
        CLI_PARM_FLAG_NO_TXT | CLI_PARM_FLAG_SET,
        cli_kr_parm_keyword,
        cli_cmd_phy_kr
    },
    {
        "fw-res",
        "fw-res: enable firmware resolve",
        CLI_PARM_FLAG_NO_TXT | CLI_PARM_FLAG_SET,
        cli_kr_parm_keyword,
        cli_cmd_phy_kr
    },

    {
        "train",
        "train: enable training",
        CLI_PARM_FLAG_NO_TXT | CLI_PARM_FLAG_SET,
        cli_kr_parm_keyword,
        cli_cmd_phy_kr
    },
    {
        "disable",
        "disable: disable kr",
        CLI_PARM_FLAG_NO_TXT | CLI_PARM_FLAG_SET,
        cli_kr_parm_keyword,
        cli_cmd_phy_kr
    },
    {
        "<speed_idx>",
        "speed_idx: Speed index 0 - 1G, 1 - 10G, 2 - 25G",
        CLI_PARM_FLAG_NO_TXT | CLI_PARM_FLAG_SET,
        cli_parm_sd_speed_idx,
    },
    {
        "aneg|eq|ber|irq|all|clr",
        "aneg   : ANEG Log"
        "eq     : ANEG/KR Equalizers status Log\n"
        "ber    : ANEG/KR BER status Log\n"
        "irq    : ANEG/KR IRQ status Log\n"
        "all    : Over all ANEG/KR summary status\n"
        "clr    : Reset KR Logging region to zero\n",
        CLI_PARM_FLAG_NO_TXT | CLI_PARM_FLAG_SET,
        cli_parm_krlog_status_sel,
    },
    {
        "host",
        "line    : Line Port\n",
        CLI_PARM_FLAG_NO_TXT | CLI_PARM_FLAG_SET,
        cli_parm_krlog_port_sel,
    },
    {
        "line",
        "line    : Line Port\n",
        CLI_PARM_FLAG_NO_TXT | CLI_PARM_FLAG_SET,
        cli_parm_krlog_port_sel,
    }
};

////////////////////////////////////////////////////////////
/////////////// Core init functions ////////////////////////
////////////////////////////////////////////////////////////

static int phy_kr_cli_init(void)
{
    int i;

    /* Register commands */
    for (i = 0; i < sizeof(cli_cmd_table) / sizeof(cli_cmd_t); i++) {
        mscc_appl_cli_cmd_reg(&cli_cmd_table[i]);
    }
    /* Register parameters */
    for (i = 0; i < sizeof(cli_parm_table) / sizeof(cli_parm_t); i++) {
        mscc_appl_cli_parm_reg(&cli_parm_table[i]);
    }
    return MESA_RC_OK;
}

void mscc_appl_phy_kr_init(mscc_appl_init_t *init)
{
    meba_phy_kr_inst = init->board_inst;
    switch (init->cmd) {
    case MSCC_INIT_CMD_REG:
        mscc_appl_trace_register(&trace_module, trace_groups, TRACE_GROUP_CNT);
        break;

    case MSCC_INIT_CMD_INIT:
        if (phy_kr_cli_init() != MEPA_RC_OK) {
            T_E("Couldn't load phy kr aneg config module");
        }
        break;
    default:
        break;
    }
}
