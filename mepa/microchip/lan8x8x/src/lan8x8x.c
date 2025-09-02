// Copyright (c) 2004-2024 Microchip Technology Inc. and its subsidiaries.
// SPDX-License-Identifier: MIT

#if defined(MEPA_HAS_LAN8X8X)
#include <microchip/ethernet/phy/api.h>
#include <mepa_driver.h>
#include <string.h>

#include "lan8x8x_private.h"

#ifdef MEPA_lan8x8x_static_mem
static mepa_device_t lan8x8x_device[LAN8X8X_PHY_MAX];
static phy_data_t lan8x8x_data[LAN8X8X_PHY_MAX];
#endif

static mepa_rc lan8x8x_phy_init(mepa_device_t *const dev);
/**********************************
 * Internal APIs
 *********************************/
//Retrieve PHY information
static mepa_rc phy_get_device_info(mepa_device_t *const dev)
{
    phy_data_t *const data = (phy_data_t *const)dev->data;
    uint32_t id1 = 0, id2 = 0;
    uint16_t val = 0;

    //Init structure
    data->dev.id = ZERO;
    data->dev.model = ZERO;
    data->dev.rev = ZERO;
    data->dev.is_master = PHY_FALSE;
    data->dev.is_master_fault = PHY_FALSE;

    (void) phy_reg_rd(dev, MII_PHYSID1, &val);
    id1 = val;

    val = 0;
    (void) phy_reg_rd(dev, MII_PHYSID2, &val);
    id2 = val;

    //Setup structure
    data->dev.rev   = (uint16_t)GET_PHY_REV(id2);
    data->dev.model = (uint16_t)GET_PHY_MODEL(id2);
    /**
     *  PHY_ID_NUM0: Assigned to the 3rd through 18th bits of the Organizationally Unique Identifier (OUI), respectively.
     */
    data->dev.id = GET_PHY_ID1(id1);
    /**
     *  PHY_ID_NUM1: Assigned to the 19th through 24th bits of the OUI.
     */
    data->dev.id |= GET_PHY_ID2(id2);

    return MEPA_RC_OK;
}

static mepa_rc phy_c45_get_link_status(mepa_device_t *const dev, mepa_status_t *const status)
{
    mepa_rc rc;
    uint16_t reg_val = 0;
    phy_data_t *const data = (phy_data_t *const)dev->data;

    //LATCH-LOW BIT
    rc = phy_mmd_reg_rd(dev, MDIO_MMD_PMAPMD, MDIO_STAT1, &reg_val);
    if (rc == MEPA_RC_OK) {
        rc = phy_mmd_reg_rd(dev, MDIO_MMD_PMAPMD, MDIO_STAT1, &reg_val);
        if (rc == MEPA_RC_OK) {
            status->link = ((reg_val & MDIO_STAT1_LSTATUS) != ZERO);
            data->link_status = status->link;
        }
    }

    T_D(  "Link Status is %s!!\n", (data->link_status ? "UP" : "DOWN"));

    return rc;
}

static mepa_rc lan8x8x_rgmii_setup(mepa_device_t *const dev, mepa_port_interface_t mac_if)
{
    mepa_bool_t updated = PHY_TRUE;
    mepa_rc rc = MEPA_RC_OK;
    uint16_t txc = 0;
    uint16_t rxc = 0;

    /* FIX_ME: Any QSGMII disable register for both 100M and 1G speed ? */

    MEPA_RC_GOTO(rc, phy_mmd_reg_rd(dev, MDIO_MMD_VEND1, LAN8X8X_RGMII_TX_DLL_CFG, &txc));

    MEPA_RC_GOTO(rc, phy_mmd_reg_rd(dev, MDIO_MMD_VEND1, LAN8X8X_RGMII_RX_DLL_CFG, &rxc));

    switch (mac_if) {
    case MESA_PORT_INTERFACE_RGMII:
        txc &= ((~LAN8X8X_RGMII_DELAY_EN) & 0xFFFFU);
        rxc &= ((~LAN8X8X_RGMII_DELAY_EN) & 0xFFFFU);
        break;
    case MESA_PORT_INTERFACE_RGMII_ID:
        txc |= LAN8X8X_RGMII_DELAY_EN;
        rxc |= LAN8X8X_RGMII_DELAY_EN;
        break;
    case MESA_PORT_INTERFACE_RGMII_RXID:
        txc &= ((~LAN8X8X_RGMII_DELAY_EN) & 0xFFFFU);
        rxc |= LAN8X8X_RGMII_DELAY_EN;
        break;
    case MESA_PORT_INTERFACE_RGMII_TXID:
        txc |= LAN8X8X_RGMII_DELAY_EN;
        rxc &= ((~LAN8X8X_RGMII_DELAY_EN) & 0xFFFFU);
        break;
    default:
        T_I( MEPA_TRACE_GRP_GEN, "PHY MAC interface is not RGMII(%d)!\n", mac_if);
        updated = PHY_FALSE;
        break;
    }

    if (updated == PHY_TRUE) {
        // Set RX DELAY
        MEPA_RC_GOTO(rc, phy_mmd_reg_modify(dev, MDIO_MMD_VEND1, LAN8X8X_RGMII_RX_DLL_CFG,
                                            LAN8X8X_RGMII_DLL_CONF, rxc));

        // Set TX DELAY
        MEPA_RC_GOTO(rc, phy_mmd_reg_modify(dev, MDIO_MMD_VEND1, LAN8X8X_RGMII_TX_DLL_CFG,
                                            LAN8X8X_RGMII_DLL_CONF, txc));

        T_I( MEPA_TRACE_GRP_GEN, "PHY RGMII setup complete!\n");
    }

    return MEPA_RC_OK;
}

