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

static mepa_rc phy_get_link_status(mepa_device_t *const dev,
                                   mepa_status_t *const status)
{
    mepa_rc rc = MEPA_RC_ERROR;
    uint16_t reg_val = 0;
    phy_data_t *const data = (phy_data_t *const)dev->data;

    if (data->conf.speed == MESA_SPEED_100M) {
        rc = phy_mmd_reg_rd(dev, MDIO_MMD_PMAPMD, T1_1G_E100T1_PMA, &reg_val);
    } else {
        rc = phy_mmd_reg_rd(dev, MDIO_MMD_PMAPMD, T1_1G_E1000T1_PMA, &reg_val);
    }

    if (rc == MEPA_RC_OK) {
        status->link = ((reg_val & T1_PMA_LINK_STATUS) != ZERO);
        data->link_status = status->link;
        status->master = (data->conf.man_neg == MEPA_MANUAL_NEG_REF) ?
                         PHY_TRUE : PHY_FALSE;
    }

    return rc;
}

static mepa_rc lan8x8x_config_mac(mepa_device_t *dev)
{
    const phy_data_t *const data = (const phy_data_t *const)dev->data;
    mepa_rc rc = MEPA_RC_ERROR;
    uint16_t txc = 0;
    uint16_t rxc = 0;

    MEPA_RC_GOTO(rc, phy_mmd_reg_set_bits(dev, MDIO_MMD_VEND1,
                                          QSGMII_PCS1G_SOFT_RESET_REG,
                                          QSGMII_PCS1G_SOFT_RESET_EN));

    if (data->mac_if == MESA_PORT_INTERFACE_SGMII) {
        MEPA_RC_GOTO(rc, phy_mmd_reg_set_bits(dev, MDIO_MMD_VEND1,
                                              QSGMII_PCS1G_CONFIG_REG,
                                              QSGMII_PCS1G_CONFIG_PCS_ENA));
        MEPA_RC_GOTO(rc, phy_mmd_reg_set_bits(dev, MDIO_MMD_VEND1,
                                              QSGMII_ANEG_EN_REG,
                                              QSGMII_ANEG_EN));
        MEPA_RC_GOTO(rc, phy_mmd_reg_set_bits(dev, MDIO_MMD_VEND1,
                                              QSGMII_PCS1G_ANEG_CONFIG,
                                              QSGMII_PCS1G_ANEG_ENA));
    } else if ((data->mac_if >= MESA_PORT_INTERFACE_RGMII) &&
               (data->mac_if <= MESA_PORT_INTERFACE_RGMII_TXID)) {
        rc = MEPA_RC_OK;

        switch (data->mac_if) {
        case MESA_PORT_INTERFACE_RGMII:
            txc &= ((~LAN8X8X_RGMII_DLL_CONF) & 0xFFFFU);
            rxc &= ((~LAN8X8X_RGMII_DLL_CONF) & 0xFFFFU);
            break;
        case MESA_PORT_INTERFACE_RGMII_ID:
            txc |= LAN8X8X_RGMII_DLL_CONF;
            rxc |= LAN8X8X_RGMII_DLL_CONF;
            break;
        case MESA_PORT_INTERFACE_RGMII_RXID:
            txc &= ((~LAN8X8X_RGMII_DLL_CONF) & 0xFFFFU);
            rxc |= LAN8X8X_RGMII_DLL_CONF;
            break;
        case MESA_PORT_INTERFACE_RGMII_TXID:
            txc |= LAN8X8X_RGMII_DLL_CONF;
            rxc &= ((~LAN8X8X_RGMII_DLL_CONF) & 0xFFFFU);
            break;
        default:
            rc = MEPA_RC_ERR_KR_CONF_NOT_SUPPORTED;
            break;
        }

        if (rc == MEPA_RC_OK) {
            // Set RX DELAY
            MEPA_RC_GOTO(rc, phy_mmd_reg_wr(dev, MDIO_MMD_VEND1,
                                            LAN8X8X_RGMII_RX_DLL_CFG, rxc));
            // Set TX DELAY
            MEPA_RC_GOTO(rc, phy_mmd_reg_wr(dev, MDIO_MMD_VEND1,
                                            LAN8X8X_RGMII_TX_DLL_CFG, txc));
        }
    } else {
        rc = MEPA_RC_ERR_KR_CONF_NOT_SUPPORTED;
    }

    return rc;
}

