// Copyright (c) 2004-2020 Microchip Technology Inc. and its subsidiaries.
// SPDX-License-Identifier: MIT


#include <stdio.h>
#include <ctype.h>
#include "microchip/ethernet/switch/api.h"
#include "microchip/ethernet/board/api.h"
#include "microchip/ethernet/phy/api/types.h"

#include "vtss_phy_10g_api.h"
#include "vtss_private.h"
#include "lan80xx.h"

#include "main.h"
#include "mesa-rpc.h"
#include "trace.h"
#include "cli.h"
#include "port.h"
#include "mepa_driver.h"
#include "phy_demo_apps.h"

meba_inst_t meba_phy_instances;

char src_input[32];
char dst_input[32];
char squelch_input[32];
float freq_input;
int enable_input = 0;

#define ARRAY_LEN(x) (sizeof(x) / sizeof((x)[0]))

static void cli_cmd_synce_configure(cli_req_t *req);

static mscc_appl_trace_module_t trace_module = {
    .name = "phy_synce_config"
};

enum {
    TRACE_GROUP_DEFAULT,
    TRACE_GROUP_CNT
};

static mscc_appl_trace_group_t trace_groups[10] = {
    // TRACE_GROUP_DEFAULT
    {
        .name = "default",
        .level = MESA_TRACE_LEVEL_ERROR
    },
};


static int cli_param_synce_ena_dis(cli_req_t *req)
{
    if (!strncasecmp(req->cmd, "enable", strlen("enable"))) {
        enable_input = 1;
    } else {
        enable_input = 0;
    }
    return 0;
}

static cli_cmd_t cli_cmd_table[] = {
    {
        "phy synce_conf <port_no> <enable|disable>",
        "syncE configuration for PHY",
        cli_cmd_synce_configure,
    },
};

static cli_parm_t cli_parm_table[] = {
    {
        "<enable|disable>",
        "\n Enable or Disable Synce Clock Output\n",
        CLI_PARM_FLAG_SET,
        cli_param_synce_ena_dis,
        cli_cmd_synce_configure,
    },
};

static const char * const src_options_malibu[] =
    {"line0","line1","line2","line3","host0","host1","host2","host3","srefclk"};

static const char * const src_options_viper[] =
    {"copper","serdes"};

static const char * const src_options_lan8814[] =
    {"copper","serdes","clock_in_1","clock_in_2"};

static const char * const dst_malibu[] =
    {"sckout","ckout0","ckout1","ckout2","ckout3"};

static const char * const dst_viper[] =
    {"rcvrclk1","rcvrclk2"};

static const char * const dst_lan8814[] =
    {"rcvrclk1","rcvrclk2"};

static const char * const lan80xx_dst[] =
    {"rcvrclk1","rcvrclk2"};

static const float freq_malibu[] =
    {62.5,125,156.25,161.13,322.27};

static const float freq_lan80xx[] =
    {75.0f, 37.50f, 79.58f, 39.79f, 125.0f, 62.50f, 31.25f, 15.62f, 128.90f, 64.45f, 32.22f, 80.56f};

static const float freq_viper[] =
    {25,31.25,125};

static const float freq_lan8814[] =
    {25,31.25,125};

static const char *sq_opt[] =
    {"none","link_line0","link_line1","link_line2","link_line3"};