static mepa_rc lan8x8x_config_mac(mepa_device_t *dev)
{
    const phy_data_t *const data = (const phy_data_t *const)dev->data;
    mepa_rc rc = MEPA_RC_ERROR;

    if ((data->mac_if >= MESA_PORT_INTERFACE_RGMII) &&
        (data->mac_if <= MESA_PORT_INTERFACE_RGMII_TXID)) {
        MEPA_RC_GOTO(rc, lan8x8x_rgmii_setup(dev, data->mac_if));
    } else if (data->mac_if == MESA_PORT_INTERFACE_SGMII) {
        //FIXME: setup qsgmii
        rc = MEPA_RC_OK;
    } else {
        //misra_c_2023_rule_15_7_violation
    }

    return rc;
}

static mepa_rc lan8x8x_check_media(const mepa_device_t *const dev,
                                   mepa_media_interface_t media_if,
                                   mesa_port_speed_t speed)
{
    mepa_rc rc = MEPA_RC_ERR_KR_CONF_NOT_SUPPORTED;

    if ((media_if == MESA_PHY_MEDIA_IF_T1_100FX) ||
        ((media_if == MESA_PHY_MEDIA_IF_T1_1000FX) &&
         ((dev->drv->id & dev->drv->mask) != PHY_ID_LAN8X8X_A))) {
        const phy_data_t *const data = (const phy_data_t *const)dev->data;

        rc = MEPA_RC_OK;
        // speed selection based on media type
        if (media_if == MESA_PHY_MEDIA_IF_T1_100FX) {
            if (((speed == MESA_SPEED_AUTO) &&
                 (data->conf.aneg.speed_100m_fdx != PHY_TRUE)) ||
                ((speed != MESA_SPEED_AUTO) &&
                 (speed != MESA_SPEED_100M))) {
                rc = MEPA_RC_ERR_PARM;
            }
        }
    }

    T_I(  "phy_id=0x%x: media=%d, speed=%d, rc=%d\n",
          dev->drv->id, media_if, speed, rc);
    return rc;
}

static mepa_rc lan8x8x_phy_reset(mepa_device_t *dev, mepa_bool_t hard_reset)
{
    mepa_rc rc = MEPA_RC_OK;
    mepa_bool_t done = PHY_FALSE;
    mepa_bool_t timeout = PHY_FALSE;
    mepa_mtimer_t   timer = { 0 };

    if (hard_reset) {
        return MEPA_RC_NOT_IMPLEMENTED;
    } else {
        // CL22 soft-reset to let the link re-train
        MEPA_RC_GOTO(rc, phy_reg_set_bits(dev, MII_BMCR, BMCR_RESET));

        LAN8X8X_MTIMER_START(&timer, 4000U);

        // disable aneg
        MEPA_RC_GOTO(rc, phy_mmd_reg_clear_bits(dev, MDIO_MMD_AN, MDIO_AN_T1_CTRL,
                                                MDIO_AN_CTRL1_ENABLE));

        // wait for reset to complete
        while ((done == PHY_FALSE) && (timeout == PHY_FALSE)) {
            uint16_t tmp = 0;

            timeout = MEPA_MTIMER_TIMEOUT(&timer);
            (void) phy_reg_rd(dev, MII_BMCR, &tmp);
            if (!((tmp & BMCR_RESET) == BMCR_RESET)) {
                done = PHY_TRUE;
            }
        }
        if (done == PHY_FALSE) {
            T_E("PHY soft-reset timedout! \r\n");
        }
    }

    T_D(  "PHY %s reset done! \r\n", (hard_reset ? "hard" : "soft"));

    return MEPA_RC_OK;
}

static mepa_rc lan8x8x_phy_setup(mepa_device_t *const dev)
{
    mepa_rc rc = MEPA_RC_OK;
    phy_data_t *const data = (phy_data_t *const)dev->data;

    MEPA_RC_GOTO(rc, phy_get_device_info(dev));

    if (!data->init_done) {
        //FIXME: onetime setup
    }
    data->init_done = PHY_TRUE;

    MEPA_RC_GOTO(rc, lan8x8x_phy_init(dev));

    T_D(  "PHY port=%u setup complete!\n", data->port_no);

    return MEPA_RC_OK;
}

static int lan8x8x_pma_baset1_setup_forced(mepa_device_t *const dev)
{
    phy_data_t *data = (phy_data_t *)dev->data;
    mepa_rc rc = MEPA_RC_OK;
    uint16_t reg_val = 0;

    /* mode configuration */
    if (data->conf.man_neg == MEPA_MANUAL_NEG_REF) {
        reg_val |= MDIO_PMA_PMD_BT1_CTRL_CFG_MST;
    }

    /* speed configuration */
    if ((data->media_intf != MESA_PHY_MEDIA_IF_T1_100FX) &&
        (data->conf.speed == MESA_SPEED_1G)) {
        reg_val |= LAN8X8X_PMA_COMM_100T1_CTL_T1_TYPE_1000;
    }

    MEPA_RC_GOTO(rc, phy_mmd_reg_wr(dev, MDIO_MMD_PMAPMD, MDIO_PMA_PMD_BT1_CTRL, reg_val));

    return MEPA_RC_OK;
}