static mepa_rc lan8x8x_phy_init(mepa_device_t *const dev)
{
    phy_data_t *const data = (phy_data_t *const)dev->data;
    mepa_rc rc = MEPA_RC_OK;

    if (IS_LAN888X(dev->drv->id)) {
        MEPA_RC_GOTO(rc, phy_mmd_reg_set_bits(dev, MDIO_MMD_PCS,
                                              T1_1G_E1000T1_PCS_EN,
                                              T1_1G_E1000T1_PCS_EN_));
    } else {
        MEPA_RC_GOTO(rc, phy_mmd_reg_wr(dev, MDIO_MMD_PCS,
                                        T1_1G_E1000T1_PCS_EN, 0));
    }

    //MAC Setup
    MEPA_RC_GOTO(rc, lan8x8x_config_mac(dev));

    rc = MEPA_RC_OK;

    return rc;
}

static mepa_rc lan8x8x_check_media(const mepa_device_t *const dev,
                                   mepa_media_interface_t media_if,
                                   mesa_port_speed_t speed)
{
    mepa_rc rc = MEPA_RC_ERR_KR_CONF_NOT_SUPPORTED;

    if ((media_if == MESA_PHY_MEDIA_IF_T1_100FX) ||
        ((media_if == MESA_PHY_MEDIA_IF_T1_1000FX) &&
         IS_LAN888X(dev->drv->id))) {
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

static mepa_rc lan8x8x_phy_reset(mepa_device_t *dev)
{
    phy_data_t *const data = (phy_data_t *const)dev->data;
    mepa_rc rc = MEPA_RC_OK;

    // CL22 soft-reset to let the link re-train
    MEPA_RC_GOTO(rc, phy_reg_set_bits(dev, MII_BMCR, BMCR_RESET));

    // disable aneg
    MEPA_RC_GOTO(rc, phy_mmd_reg_clear_bits(dev, MDIO_MMD_AN,
                                            MDIO_AN_T1_CTRL,
                                            MDIO_AN_CTRL1_ENABLE));

    MEPA_RC_GOTO(rc, phy_mmd_reg_poll(dev, 0U, MII_BMCR,
                                      BMCR_RESET, PHY_FALSE, 4000U));

    /* Clear LINK_CONTROL and PHY_CONFIG_DONE */
    MEPA_RC_GOTO(rc, phy_mmd_reg_wr(dev, MDIO_MMD_PMAPMD,
                                    T1_1G_TOP_CTRL_CONFIG, 0U));

    /* Set SYSTEM_CONTROL_SOFT_RESET */
    MEPA_RC_GOTO(rc, phy_mmd_reg_wr(dev, MDIO_MMD_PMAPMD,
                                    T1_1G_TOP_CTRL_CONFIG,
                                    T1_1G_TOP_CTRL_SOFT_RESET));

    /* Wait for RESET done */
    MEPA_RC_GOTO(rc, phy_mmd_reg_poll(dev, MDIO_MMD_PMAPMD,
                                      T1_1G_TOP_CTRL_CONFIG,
                                      T1_1G_TOP_CTRL_SOFT_RESET,
                                      PHY_FALSE, 4000U));

    data->init_done = PHY_FALSE;

    return MEPA_RC_OK;
}

static mepa_rc lan8x8x_config_led(mepa_device_t *const dev,
                                  const mepa_gpio_conf_t *conf)
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

//one-time configuration to be done after CONFIG_DONE
static int lan8x8x_onetime_post_config_done(mepa_device_t *const dev)
{
    phy_data_t *const data = (phy_data_t *const)dev->data;
    int rc;

    if (!data->init_done) {
        //LED setup
        MEPA_RC_GOTO(rc, lan8x8x_config_leds(dev, PHY_FALSE));

        /* Bypass MACsec mega block */
        MEPA_RC_GOTO(rc,
                     phy_mmd_reg_set_bits(dev, MDIO_MMD_VEND1,
                                          XGMII_GMII_BYPASS,
                                          XGMII_BYPASS_SET_));

        data->init_done = PHY_TRUE;
    }

    return 0;
}

static int lan8x8x_config_done(mepa_device_t *const dev)
{
    int rc;

    /* Enable LINK_CONTROL + CONFIG_DONE */
    MEPA_RC_GOTO(rc, phy_mmd_reg_wr(dev, MDIO_MMD_PMAPMD,
                                    T1_1G_TOP_CTRL_CONFIG,
                                    T1_1G_TOP_CTRL_CONFIG_SET));

    return lan8x8x_onetime_post_config_done(dev);
}

static int lan8x8x_speed_config(mepa_device_t *const dev)
{
    phy_data_t *const data = (phy_data_t *const)dev->data;
    uint16_t val;


    if (data->conf.speed == MEPA_SPEED_100M) {
        phy_mmd_reg_wr32(dev, MDIO_MMD_PMAPMD,
                         T1_1G_E100T1_PMD_ADPLL_CFG_0,
                         0x600E2804);

        /* AFE Config */
        phy_mmd_reg_wr32(dev, MDIO_MMD_PMAPMD,
                         T1_1G_E100T1_PMA_ADFE_CFG2, 0x14F4040C);

        phy_mmd_reg_wr(dev, MDIO_MMD_PMAPMD,
                       T1_1G_E100T1_PMA_ADFE_CFG3, 0x43);
    } else {
        phy_mmd_reg_wr32(dev, MDIO_MMD_PMAPMD,
                         T1_1G_E1000T1_PMD_LCPLL_CFG_0,
                         0x5FE34C08);
    }

    return lan8x8x_config_done(dev);
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

    MEPA_RC_GOTO(rc, phy_mmd_reg_wr(dev, MDIO_MMD_PMAPMD,
                                    MDIO_PMA_PMD_BT1_CTRL, reg_val));

    return MEPA_RC_OK;
}

static int lan8x8x_pma_baset1_setup_aneg(mepa_device_t *const dev)
{
    phy_data_t *data = (phy_data_t *)dev->data;
    mepa_rc rc = MEPA_RC_OK;
    u16 adv_r1 = 0;
    u16 adv_r2_mask = 0;
    u16 adv_r2 = 0;

    adv_r2_mask = (MDIO_AN_T1_ADV_M_B1000 |
                   MDIO_AN_T1_ADV_M_B100 |
                   MDIO_AN_T1_ADV_M_MST);

    // Advertise preferred master/slave mode
    if (data->conf.man_neg != MEPA_MANUAL_NEG_CLIENT) {
        adv_r2 |= MDIO_AN_T1_ADV_M_MST;
    }

    // Advertise 1G and 100M speed
    if ((data->media_intf != MESA_PHY_MEDIA_IF_T1_100FX)
        && (data->conf.aneg.speed_1g_fdx == PHY_TRUE)) {
        adv_r2 |= MDIO_AN_T1_ADV_M_B100 | MDIO_AN_T1_ADV_M_B1000;
    } else {
        adv_r2 |= MDIO_AN_T1_ADV_M_B100;
    }

    MEPA_RC_GOTO(rc, phy_mmd_reg_modify(dev, MDIO_MMD_AN, MDIO_AN_T1_ADV_M, adv_r2_mask, adv_r2));

    /* NOTE:
     *  The Base Page value is transferred to mr_adv_ability when register 7.514 is written.
     *  Therefore, registers 7.515 and 7.516 should be written before 7.514.
     */
    MEPA_RC_GOTO(rc, phy_mmd_reg_wr(dev, MDIO_MMD_AN, MDIO_AN_T1_ADV_L, adv_r1));

    MEPA_RC_GOTO(rc, phy_mmd_reg_set_bits(dev, MDIO_MMD_AN, MDIO_AN_T1_CTRL,
                                          MDIO_AN_CTRL1_ENABLE | MDIO_AN_CTRL1_RESTART));

    return MEPA_RC_OK;
}

static mepa_rc lan8x8x_phy_setup(mepa_device_t *const dev)
{
    mepa_rc rc = MEPA_RC_OK;
    phy_data_t *const data = (phy_data_t *const)dev->data;

    MEPA_RC_GOTO(rc, lan8x8x_phy_reset(dev));

    MEPA_RC_GOTO(rc, lan8x8x_phy_init(dev));

    if (data->conf.speed == MESA_SPEED_AUTO) {
        MEPA_RC_GOTO(rc, lan8x8x_pma_baset1_setup_aneg(dev));

        if (IS_LAN888X(dev->drv->id)) {
            MEPA_RC_GOTO(rc, lan8x8x_config_done(dev));
        }
    } else {
        MEPA_RC_GOTO(rc, lan8x8x_pma_baset1_setup_forced(dev));
        MEPA_RC_GOTO(rc, lan8x8x_speed_config(dev));
    }

    T_D(  "PHY port=%u setup complete!\n", data->port_no);

    return MEPA_RC_OK;
}

static mepa_rc lan8x8x_config_set(mepa_device_t *dev, const mepa_conf_t *config)
{
    mepa_rc rc = MEPA_RC_OK;
    phy_data_t *const data = (phy_data_t *const)dev->data;
    mepa_bool_t re_config = PHY_FALSE;

    if (((config->man_neg != MEPA_MANUAL_NEG_CLIENT) &&
         (config->man_neg != MEPA_MANUAL_NEG_REF)) ||
        (config->fdx != PHY_TRUE) ||
        ((config->speed != MESA_SPEED_100M) &&
         (config->speed != MESA_SPEED_1G) &&
         (config->speed != MESA_SPEED_AUTO))) {
        rc = MEPA_RC_ERR_PARM;
    } else {
        mesa_port_speed_t speed = config->speed;

        data->conf.fdx = PHY_TRUE;
        data->conf.flow_control = config->flow_control;
        data->conf.aneg.speed_100m_fdx = config->aneg.speed_100m_fdx;
        data->conf.aneg.speed_1g_fdx = config->aneg.speed_1g_fdx;

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
                //clear power down bit
                MEPA_RC_GOTO(rc, phy_mmd_reg_clear_bits(dev, MDIO_MMD_PMAPMD,
                                                        MDIO_CTRL1, BMCR_PDOWN));
                //Setup PHY
                MEPA_RC_GOTO(rc, lan8x8x_phy_setup(dev));
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

static mepa_rc lan8x8x_aneg_resolve_master_slave(mepa_device_t *dev,
                                                 uint8_t *mode)
{
    uint16_t val;
    mepa_rc rc;

    MEPA_RC_GOTO(rc, phy_mmd_reg_rd(dev, MDIO_MMD_AN,
                                    T1_AUTONEG_STATUS, &val));

    *mode = MASTER_SLAVE_STATE_SLAVE;
    if ((val & T1_AUTONEG_MS_CONFIG_FAULT) != 0U) {
        *mode = MASTER_SLAVE_STATE_ERR;
    } else if ((val & T1_AUTONEG_CONFIG_AS_MASTER) != 0U) {
        *mode = MASTER_SLAVE_STATE_MASTER;
    }

    return MEPA_RC_OK;
}

static mepa_rc lan8x8x_aneg_read_status(mepa_device_t *dev,
                                        mepa_status_t *status)
{
    mepa_rc rc = MEPA_RC_INCOMPLETE;
    //mepa_bool_t lp_sym_pause, lp_asym_pause;
    uint16_t val = 0, lp_l = 0, lp_m = 0;
    phy_data_t *data = (phy_data_t *)dev->data;

    status->aneg.obey_pause = PHY_FALSE;
    status->aneg.generate_pause = PHY_FALSE;
    status->link = PHY_FALSE;
    status->speed = MESA_SPEED_UNDEFINED;

    MEPA_RC_GOTO(rc, phy_mmd_reg_rd(dev, MDIO_MMD_AN, MDIO_AN_T1_STAT, &val));
    if ((val & MDIO_AN_STAT1_COMPLETE) == ZERO) {
        //T_D( MEPA_TRACE_GRP_GEN, "aneg is not completed \r\n");
    } else {
        rc = MEPA_RC_OK;

        MEPA_RC_GOTO(rc, phy_mmd_reg_rd(dev, MDIO_MMD_AN,
                                        MDIO_AN_T1_LP_L, &lp_l));
        MEPA_RC_GOTO(rc, phy_mmd_reg_rd(dev, MDIO_MMD_AN,
                                        MDIO_AN_T1_LP_M, &lp_m));

        if (((lp_m & LPA_1000FULL) == LPA_1000FULL) &&
            (data->conf.aneg.speed_1g_fdx == PHY_TRUE)) {
            status->speed = MEPA_SPEED_1G;
        } else if (((lp_m & LPA_100FULL) == LPA_100FULL) &&
                   (data->conf.aneg.speed_100m_fdx == PHY_TRUE)) {
            status->speed = MEPA_SPEED_100M;
        } else {
            status->speed = MESA_SPEED_UNDEFINED;
        }
        //T_D( MEPA_TRACE_GRP_GEN, "aneg link resolved \r\n");
    }

    return rc;
}

static void lan8x8x_fill_probe_data(mepa_driver_t *drv,
                                    mepa_device_t *dev,
                                    phy_data_t *data,
                                    const mepa_callout_t MEPA_SHARED_PTR *callout,
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
    data->conf.speed = MESA_SPEED_AUTO;
    data->conf.aneg.speed_100m_fdx = PHY_TRUE;
    data->conf.aneg.speed_1g_fdx = PHY_FALSE;
    data->media_intf = MESA_PHY_MEDIA_IF_T1_100FX;

    if (IS_LAN888X(dev->drv->id)) {
        data->media_intf = MESA_PHY_MEDIA_IF_T1_1000FX;
        data->conf.aneg.speed_1g_fdx = PHY_TRUE;
        T_I(  "LAN888X_A phy_id=0x%x\n", dev->drv->id);
    }
    data->mac_if = MESA_PORT_INTERFACE_RGMII_TXID;

    data->led_conf[MEPA_LED2].led_num = MEPA_LED2;
    data->led_conf[MEPA_LED2].mode = MEPA_GPIO_MODE_LED_LINK_ACTIVITY;

    //Cable diag data reset
    data->cd_res.link = PHY_LINKDOWN;
    data->cd_res.length[0] = 0;
    data->cd_res.status[0] = MESA_VERIPHY_STATUS_UNKNOWN;

    (void) lan8x8x_phy_setup(dev);

    T_I(  "phy probe port=%u probed phy_id=0x%x\n",
          data->port_no, dev->drv->id);
}

//callout print API
static void phy_dbg_pr (mepa_device_t *const dev, const mepa_debug_print_t pr,
                        uint8_t mmd, uint16_t offset, const char *str)
{
    uint16_t value = 0;

    if (MEPA_RC_OK == phy_mmd_reg_rd(dev, mmd, offset, &value)) {
        (void) pr("%-45s:\t[0X%02X].[0X%X]\t=\t0X%08X \r\n", str, mmd, offset, value);
    }
}

//Register dump
static void phy_reg_dump(struct mepa_device *const dev,
                         const mepa_debug_print_t pr,
                         const struct phy_reg_dbg *const regs, const uint8_t reglen)
{
    uint8_t i;
    phy_data_t *data = (phy_data_t *)dev->data;
    uint32_t port_no = data->port_no;
    uint32_t dev_id = dev->drv->id;

    //Direct registers
    (void) pr("************ Register Dump for PHY-0x%x @ Port-%u ************ \r\n", dev_id, port_no);
    (void) pr("%-45s:\tPAGE.REG\t=\tVALUE \r\n", "REG_NAME");
    for (i = 0; i < reglen; i++) {
        phy_dbg_pr(dev, pr, regs[i].mmd, regs[i].reg, regs[i].string);
    }
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
    uint16_t sqi_reg = LAN8X8X_SQI_1000_REG, sqi_val = 0;

    if ((dev != NULL) && (value != NULL)) {
        MEPA_ENTER(dev);

        const phy_data_t *const data = (const phy_data_t *const)dev->data;
        *value = 0;

        if (data->conf.speed == MESA_SPEED_100M) {
            sqi_reg = LAN8X8X_SQI_100_REG;
        }

        rc = phy_mmd_reg_rd(dev, MDIO_MMD_PMAPMD, sqi_reg, &sqi_val);
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
                T_D("LAN8X8X driver already @ idx=%d[port-%d]!!\n",
                    pidx, lan8x8x_device[pidx].numeric_handle);
                continue;
            }

            dev = &lan8x8x_device[pidx];
            data = &lan8x8x_data[pidx];

            T_D("LAN8X8X driver probe @ idx=%d, port=%d!!\n",
                pidx, conf->numeric_handle);
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

// read direct registers
static mepa_rc lan8x8x_reg_read(mepa_device_t *dev, uint32_t addr, uint16_t *const value)
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
static mepa_rc lan8x8x_mmd_reg_read(mepa_device_t *dev,
                                    uint32_t addr, uint16_t *const value)
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
static mepa_rc lan8x8x_mmd_reg_write(mepa_device_t *dev,
                                     uint32_t addr, uint16_t value)
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
                rc = lan8x8x_phy_setup(dev);
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

static mepa_rc lan8x8x_media_set(mepa_device_t *dev,
                                 mepa_media_interface_t media_if)
{
    mepa_rc rc = MESA_RC_ERROR;

    if (dev != NULL) {
        rc = MEPA_RC_ERR_KR_CONF_NOT_SUPPORTED;
        if ((media_if == MESA_PHY_MEDIA_IF_T1_100FX) ||
            (media_if == MESA_PHY_MEDIA_IF_T1_1000FX)) {
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
                        rc = lan8x8x_phy_setup(dev);
                    }
                    T_D(  "Set media type %d! rc=%d.\r\n", media_if, rc);
                }
            }
            MEPA_EXIT(dev);
        }
    }

    return rc;
}

static mepa_rc lan8x8x_media_get(mepa_device_t *dev,
                                 mepa_media_interface_t *media_if)
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

static mepa_rc lan8x8x_info_get(mepa_device_t *dev,
                                mepa_phy_info_t *const phy_info)
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
        {"CL22 Control",                0, 0},
        {"CL22 Status",                 0, 1},
        {"CHIPTOP:MACSEC_STATUS",       MDIO_MMD_VEND1, (CHIPTOP + 0x2)},
        {"CHIPTOP:MAC_NE_LPBK",         MDIO_MMD_VEND1, (CHIPTOP + 0x1F)},
        {"CHIPTOP:T1_PHY_RES_CAL",      MDIO_MMD_VEND1, (CHIPTOP + 0x28)},
        {"CHIPTOP:T1_PHY_COMM_READY",   MDIO_MMD_VEND1, (CHIPTOP + 0x2D)},
        {"CHIPTOP:STRAP_READ_REG",      MDIO_MMD_VEND1, (CHIPTOP + 0x36)},
        {"CHIPTOP:CLKOUT_CONFIG",       MDIO_MMD_VEND1, (CHIPTOP + 0x37)},
        {"T1_1G_TOP_CTRL_CONFIG",       MDIO_MMD_PMAPMD, T1_1G_TOP_CTRL_CONFIG},
        {"T1_1G_E1000T1_PCS_EN",        MDIO_MMD_PCS, T1_1G_E1000T1_PCS_EN},
        {"CLK_RST_RGMII_RX_DLL_CFG",    MDIO_MMD_VEND1, LAN8X8X_RGMII_RX_DLL_CFG},
        {"CLK_RST_RGMII_TX_DLL_CFG",    MDIO_MMD_VEND1, LAN8X8X_RGMII_TX_DLL_CFG},
        {"AN:BASE_T1_AN_CONTROL",       MDIO_MMD_AN, 0x200},
        {"AN:BASE_T1_AN_STATUS",        MDIO_MMD_AN, 0x201},
        {"AN:AUTONEG_STATUS",           MDIO_MMD_AN, 0x8002},
    };

    mepa_rc rc = MEPA_RC_ERROR;

    if ((dev != NULL) && (pr != NULL) && (info != NULL)) {
        //PHY Debugging
        switch (info->group) {
        case MEPA_DEBUG_GROUP_ALL:
        case MEPA_DEBUG_GROUP_PHY: {
            MEPA_ENTER(dev);
            phy_reg_dump(dev, pr, lan8x8x_regs, ARRAY_SIZE(lan8x8x_regs));
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
    uint16_t old_speed = status->speed;
    mepa_rc rc = MEPA_RC_ERROR;
    uint8_t master_slave;
    uint16_t val;

    //Current link status
    data->link_status = PHY_FALSE;

    if (data->conf.speed == MESA_SPEED_AUTO) {
        if (IS_LAN888X(dev->drv->id)) {
            if (data->conf.speed != old_speed && old_speed != MESA_SPEED_UNDEFINED) {
                MEPA_RC_GOTO(rc, lan8x8x_speed_config(dev));
            }
        }
        //Resolve speed
        MEPA_RC_GOTO(rc, lan8x8x_aneg_read_status(dev, status));
        //Resolve mode
        MEPA_RC_GOTO(rc, lan8x8x_aneg_resolve_master_slave(dev, &master_slave));
        status->master = (master_slave == MASTER_SLAVE_STATE_MASTER) ?
                         PHY_TRUE : PHY_FALSE;
        MEPA_RC_GOTO(rc, phy_mmd_reg_rd(dev, MDIO_MMD_AN, MDIO_AN_T1_STAT, &val));
        status->link = (val & MDIO_STAT1_LSTATUS) ? PHY_TRUE : PHY_FALSE;
        data->link_status =  status->link;
    } else {
        MEPA_RC_GOTO(rc, phy_get_link_status(dev, status));

        status->master = ((data->conf.man_neg == MEPA_MANUAL_NEG_REF) ?
                          PHY_TRUE : PHY_FALSE);
        status->speed = data->conf.speed;
    }

    //T1 PHY supports only Full Duplex
    status->fdx = PHY_TRUE;
    data->dev.is_master = status->master;

    return rc;
}

static mepa_rc lan8x8x_aneg_status_get(mepa_device_t *dev, mepa_aneg_status_t *status)
{
    mepa_rc rc = MEPA_RC_ERROR;
    uint8_t master_slave_state;

    if ((dev != NULL) && (status != NULL)) {
        phy_data_t *data = (phy_data_t *)dev->data;

        rc = MEPA_RC_INV_STATE;
        if (data->conf.speed == MESA_SPEED_AUTO) {

            MEPA_ENTER(dev);

            rc = lan8x8x_aneg_resolve_master_slave(dev, &master_slave_state);
            if (rc == MEPA_RC_OK) {
                status->master_cfg_fault = (master_slave_state == MASTER_SLAVE_STATE_ERR) ? PHY_TRUE : PHY_FALSE;
                status->master = (master_slave_state == MASTER_SLAVE_STATE_MASTER) ? PHY_TRUE : PHY_FALSE;
            }

            MEPA_EXIT(dev);
        }
    }

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

static mepa_rc lan8x8x_reset(mepa_device_t *dev,
                             const mepa_reset_param_t *rst_conf)
{
    mepa_rc rc = MEPA_RC_ERROR;

    (void) rst_conf;

    if (dev != NULL) {
        MEPA_ENTER(dev);
        rc = lan8x8x_phy_setup(dev);
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
    drv_inst->mepa_driver_aneg_status_get    = &lan8x8x_aneg_status_get;
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
    uint32_t lan8x8x_ids[] = {PHY_ID_LAN888X, PHY_ID_LAN878X};
    static mepa_driver_t lan8x8x_driver[2U];
    mepa_drivers_t result;
    uint8_t idx = 0;

    for (idx = 0; idx < ARRAY_SIZE(lan8x8x_driver); idx++) {
        fill_driver_info(lan8x8x_ids[idx], PHY_ID_MASK, &lan8x8x_driver[idx]);
    }

    result.phy_drv = &lan8x8x_driver[0];
    result.count = ARRAY_SIZE(lan8x8x_driver);

    return result;
}
#endif // MEPA_HAS_LAN8X8X
