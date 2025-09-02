// Copyright (c) 2004-2024 Microchip Technology Inc. and its subsidiaries.
// SPDX-License-Identifier: MIT

#ifndef LAN8X8X_REGISTERS_H
#define LAN8X8X_REGISTERS_H

#define LAN8X8X_PHY_REG_ADDR(addr)      ((addr) & 0x00FFU)
#define PHY_REG_ADDR(addr) ((addr) & 0xFFFFU)
#define PHY_MMD_DEVAD(addr) (((addr) & 0x00FF0000U) >> 16U)

#define LAN8X8X_DEF_MASK    DEF_MASK

#define BIT32(x)                        ((ONE32) << (x))

#define BIT_MASK(x)             (((ONE) << (x)) - (ONE))
#define BIT32_MASK(x)           (((ONE32) << (x)) - (ONE32))

#define GENMASK(offset, width)      (BIT_MASK(width) << (offset))
#define GENMASK32(offset, width)    (BIT32_MASK(width) << (offset))

#define MCHP_MASK           GENMASK32(15, 0)
#define MCHP_4B_MASK        GENMASK32(31, 0)
#define DEF_MASK            (0xFFFFU)

#define EXTRACT_BITFIELD(value, offset, width)  (((value) >> (offset)) & BIT_MASK(width))
#define EXTRACT_BITS(val, offset, width)    EXTRACT_BITFIELD(val, offset, width)

#define ENCODE_BITFIELD(value, offset, width)   (((value) & BIT_MASK(width)) << (offset))

#define ARRAY_SIZE(x)       (sizeof(x) / sizeof((x)[0]))

/* Generate register address from reg_group + reg_offset */
#define MCHP_TEST_BIT(v, x)      ((((v) & BIT32(x)) != 0U) ? 1U : 0U)

#define MCHP_EXTRACT_V(v, h, l) (((v) & GENMASK32((h), (l))) >> (l))

#define MCHP_BSWAP(A) ((((A) & 0xFF00UL) >> 8UL) | (((A) & 0x00FFUL) << 8UL))

#define MCHP_BSWAP32(V) ((((V) & 0x000000FFU) << 24U) | (((V) & 0x0000FF00U) << 8U) | (((V) & 0x00FF0000U) >> 8U) | (((V) & 0xFF000000U) >> 24U))

#define MCHP_GET_U32(v, idx) ((uint32_t)(v)[(idx)] | \
            ((uint32_t)(v)[(idx) + 1U] << 8U) | \
            ((uint32_t)(v)[(idx) + 2U] << 16U) | \
            ((uint32_t)(v)[(idx) + 3U] << 24U))

#define LAN8X8X_PMA_COMM_100T1_CTL_T1_TYPE_1000 (0x1U)

/* GPIO Registers */
#define LAN8X8X_GPIO_BASE_REG   (0xF020U)
#define LAN8X8X_GPIO_DIR    (LAN8X8X_GPIO_BASE_REG + 0U)
#define LAN8X8X_GPIO_DATA   (LAN8X8X_GPIO_BASE_REG + 2U)
/* Not found in ewood cml*/
#define LAN8X8X_REG_CONTROL1    (0xFFFF)

/* LED Registers */
#define LAN8X8X_LED_BASE_REG            (0xF010U)
#define LAN8X8X_COMM_LED1_LED0          (LAN8X8X_LED_BASE_REG + 1U)
#define LAN8X8X_COMM_LED3_LED2          (LAN8X8X_LED_BASE_REG + 2U)
#define LAN8X8X_LED_LINK_ACT_ANY_SPEED      (0x0U)
#define LAN8X8X_LED_LINK_ACT_1000_SPEED     (0x1U)
#define LAN8X8X_LED_LINK_ACT_100_SPEED      (0x2U)
#define LAN8X8X_LED_LINK_NO_ACT_ANY_SPEED   (0x3U)
#define LAN8X8X_LED_LOCAL_RXER_STATUS       (0x5U)
#define LAN8X8X_LED_REMOTE_RXER_STATUS      (0x6U)
#define LAN8X8X_LED_NEGOTIATED_SPEED        (0x7U)
#define LAN8X8X_LED_MASTER_SLAVE_MODE       (0x8U)
#define LAN8X8X_LED_PCS_TX_ERR_STATUS       (0x9U)
#define LAN8X8X_LED_PCS_RX_ERR_STATUS       (0xAU)
#define LAN8X8X_LED_PCS_TX_ACTIVITY         (0xBU)
#define LAN8X8X_LED_PCS_RX_ACTIVITY     (0xCU)
#define LAN8X8X_LED_WAKE_ON_LAN         (0xDU)
#define LAN8X8X_LED_FORCED_LED_OFF      (0xEU)
#define LAN8X8X_LED_FORCED_LED_ON       (0xFU)

#define LAN8X8X_CLK_RST_REG             0xF070
#define LAN8X8X_RGMII_RX_DLL_CFG        (LAN8X8X_CLK_RST_REG + 0xA)
#define LAN8X8X_RGMII_TX_DLL_CFG        (LAN8X8X_CLK_RST_REG + 0xB)