static mepa_rc lan8x8x_init_conf(mepa_device_t *const dev)
{
    phy_data_t *data = (phy_data_t *)dev->data;
    mepa_manual_neg_t mode_ms = data->conf.man_neg;
    mepa_rc rc = MEPA_RC_OK;

    if (data->conf.admin.enable == PHY_TRUE) {

        //clear power down bit
        MEPA_RC_GOTO(rc, phy_reg_clear_bits(dev, MII_BMCR, BMCR_PDOWN));

        MEPA_RC_GOTO(rc, lan8x8x_pma_baset1_setup_forced(dev));

        T_I( MEPA_TRACE_GRP_GEN, "PHY port-%u init_conf aneg %sabled, mode %s-%s speed %sM!\n",
             data->port_no, ((data->conf.speed == MESA_SPEED_AUTO) ? "en" : "dis"),
             ((data->conf.speed == MESA_SPEED_AUTO) ? "preferred" : "forced"),
             ((mode_ms == MEPA_MANUAL_NEG_REF) ? "master" : "slave"),
             ((data->conf.speed == MESA_SPEED_100M) ? "100" :
              ((data->conf.speed == MESA_SPEED_1G) ? "1000" :
               ((data->conf.speed == MESA_SPEED_AUTO) ? "auto" : "unknown"))));

    }

    //T_D( MEPA_TRACE_GRP_GEN, "PHY port-%u configuration complete!\n", data->port_no);

    return MEPA_RC_OK;
}

static mepa_rc lan8x8x_int_reset(mepa_device_t *dev, const lan8x8x_reset_typ rst_typ)
{
    mepa_rc rc = MEPA_RC_OK;
    phy_data_t *const data = (phy_data_t *const)dev->data;
    T_I(  "PHY reset & initialize! \r\n");

    //reset PHY
    if ((rst_typ == LAN8X8X_RST_HARD) || (rst_typ == LAN8X8X_RST_HARD_ONLY)) { //hard-reset
        MEPA_RC_GOTO(rc, lan8x8x_phy_reset(dev, PHY_TRUE));

        if (rst_typ == LAN8X8X_RST_HARD_ONLY) {
            return MEPA_RC_OK;
        }
        data->init_done = PHY_FALSE;
        T_I(  "PHY re-configured after reset!\n");
    } else {
        if (rst_typ != LAN8X8X_RST_SKIP_TO_CONF) { //soft-reset
            MEPA_RC_GOTO(rc, lan8x8x_phy_reset(dev, PHY_FALSE));
        }
    }

    if ((rst_typ == LAN8X8X_RST_SOFT_EXT) ||
        (rst_typ == LAN8X8X_RST_HARD)) {
        MEPA_RC_GOTO(rc, lan8x8x_phy_setup(dev));
    }

    //configure after reset
    if ((rst_typ == LAN8X8X_RST_SOFT_MAC) ||
        (rst_typ == LAN8X8X_RST_SKIP_TO_CONF)) {
        MEPA_RC_GOTO(rc, lan8x8x_config_mac(dev));
    }

    //For the first time init_conf will take care of all phy setup
    MEPA_RC_GOTO(rc, lan8x8x_init_conf(dev));

    MEPA_RC_GOTO(rc, phy_get_device_info(dev));

    return rc;
}

static mepa_rc lan8x8x_config_set(mepa_device_t *dev, const mepa_conf_t *config)
{
    mepa_rc rc = MEPA_RC_OK;
    phy_data_t *const data = (phy_data_t *const)dev->data;
    mepa_bool_t re_config = PHY_FALSE;
    lan8x8x_reset_typ type = LAN8X8X_RST_SOFT;

    if (((config->man_neg != MEPA_MANUAL_NEG_CLIENT) &&
         (config->man_neg != MEPA_MANUAL_NEG_REF)) ||
        (config->fdx != PHY_TRUE) ||
        ((config->speed != MESA_SPEED_100M) &&
         (config->speed != MESA_SPEED_1G) &&
         (config->speed != MESA_SPEED_AUTO))) {
        rc = MEPA_RC_ERR_PARM;
    } else {
        //AUTO is always 1G
        mesa_port_speed_t speed = config->speed;

        data->conf.fdx = PHY_TRUE;
        data->conf.flow_control = config->flow_control;
        data->conf.aneg.speed_100m_fdx = config->aneg.speed_100m_fdx;
        data->conf.aneg.speed_1g_fdx = config->aneg.speed_1g_fdx;

        // Setup MAC ANEG
        if (data->conf.mac_if_aneg_ena != config->mac_if_aneg_ena) {
            re_config = PHY_TRUE;
            data->conf.mac_if_aneg_ena = config->mac_if_aneg_ena;
            type = LAN8X8X_RST_SOFT_MAC;
        }

        // Setup Speed
        if (data->conf.speed != speed) {
            //check media/speed/aneg capabilities
            MEPA_RC_GOTO(rc, lan8x8x_check_media(dev, data->media_intf, speed));
            re_config = PHY_TRUE;
            data->conf.speed = speed;
        }

        // Setup Master/Slave Mode
        if (data->conf.man_neg != config->man_neg) {
            re_config = PHY_TRUE;
            data->conf.man_neg = config->man_neg;
        }

        // handle admin enable/disable
        if (data->conf.admin.enable != config->admin.enable) {
            re_config = PHY_TRUE;
            data->conf.admin.enable = config->admin.enable;
        }

        if (re_config == PHY_TRUE) {
            //config change and admin enable
            if (data->conf.admin.enable == PHY_TRUE) {
                MEPA_RC_GOTO(rc, lan8x8x_int_reset(dev, type));
            } else {
                //Power down
                MEPA_RC_GOTO(rc, phy_mmd_reg_set_bits(dev, MDIO_MMD_PMAPMD,
                                                      MDIO_CTRL1, BMCR_PDOWN));
            }
        }

        rc = MEPA_RC_OK;
    }

    return rc;
}