mepa_synce_clock_src_t str_to_src_enum(const char *src) {
    if (strcmp(src, "line0") == 0) return MEPA_SYNCE_CLOCK_SRC_LINE0;
    if (strcmp(src, "line1") == 0) return MEPA_SYNCE_CLOCK_SRC_LINE1;
    if (strcmp(src, "line2") == 0) return MEPA_SYNCE_CLOCK_SRC_LINE2;
    if (strcmp(src, "line3") == 0) return MEPA_SYNCE_CLOCK_SRC_LINE3;
    if (strcmp(src, "host0") == 0) return MEPA_SYNCE_CLOCK_SRC_HOST0;
    if (strcmp(src, "host1") == 0) return MEPA_SYNCE_CLOCK_SRC_HOST1;
    if (strcmp(src, "host2") == 0) return MEPA_SYNCE_CLOCK_SRC_HOST2;
    if (strcmp(src, "host3") == 0) return MEPA_SYNCE_CLOCK_SRC_HOST3;
    if (strcmp(src, "srefclk") == 0) return MEPA_SYNCE_CLOCK_SRC_SREFCLK;
    if (strcmp(src, "copper") == 0) return MEPA_SYNCE_CLOCK_SRC_COPPER_MEDIA;
    if (strcmp(src, "serdes") == 0) return MEPA_SYNCE_CLOCK_SRC_SERDES_MEDIA;
    if (strcmp(src, "clock_in_1") == 0) return MEPA_SYNCE_CLOCK_SRC_CLOCK_IN_1;
    if (strcmp(src, "clock_in_2") == 0) return MEPA_SYNCE_CLOCK_SRC_CLOCK_IN_2;
    if (strcmp(src, "disabled") == 0) return MEPA_SYNCE_CLOCK_SRC_DISABLED;
    // Add more as needed
    return MEPA_SYNCE_CLOCK_SRC_DISABLED; // Default/fallback
}

mepa_synce_clock_dst_t str_to_dst_enum(const char *dst) {
    if (strcmp(dst, "sckout") == 0) return MEPA_SYNCE_CLOCK_DST_SCKOUT;
    if (strcmp(dst, "ckout0") == 0) return MEPA_SYNCE_CLOCK_DST_1;
    if (strcmp(dst, "ckout1") == 0) return MEPA_SYNCE_CLOCK_DST_2;
    if (strcmp(dst, "ckout2") == 0) return MEPA_SYNCE_CLOCK_DST_3;
    if (strcmp(dst, "ckout3") == 0) return MEPA_SYNCE_CLOCK_DST_4;
    if (strcmp(dst, "rcvrclk1") == 0) return MEPA_SYNCE_CLOCK_DST_1;
    if (strcmp(dst, "rcvrclk2") == 0) return MEPA_SYNCE_CLOCK_DST_2;
    // Add more as needed
    return MEPA_SYNCE_CLOCK_DST_NONE; // Default/fallback
}

mepa_freq_t float_to_freq_enum(float freq) {
    if (freq == 25.0f)         return MEPA_FREQ_25M;
    if (freq == 31.25f)        return MEPA_FREQ_31_25M;
    if (freq == 62.5f)         return MEPA_FREQ_62_5M;
    if (freq == 125.0f)        return MEPA_FREQ_125M;
    if (freq == 155.52f)       return MEPA_FREQ_155_52M;
    if (freq == 156.25f)       return MEPA_FREQ_156_25M;
    if (freq == 161.13f)       return MEPA_FREQ_161_13M;
    if (freq == 311.04f)       return MEPA_FREQ_311_04M;
    if (freq == 322.27f)       return MEPA_FREQ_322_27M;
    if (freq == 75.0f)         return MEPA_FREQ_75M;
    if (freq == 37.50f)        return MEPA_FREQ_37_50M;
    if (freq == 79.58f)        return MEPA_FREQ_79_58M;
    if (freq == 39.79f)        return MEPA_FREQ_39_79M;
    if (freq == 15.62f)        return MEPA_FREQ_15_62M;
    if (freq == 128.90f)       return MEPA_FREQ_128_90M;
    if (freq == 64.45f)        return MEPA_FREQ_64_45M;
    if (freq == 32.22f)        return MEPA_FREQ_32_22M;
    if (freq == 80.56f)        return MEPA_FREQ_80_56M;
    return MEPA_FREQ_125M; // Default/fallback
}


mepa_squelch_src_t str_to_squelch_enum(const char *squelch) {
    if (strcmp(squelch, "none") == 0) return MEPA_SYNCE_NO_SQUELCH;
    if (strcmp(squelch, "link_line0") == 0) return MEPA_SYNCE_SQUELCH_LINK_LINE0;
    if (strcmp(squelch, "link_line1") == 0) return MEPA_SYNCE_SQUELCH_LINK_LINE1;
    if (strcmp(squelch, "link_line2") == 0) return MEPA_SYNCE_SQUELCH_LINK_LINE2;
    if (strcmp(squelch, "link_line3") == 0) return MEPA_SYNCE_SQUELCH_LINK_LINE3;
    // Add more as needed
    return MEPA_SYNCE_NO_SQUELCH; // Default/fallback
}