#define LAN8X8X_RGMII_DELAY_EN          BIT(15)
#define LAN8X8X_RGMII_DLL_EN            BIT(0)
#define LAN8X8X_RGMII_DLL_CONF  (LAN8X8X_RGMII_DELAY_EN |\
                                 LAN8X8X_RGMII_DLL_EN)

/* SQI Registers */
#define LAN8X8X_SQI_REG         (0x8218U)
#define T1_DCQ_SQI_MSK          GENMASK(3, 1)
#define LAN8X8X_SQI_GET(v)  (((v) & T1_DCQ_SQI_MSK) >> ONE)

/* Cable Diagonistics Registers */
#define LAN8X8X_CD_CFG      (0x8918U)
#define LAN8X8X_CD_DONE     BIT(1)
#define LAN8X8X_CD_EN       BIT(0)

#define TC12_HDD_TDR        (0x8930U)
#define TC12_HDD_TDR_LOC    GENMASK(13, 8)
#define TC12_HDD_TDR_STS    GENMASK(7, 4)
#define TC12_HDD_TDR_EN     GENMASK(1, 0)
#define TC12_HDD_TDR_ON     BIT(1)

#define LAN8X8X_CD_STS(v)   (((v) & TC12_HDD_TDR_STS) >> 4U)
#define LAN8X8X_CD_LOC(v)   (((v) & TC12_HDD_TDR_LOC) >> 8U)

/* Loopback Registers */
#define T1_1G_E100T1_PCS_REMOTE_LPBK    (32768U)
#define T1_1G_E1000T1_PCS_REMOTE_LPBK   (33570U)
#define T1_1G_PCS_REMOTE_LPBK       BIT(0)

#define LAN8X8X_CHIPTOP         (0xF0C0U)

/* Interrupts */
#define LAN8X8X_INT_STS0_SC     (LAN8X8X_CHIPTOP + 0x20U)
#define LAN8X8X_INT_EN0_SC              (LAN8X8X_CHIPTOP + 0x24U)
#define LAN8X8X_INT_EN0_MAC_INTRF       BIT(11)
#define LAN8X8X_INT_EN0_EPG         BIT(10)
#define LAN8X8X_INT_EN0_GPIO            BIT(9)
#define LAN8X8X_INT_EN0_WDT         BIT(8)
#define LAN8X8X_INT_EN0_TC10_PRT        BIT(7)
#define LAN8X8X_INT_EN0_CHIP_TOP        BIT(6)
#define LAN8X8X_INT_EN0_PVT         BIT(5)
#define LAN8X8X_INT_EN0_TC10_COM        BIT(4)
#define LAN8X8X_INT_EN0_UVOV            BIT(3)
#define LAN8X8X_INT_EN0_T1_DATA_FAULT       BIT(2)
#define LAN8X8X_INT_EN0_T1_FUNC_SC      BIT(1)
#define LAN8X8X_INT_EN0_T1_FUNC         BIT(0)

#define LAN8X8X_INT_STS1_SC     (LAN8X8X_CHIPTOP + 0x21U)
#define LAN8X8X_INT_EN1_SC      (LAN8X8X_CHIPTOP + 0x25U)
#define LAN8X8X_INT_EN1_MS_EG_EN    BIT(8)
#define LAN8X8X_INT_EN1_MS_IG_EN    BIT(7)
#define LAN8X8X_INT_EN1_MS_FCB_EN   BIT(6)
#define LAN8X8X_INT_EN1_MS_HMAC_EN  BIT(5)
#define LAN8X8X_INT_EN1_MS_LMAC_EN  BIT(4)
#define LAN8X8X_INT_EN1_PTP_PRT     BIT(0)

#define T1_IRQ              (0x8500U)
#define LAN8X8X_INT_IRQ_FUNC_STS    (T1_IRQ)
#define LAN8X8X_INT_IRQ_FUNC_MSK    (T1_IRQ + 2U)
#define LAN8X8X_INT_IRQ_FUNC_CLR    (T1_IRQ + 4U)
#define LAN8X8X_INT_SLEEP_FAIL      BIT(8)
#define LAN8X8X_INT_SLEEP_MODE      BIT(7)
#define LAN8X8X_INT_MS_TRAINING_COMP    BIT(6)
#define LAN8X8X_INT_LINK_CHANGE_1G  BIT(2)
#define LAN8X8X_INT_LINK_CHANGE     BIT(1)

/* T1_100M_PHY_VENDOR_AN */
#define LAN8X8X_V_AN        (0x8000U)
#define LAN8X8X_V_AN_STS    (LAN8X8X_V_AN + 2U)
#define LAN8X8X_V_AN_STS_MS_FAULT   BIT(1)
#define LAN8X8X_V_AN_STS_CFG_AS_MASTER  BIT(0)

#endif //LAN8X8X_REGISTERS_H