static void lan8x8x_fill_probe_data(mepa_driver_t *drv,
                                    mepa_device_t *dev,
                                    phy_data_t *data,
                                    const mepa_callout_t    MEPA_SHARED_PTR *callout,
                                    struct mepa_callout_ctx MEPA_SHARED_PTR *callout_ctx,
                                    const struct mepa_board_conf *const conf)
{
    data->ctx_status = PHY_TRUE;
    data->port_no = conf->numeric_handle;

    dev->numeric_handle  = conf->numeric_handle;
    dev->drv = drv;
    dev->data = (void *)data;
    dev->callout = callout;
    dev->callout_ctx = callout_ctx;
    data->init_done = PHY_FALSE;
    data->events = 0;
    //Default is preferred master when autoneg is enabled and forced master when aneg is disabled
    data->conf.man_neg = MEPA_MANUAL_NEG_REF;
    data->conf.admin.enable = PHY_TRUE;
    data->conf.fdx = PHY_TRUE;
    //mac-if aneg must be enabled always
    data->conf.mac_if_aneg_ena = PHY_TRUE;
    //phy aneg
    data->conf.speed = MESA_SPEED_1G;
    data->conf.aneg.speed_100m_fdx = PHY_FALSE;
    data->conf.aneg.speed_1g_fdx = PHY_FALSE;
    data->media_intf = MESA_PHY_MEDIA_IF_T1_100FX;

    if ((dev->drv->id & dev->drv->mask) != PHY_ID_LAN8X8X_A) {
        data->media_intf = MESA_PHY_MEDIA_IF_T1_1000FX;
        data->conf.aneg.speed_1g_fdx = PHY_TRUE;
        T_I(  "LAN8X8X_A phy_id=0x%x\n", dev->drv->id);
    }
    data->mac_if = MESA_PORT_INTERFACE_RGMII_RXID;

    data->led_conf[MEPA_LED2].led_num = MEPA_LED2;
    data->led_conf[MEPA_LED2].mode = MEPA_GPIO_MODE_LED_LINK_ACTIVITY;

    //Cable diag data reset
    data->cd_res.link = PHY_LINKDOWN;
    data->cd_res.length[0] = 0;
    data->cd_res.status[0] = MESA_VERIPHY_STATUS_UNKNOWN;

    (void) lan8x8x_phy_setup(dev);

    T_I(  "phy probe port=%u probed phy_id=0x%x\n", data->port_no, dev->drv->id);
}