static int mepa_dev_check(meba_inst_t meba_instance, mepa_port_no_t port_no)
{
    if (!meba_instance->phy_devices[port_no]) {
        return MEPA_RC_ERROR;
    }
    return MEPA_RC_OK;
}

static inline void read_input(char *buf)
{
    scanf("%31s", buf);
}

static int menu_select_string(const char *title, const char * const options[], int count)
{
    char input[32];
    int sel;

    cli_printf("\n%s\n", title);

    for (int i = 0; i < count; i++)
        cli_printf("  %d) %s\n", i + 1, options[i]);

    cli_printf("Enter choice [1-%d]: ", count);
    read_input(input);
    sel = atoi(input);

    if (sel < 1 || sel > count)
        return -1;

    return (sel - 1);
}

static int menu_select_float(const char *title, const float options[], int count)
{
    char input[32];
    int sel;

    cli_printf("\n%s\n", title);

    for (int i = 0; i < count; i++)
        cli_printf("  %d) %.2f\n", i + 1, options[i]);

    cli_printf("\nEnter choice [1-%d]: ", count);
    read_input(input);
    sel = atoi(input);

    if (sel < 1 || sel > count)
        return -1;

    return (sel - 1);
}

static void cli_cmd_synce_configure(cli_req_t *req)
{
    mepa_rc rc;
    demo_phy_info_t phy_type;
    mepa_synce_clock_conf_t conf = {0};
    struct mepa_device *dev;

    // --- Device check ---
    if (mepa_dev_check(meba_phy_instances, req->port_no) != MEPA_RC_OK) {
        T_E("Dev not created for port %d\n", req->port_no + 1);
        return;
    }

    if (phy_family_detect(meba_phy_instances, req->port_no, &phy_type) != MEPA_RC_OK) {
        T_E("PHY family detect failed on port %d\n", req->port_no + 1);
        return;
    }

    T_D("PHY family detected on port : %d is %s\n", (req->port_no + 1), phy_type.family_name);
    dev = meba_phy_instances->phy_devices[req->port_no];

    const char * const *src_options;
    const char * const *dst_options;
    const float *freq_options = NULL;

    int src_count = 0, dst_count = 0, freq_count = 0;

    // Select table based on family
    switch (phy_type.family) {
    case PHY_FAMILY_MALIBU_10G:
        src_options  = src_options_malibu;
        dst_options  = dst_malibu;
        freq_options = freq_malibu;

        src_count  = ARRAY_LEN(src_options_malibu);
        dst_count  = ARRAY_LEN(dst_malibu);
        freq_count = ARRAY_LEN(freq_malibu);
        break;

    case PHY_FAMILY_MALIBU_25G:
        src_options  = src_options_malibu;
        dst_options  = lan80xx_dst;
        freq_options = freq_lan80xx;

        src_count  = ARRAY_LEN(src_options_malibu);
        dst_count  = ARRAY_LEN(lan80xx_dst);
        freq_count = ARRAY_LEN(freq_lan80xx);
        break;

    case PHY_FAMILY_VIPER:
        src_options  = src_options_viper;
        dst_options  = dst_viper;
        freq_options = freq_viper;

        src_count  = ARRAY_LEN(src_options_viper);
        dst_count  = ARRAY_LEN(dst_viper);
        freq_count = ARRAY_LEN(freq_viper);
        break;

    case PHY_FAMILY_LAN8814:
        src_options  = src_options_lan8814;
        dst_options  = dst_lan8814;
        freq_options = freq_lan8814;

        src_count  = ARRAY_LEN(src_options_lan8814);
        dst_count  = ARRAY_LEN(dst_lan8814);
        freq_count = ARRAY_LEN(freq_lan8814);
        break;
    default:
        T_E("\n SyncE not supported on the PHY \n");
        return;
    }

    if (enable_input == 0) {
        int dst_sel = menu_select_string("Select output destination:", dst_options, dst_count);
        if (dst_sel < 0) { T_E("\nInvalid selection\n"); return; }
        conf.src = MEPA_SYNCE_CLOCK_SRC_DISABLED;
        conf.dst    = str_to_dst_enum(dst_options[dst_sel]);

        goto synce_disable;
    }

    // --- Source selection ------------------------------------------------
    int src_sel = menu_select_string("Select recovered clock source:", src_options, src_count);
    if (src_sel < 0) { T_E("\nInvalid selection\n"); return; }

    // --- Destination selection ------------------------------------------
    int dst_sel = menu_select_string("Select output destination:", dst_options, dst_count);
    if (dst_sel < 0) { T_E("\nInvalid selection\n"); return; }

    if (phy_type.family == PHY_FAMILY_MALIBU_10G) {
        if (strcmp(dst_options[dst_sel], "sckout") == 0) {
            static const float sckout_freqs[] = {125, 156.25};
            freq_options = sckout_freqs;
            freq_count = ARRAY_LEN(sckout_freqs);
        } else if (strncmp(dst_options[dst_sel], "ckout", 5) == 0) {

            mepa_conf_t conf_get = {0};

            if ((rc = mepa_conf_get(dev, &conf_get)) != MEPA_RC_OK) {
                T_E("\n Error Getting Conf_get on port : %d \n", (req->port_no + 1));
                return;
            }
            if (conf_get.conf_10g.oper_mode == MEPA_PHY_1G_MODE) {
                static const float ckout_1g_freqs[] = {125, 62.5};
                freq_options = ckout_1g_freqs;
                freq_count = ARRAY_LEN(ckout_1g_freqs);
            } else if (conf_get.conf_10g.oper_mode == MEPA_PHY_LAN_MODE) {
                static const float ckout_10g_lan_freqs[] = {322.27, 161.13};
                freq_options = ckout_10g_lan_freqs;
                freq_count = ARRAY_LEN(ckout_10g_lan_freqs);
            } else if (conf_get.conf_10g.oper_mode == MEPA_PHY_WAN_MODE) {
                static const float ckout_10g_wan_freqs[] = {311.04, 155.52};
                freq_options = ckout_10g_wan_freqs;
                freq_count = ARRAY_LEN(ckout_10g_wan_freqs);
            }
        }
    }

    // --- Frequency selection --------------------------------------------
    int freq_sel = menu_select_float("Select output frequency (MHz):", freq_options, freq_count);
    if (freq_sel < 0) { T_E("\nInvalid selection\n"); return; }

    // --- Squelch selection ----------------------------------------------
    const char *squelch_str = "none";

    if (phy_type.family == PHY_FAMILY_MALIBU_10G || phy_type.family == PHY_FAMILY_MALIBU_25G) {

        int sq_sel = menu_select_string("Select squelch source:", sq_opt, ARRAY_LEN(sq_opt));
        if (sq_sel < 0) { T_E("Invalid squelch source\n"); return; }

        squelch_str = sq_opt[sq_sel];
    }

    // --- Mapping to enums -----------------------------------------------
    conf.src    = str_to_src_enum(src_options[src_sel]);
    conf.dst    = str_to_dst_enum(dst_options[dst_sel]);
    conf.freq   = float_to_freq_enum(freq_options[freq_sel]);
    conf.squelch.squelch_src = str_to_squelch_enum(squelch_str);
    if (conf.squelch.squelch_src == MEPA_SYNCE_NO_SQUELCH) {
        conf.squelch.squelch_inv = FALSE;
    } else {
        conf.squelch.squelch_inv = TRUE;
    }

synce_disable:
    T_D("src : %d, dst : %d, freq : %d, squelch_src : %d, squelch_inv : %d", conf.src, conf.dst, conf.freq, conf.squelch.squelch_src, conf.squelch.squelch_inv);
    // --- Apply -----------------------------------------------------------
    rc = mepa_synce_clock_conf_set(dev, &conf);

    if (rc == MEPA_RC_OK) {
        cli_printf("\nSyncE configuration applied successfully!\n");
    } else {
        T_E("\nError applying SyncE configuration \n");
    }
}


static void phy_cli_init(void)
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

}

void mscc_appl_phy_synce(mscc_appl_init_t *init)
{
    meba_phy_instances = init->board_inst;
    switch (init->cmd) {
    case MSCC_INIT_CMD_REG:
        mscc_appl_trace_register(&trace_module, trace_groups, TRACE_GROUP_CNT);
        break;

    case MSCC_INIT_CMD_INIT:
        phy_cli_init();
        break;
    default:
        break;
    }
}