static mepa_rc lan8x8x_config_led(mepa_device_t *const dev, const mepa_gpio_conf_t *conf)
{
    mepa_rc rc = MEPA_RC_OK;
    uint16_t reg_offset = 0;
    uint16_t reg_val = LAN8X8X_LED_LINK_ACT_ANY_SPEED;

    switch (conf->mode) {
    case MEPA_GPIO_MODE_LED_LINK1000_ACTIVITY:
        reg_val = LAN8X8X_LED_LINK_ACT_1000_SPEED;
        break;
    case MEPA_GPIO_MODE_LED_LINK100_ACTIVITY:
        reg_val = LAN8X8X_LED_LINK_ACT_100_SPEED;
        break;
    case MEPA_GPIO_MODE_LED_LINK_NO_ACT_ANY_SPEED:
        reg_val = LAN8X8X_LED_LINK_NO_ACT_ANY_SPEED;
        break;
    case MEPA_GPIO_MODE_LED_LOCAL_RXER_STATUS:
        reg_val = LAN8X8X_LED_LOCAL_RXER_STATUS;
        break;
    case MEPA_GPIO_MODE_LED_REMOTE_RXER_STATUS:
        reg_val = LAN8X8X_LED_REMOTE_RXER_STATUS;
        break;
    case MEPA_GPIO_MODE_LED_NEGOTIATED_SPEED:
        reg_val = LAN8X8X_LED_NEGOTIATED_SPEED;
        break;
    case MEPA_GPIO_MODE_LED_MASTER_SLAVE_MODE:
        reg_val = LAN8X8X_LED_MASTER_SLAVE_MODE;
        break;
    case MEPA_GPIO_MODE_LED_PCS_TX_ERR_STATUS:
        reg_val = LAN8X8X_LED_PCS_TX_ERR_STATUS;
        break;
    case MEPA_GPIO_MODE_LED_PCS_RX_ERR_STATUS:
        reg_val = LAN8X8X_LED_PCS_RX_ERR_STATUS;
        break;
    case MEPA_GPIO_MODE_LED_PCS_TX_ACTIVITY:
        reg_val = LAN8X8X_LED_PCS_TX_ACTIVITY;
        break;
    case MEPA_GPIO_MODE_LED_PCS_RX_ACTIVITY:
        reg_val = LAN8X8X_LED_PCS_RX_ACTIVITY;
        break;
    case MEPA_GPIO_MODE_LED_WAKE_ON_LAN:
        reg_val = LAN8X8X_LED_WAKE_ON_LAN;
        break;
    case MEPA_GPIO_MODE_LED_FORCE_LED_OFF:
        reg_val = LAN8X8X_LED_FORCED_LED_OFF;
        break;
    case MEPA_GPIO_MODE_LED_FORCE_LED_ON:
        reg_val = LAN8X8X_LED_FORCED_LED_ON;
        break;
    case MEPA_GPIO_MODE_LED_LINK_ACTIVITY:
        reg_val = LAN8X8X_LED_LINK_ACT_ANY_SPEED;
        break;
    default:
        rc = MEPA_RC_ERR_KR_CONF_NOT_SUPPORTED;
        break;
    }
    if ((conf->led_num == MEPA_LED0) || (conf->led_num == MEPA_LED1)) {
        reg_offset = LAN8X8X_COMM_LED1_LED0;
    } else if ((conf->led_num == MEPA_LED2) || (conf->led_num == MEPA_LED3)) {
        reg_offset = LAN8X8X_COMM_LED3_LED2;
    } else {
        rc = MEPA_RC_ERR_KR_CONF_NOT_SUPPORTED;
    }

    if (rc == MEPA_RC_OK) {
        if ((conf->led_num == MEPA_LED1) || (conf->led_num == MEPA_LED3)) {
            reg_val <<= 8;
        }
        rc = phy_mmd_reg_wr(dev, MDIO_MMD_VEND1, reg_offset, reg_val);
    }

    return rc;
}

static mepa_rc lan8x8x_config_leds(mepa_device_t *const dev, mepa_bool_t isolate)
{
    phy_data_t *data = (phy_data_t *) dev->data;
    mepa_gpio_conf_t conf[4];
    mepa_rc rc = MEPA_RC_OK;
    int i;

    for (i = 0; i < 4; i ++) {
        if ((data->led_conf[i].mode >= MEPA_GPIO_MODE_LED_LINK_ACTIVITY) &&
            (data->led_conf[i].mode <= MEPA_GPIO_MODE_LED_DISABLE_EXTENDED)) {
            conf[i].led_num = data->led_conf[i].led_num;
            if (isolate) {
                conf[i].mode = MEPA_GPIO_MODE_LED_FORCE_LED_OFF;
            } else {
                conf[i].mode = data->led_conf[i].mode;
            }
            MEPA_RC_GOTO(rc, lan8x8x_config_led(dev, &conf[i]));
        }
    }

    return rc;
}

static mepa_rc lan8x8x_phy_init(mepa_device_t *const dev)
{
    mepa_rc rc = MEPA_RC_OK;

    //MAC Setup
    MEPA_RC_GOTO(rc, lan8x8x_config_mac(dev));

    //LED setup
    MEPA_RC_GOTO(rc, lan8x8x_config_leds(dev, PHY_FALSE));

    rc = MEPA_RC_OK;

    return rc;
}

/**********************************
 * Internal APIs must be above this
 *********************************/

/**********************************
 * External driver callbacks
 * MEPA_RC must be used to unlock and return!
 *********************************/
static mepa_rc lan8x8x_sqi_read(mepa_device_t *dev, uint32_t *const value)
{
    mepa_rc rc = MEPA_RC_ERROR;
    uint16_t sqi_val = 0;

    if ((dev != NULL) && (value != NULL)) {
        MEPA_ENTER(dev);

        *value = 0;
        rc = phy_mmd_reg_rd(dev, MDIO_MMD_PMAPMD, LAN8X8X_SQI_REG, &sqi_val);
        if (rc == MEPA_RC_OK) {
            *value = LAN8X8X_SQI_GET((uint32_t)sqi_val);
        }
        MEPA_EXIT(dev);
    }

    return rc;
}

static mepa_rc lan8x8x_delete(mepa_device_t *dev)
{
    mepa_rc rc = MEPA_RC_ERROR;

    if (dev != NULL) {
        //cleanup
#ifdef MEPA_lan8x8x_static_mem
        T_D(  "static driver cleanup!!\n");
        (void) memset(dev->data, 0, sizeof(phy_data_t));
        (void) memset(dev, 0, sizeof(mepa_device_t));
        rc = MEPA_RC_OK;
#else
        T_D(  "dynamic driver cleanup!!\n");
        rc = mepa_delete_int(dev);
#endif
    }

    return rc;
}

static mepa_device_t *lan8x8x_probe(mepa_driver_t *drv,
                                    const mepa_callout_t    MEPA_SHARED_PTR *callout,
                                    struct mepa_callout_ctx MEPA_SHARED_PTR *callout_ctx,
                                    struct mepa_board_conf                  *conf)
{
    mepa_device_t   *dev = NULL;

    if (drv != NULL) {
        phy_data_t      *data = NULL;

#ifdef MEPA_lan8x8x_static_mem
        uint8_t pidx = 0;

        T_D(  "static driver create!!\n");
        for (pidx = 0; pidx < LAN8X8X_PHY_MAX; pidx++) {
            if (lan8x8x_data[pidx].ctx_status) {
                T_D(  "LAN8X8X driver already @ idx=%d[port-%d]!!\n", pidx, lan8x8x_device[pidx].numeric_handle);
                continue;
            }

            dev = &lan8x8x_device[pidx];
            data = &lan8x8x_data[pidx];

            T_D(  "LAN8X8X driver probe @ idx=%d, port=%d!!\n", pidx, conf->numeric_handle);
            lan8x8x_fill_probe_data(drv, dev, data, callout, callout_ctx, conf);

            break;
        }
#else
        T_D(  "dynamic driver create!!\n");
        //MISRA C-2023 Rule 11.5 - use static pointer assigned during probe
        dev = mepa_create_int(drv, callout, callout_ctx, conf, (int32_t)(sizeof(phy_data_t)));

        if (dev != NULL) {
            data = dev->data;
            data->port_no = conf->numeric_handle;

            T_I("\n lan8x8x created (%d) at %p, data: %p\n",
                conf->numeric_handle, dev, dev->data);
            lan8x8x_fill_probe_data(drv, dev, data, callout, callout_ctx, conf);
        }
#endif
    }

    return dev;
}

static mepa_rc lan8x8x_conf_get(mepa_device_t *dev,
                                mepa_conf_t *const config)
{
    mepa_rc rc = MEPA_RC_ERROR;

    if ((dev != NULL) && (config != NULL)) {
        const phy_data_t *const data = (const phy_data_t *const)dev->data;

        rc = MEPA_RC_OK;
        MEPA_ENTER(dev);
        (void) memcpy(config, &(data->conf), sizeof(mepa_conf_t));
        MEPA_EXIT(dev);
    }

    return rc;
}

static mepa_rc lan8x8x_conf_set(mepa_device_t *dev, const mepa_conf_t *config)
{
    mepa_rc rc = MEPA_RC_ERROR;

    if ((dev != NULL) && (config != NULL)) {
        MEPA_ENTER(dev);
        rc = lan8x8x_config_set(dev, config);
        MEPA_EXIT(dev);
    }

    return rc;
}

static mepa_rc lan8x8x_reset(mepa_device_t *dev, const mepa_reset_param_t *rst_conf)
{
    mepa_rc rc = MEPA_RC_ERROR;
    mepa_bool_t is_reset = PHY_TRUE;

    if (dev != NULL) {
        phy_data_t *const data = (phy_data_t *const)dev->data;

        MEPA_ENTER(dev);
        if (rst_conf != NULL) {
            // reset points are not supported
            if ((rst_conf->reset_point != MEPA_RESET_POINT_DEFAULT) &&
                (rst_conf->reset_point != MEPA_RESET_POINT_PRE)) {
                rc = MEPA_RC_OK;
                is_reset = PHY_FALSE;
            } else {
                rc = lan8x8x_check_media(dev, rst_conf->media_intf, data->conf.speed);
                if (rc == MEPA_RC_OK) {
                    //re-configure media interface
                    data->media_intf = rst_conf->media_intf;
                    if ((rst_conf->media_intf == MESA_PHY_MEDIA_IF_T1_100FX) &&
                        (data->conf.speed != MESA_SPEED_AUTO)) {
                        data->conf.speed = MESA_SPEED_100M;
                    }
                } else {
                    //Invalid media type
                    is_reset = PHY_FALSE;
                }
            }
        }
        if (is_reset == PHY_TRUE) {
            rc = lan8x8x_int_reset(dev, LAN8X8X_RST_SOFT_EXT);
        }

        MEPA_EXIT(dev);
    }

    return rc;
}

// read direct registers
static mepa_rc lan8x8x_reg_read (mepa_device_t *dev, uint32_t addr, uint16_t *const value)
{
    mepa_rc rc = MESA_RC_ERROR;

    if ((dev != NULL) && (value != NULL)) {
        MEPA_ENTER(dev);
        rc = phy_reg_rd(dev, LAN8X8X_PHY_REG_ADDR(addr), value);
        MEPA_EXIT(dev);
    }

    return rc;
}

// write direct registers
static mepa_rc lan8x8x_reg_write(mepa_device_t *dev, uint32_t addr, uint16_t value)
{
    mepa_rc rc = MESA_RC_ERROR;

    if (dev != NULL) {
        MEPA_ENTER(dev);
        rc = phy_reg_wr(dev, LAN8X8X_PHY_REG_ADDR(addr), value);
        MEPA_EXIT(dev);
    }

    return rc;
}

// read mmd registers
static mepa_rc lan8x8x_mmd_reg_read(mepa_device_t *dev, uint32_t addr, uint16_t *const value)
{
    mepa_rc rc = MESA_RC_ERROR;

    if ((dev != NULL) && (value != NULL)) {
        MEPA_ENTER(dev);
        rc = phy_mmd_reg_rd(dev, PHY_MMD_DEVAD(addr), PHY_REG_ADDR(addr), value);
        MEPA_EXIT(dev);
    }

    return rc;
}

// write mmd registers
static mepa_rc lan8x8x_mmd_reg_write(mepa_device_t *dev, uint32_t addr, uint16_t value)
{
    mepa_rc rc = MESA_RC_ERROR;

    if (dev != NULL) {
        MEPA_ENTER(dev);
        rc = phy_mmd_reg_wr(dev, PHY_MMD_DEVAD(addr), PHY_REG_ADDR(addr), value);
        MEPA_EXIT(dev);
    }

    return rc;
}

static mepa_rc lan8x8x_if_set(mepa_device_t *dev, mepa_port_interface_t mac_if)
{
    mepa_rc rc = MEPA_RC_ERROR;

    if (dev != NULL) {
        rc = MEPA_RC_ERR_KR_CONF_NOT_SUPPORTED;

        if (((mac_if >= MESA_PORT_INTERFACE_RGMII) &&
             (mac_if <= MESA_PORT_INTERFACE_RGMII_TXID)) ||
            (mac_if == MESA_PORT_INTERFACE_SGMII)) {
            phy_data_t *const data = (phy_data_t *const)dev->data;

            rc = MEPA_RC_OK;
            if (data->mac_if != mac_if) {
                MEPA_ENTER(dev);
                //Setup mac_if
                data->mac_if = mac_if;
                rc = lan8x8x_config_mac(dev);
                MEPA_EXIT(dev);
            }
        }
    }

    return rc;
}

static mepa_rc lan8x8x_if_get(mepa_device_t *dev,
                              mepa_port_speed_t speed, mepa_port_interface_t *mac_if)
{
    mepa_rc rc = MESA_RC_ERROR;

    (void) speed;

    if ((dev != NULL) && (mac_if != NULL)) {
        const phy_data_t *const data = (const phy_data_t *const)dev->data;

        rc = MEPA_RC_OK;
        MEPA_ENTER(dev);
        *mac_if = data->mac_if;
        MEPA_EXIT(dev);

        T_D(  "Get MAC Interface type %d\r\n", *mac_if);
    }

    return rc;
}

static mepa_rc lan8x8x_media_set(mepa_device_t *dev, mepa_media_interface_t media_if)
{
    mepa_rc rc = MESA_RC_ERROR;

    if (dev != NULL) {
        rc = MEPA_RC_ERR_KR_CONF_NOT_SUPPORTED;
        if ((media_if == MESA_PHY_MEDIA_IF_T1_100FX) || (media_if == MESA_PHY_MEDIA_IF_T1_1000FX)) {
            phy_data_t *const data = (phy_data_t *const)dev->data;

            rc = MEPA_RC_OK;
            MEPA_ENTER(dev);

            if (data->media_intf != media_if) {
                //check media/speed/aneg capabilities
                rc = lan8x8x_check_media(dev, media_if, data->conf.speed);
                if (rc == MEPA_RC_OK) {
                    data->media_intf = media_if;

                    /* re-configure media interface
                       when aneg = true, apply change
                       when aneg = false, then following
                       old_media_type new_media_type old_speed  new_speed
                       1000_t1         100_t1        1000       100    ==> reset
                       1000_t1         100_t1         100       100    ===> No reset
                       100t1           1000t1         100       100   ===> No reset */
                    if ((data->conf.speed != MESA_SPEED_100M) ||
                        (data->conf.speed == MESA_SPEED_AUTO)) {
                        rc = lan8x8x_int_reset(dev, LAN8X8X_RST_SOFT);
                    }
                    T_D(  "Set media type %d! rc=%d.\r\n", media_if, rc);
                }
            }
            MEPA_EXIT(dev);
        }
    }

    return rc;
}

static mepa_rc lan8x8x_media_get(mepa_device_t *dev, mepa_media_interface_t *media_if)
{
    mepa_rc rc = MESA_RC_ERROR;

    if ((dev != NULL) && (media_if != NULL)) {
        const phy_data_t *const data = (const phy_data_t *const)dev->data;

        rc = MEPA_RC_OK;
        MEPA_ENTER(dev);
        *media_if = data->media_intf;
        MEPA_EXIT(dev);

        T_D(  "Get media type %d\r\n", *media_if);
    }

    return rc;
}

static mepa_rc lan8x8x_info_get(mepa_device_t *dev, mepa_phy_info_t *const phy_info)
{
    mepa_rc rc = MEPA_RC_ERROR;

    if ((dev != NULL) && (phy_info != NULL)) {
        const phy_data_t *const data = (const phy_data_t *const)dev->data;

        MEPA_ENTER(dev);
        if (data->init_done == PHY_TRUE) {
            phy_info->part_number = data->dev.model;
            phy_info->revision = data->dev.rev;
            phy_info->cap = ((data->conf.speed == MESA_SPEED_100M) ?
                             MEPA_CAP_SPEED_MASK_1G : MEPA_CAP_TS_MASK_NONE);

            rc = MEPA_RC_OK;
        }
        MEPA_EXIT(dev);
    }
    T_D(  "phy_info_get rc=%d\r\n", rc);

    return rc;
}

static mepa_rc lan8x8x_debug_info(mepa_device_t *dev,
                                  const mepa_debug_print_t pr,
                                  const mepa_debug_info_t   *const info)
{
    struct phy_reg_dbg lan8x8x_regs[] = {
        { "Basic Control Register", 0U, MII_BMCR, 0U, 0U},
        { "Basic Status Register", 0U, MII_BMSR, 0U, 0U},
        { "Device Identifier 1 Register", 0U, MII_PHYSID1, 0U, 0U},
        { "Device Identifier 2 Register", 0U, MII_PHYSID2, 0U, 0U},
        { "MMD Access Control Register", 0U, MII_MMD_CTRL, 0U, 0U},
        { "MMD Access Address/Data Register", 0U, MII_MMD_DATA, 0U, 0U},
        {
            "chiptop_regs:PMA_CONTROL_T1", MDIO_MMD_PMAPMD, MDIO_PMA_PMD_BT1_CTRL,
            0U, 0U
        },
    };

    mepa_rc rc = MEPA_RC_ERROR;

    if ((dev != NULL) && (pr != NULL) && (info != NULL)) {
        //PHY Debugging
        switch (info->group) {
        case MEPA_DEBUG_GROUP_ALL:
        case MEPA_DEBUG_GROUP_PHY: {
            MEPA_ENTER(dev);
            lan8x8x_phy_reg_dump(dev, pr, lan8x8x_regs, ARRAY_SIZE(lan8x8x_regs), 0U);
            rc = MEPA_RC_OK;
            MEPA_EXIT(dev);
        }
        break;
        default:
            rc = MEPA_RC_OK;
            break;
        }
    }
    return rc;
}

static mepa_rc lan8x8x_poll_int(mepa_device_t *dev, mepa_status_t *status)
{
    phy_data_t *const data = (phy_data_t *const)dev->data;
    mepa_rc rc = MEPA_RC_ERROR;

    //Current link status
    data->link_status = PHY_FALSE;

    status->master = ((data->conf.man_neg == MEPA_MANUAL_NEG_REF) ?
                       PHY_TRUE : PHY_FALSE);
    status->speed = data->conf.speed;

    //T1 PHY supports only Full Duplex
    status->fdx = PHY_TRUE;
    data->dev.is_master = status->master;

    MEPA_RC_GOTO(rc, phy_c45_get_link_status(dev, status));

    return rc;
}

static mepa_rc lan8x8x_poll(mepa_device_t *dev, mepa_status_t *status)
{
    mepa_rc rc = MEPA_RC_ERROR;

    if ((dev != NULL) && (status != NULL)) {
        MEPA_ENTER(dev);
        rc = lan8x8x_poll_int(dev, status);
        MEPA_EXIT(dev);
    }

    return rc;
}

static void fill_driver_info(uint32_t id, uint32_t mask, mepa_driver_t *drv_inst)
{
    T_D(  "Fill driver info for phy_id=0x%x\n", id);
    /* Device ID & Mask */
    drv_inst->id             = id;
    drv_inst->mask           = mask;
    /* LAN8X8X Driver APIs */
    drv_inst->mepa_driver_delete         = &lan8x8x_delete;
    drv_inst->mepa_driver_reset              = &lan8x8x_reset;
    drv_inst->mepa_driver_poll               = &lan8x8x_poll;
    drv_inst->mepa_driver_probe              = &lan8x8x_probe;
    drv_inst->mepa_driver_conf_set           = &lan8x8x_conf_set;
    drv_inst->mepa_driver_conf_get           = &lan8x8x_conf_get;
    drv_inst->mepa_driver_if_set             = &lan8x8x_if_set;
    drv_inst->mepa_driver_if_get             = &lan8x8x_if_get;
    drv_inst->mepa_driver_media_set          = &lan8x8x_media_set;
    drv_inst->mepa_driver_media_get          = &lan8x8x_media_get;
    drv_inst->mepa_driver_sqi_read           = &lan8x8x_sqi_read;
    drv_inst->mepa_driver_phy_info_get       = &lan8x8x_info_get;
    drv_inst->mepa_debug_info_dump           = &lan8x8x_debug_info;
    drv_inst->mepa_driver_clause22_read      = &lan8x8x_reg_read;
    drv_inst->mepa_driver_clause22_write     = &lan8x8x_reg_write;
    drv_inst->mepa_driver_clause45_read      = &lan8x8x_mmd_reg_read;
    drv_inst->mepa_driver_clause45_write     = &lan8x8x_mmd_reg_write;
}

mepa_drivers_t mepa_lan8x8x_driver_init(void)
{
    uint32_t lan8x8x_ids[] = {PHY_ID_LAN8X8X_B, PHY_ID_LAN8X8X_A};
    static mepa_driver_t lan8x8x_driver[2U];
    mepa_drivers_t result;
    uint8_t idx = 0;

    for (idx = 0; idx < ARRAY_SIZE(lan8x8x_driver); idx++) {
        fill_driver_info(lan8x8x_ids[idx], LAN8X8X_PHY_ID_MASK, &lan8x8x_driver[idx]);
    }

    result.phy_drv = &lan8x8x_driver[0];
    result.count = ARRAY_SIZE(lan8x8x_driver);

    return result;
}
#endif // MEPA_HAS_LAN8X8X
