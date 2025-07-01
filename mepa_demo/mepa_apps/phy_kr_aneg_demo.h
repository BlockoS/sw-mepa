// Copyright (c) 2004-2020 Microchip Technology Inc. and its subsidiaries.
// SPDX-License-Identifier: MIT

#define MAKEDWORD(low, high)	  ((uint32_t)(((uint32_t)((uint16_t)(low))) | (((uint32_t)((uint16_t)(high))) << 16UL)))

#define KR_LOG_MAX_PORTS 4 // KR_LOG_MAX_PORTS - 0x04. This includes Host and line side for a port
#define KR_LOG_BASE_SUMMARY_PORT0     (0x00087000U) /**< \brief (KR LOG   ) Base Address */
#define MAX_MEMRW_DATA_LEN			1012U
#define MAX_DATA_RAM_ADDRESS 0x90000

#define IRQ_VEC0_AN_RATE						(0x000FU)
/* Gen 1 timer done */
#define IRQ_VEC0_GEN1_DONE						(0x0010U)
/* Gen 0 timer done */
#define IRQ_VEC0_GEN0_DONE						(0x0020U)
/* AN Incompatible link */
#define IRQ_VEC0_INCP_LINK						(0x0040U)
/* AN LP Next page received */
#define IRQ_VEC0_NP_RX							(0x0080U)
/* AN Next page fail */
#define IRQ_VEC0_NP_FAIL						(0x0100U)
/* AN Acknowledge detect fail */
#define IRQ_VEC0_ACK_FAIL						(0x0200U)
/* Ability detect fail */
#define IRQ_VEC0_ABD_FAIL						(0x0400U)
/* AN Link fail */
#define IRQ_VEC0_LINK_FAIL						(0x0800U)
/* AN Good */
#define IRQ_VEC0_AN_GOOD						(0x1000U)
/* AN Complete acknowledgement */
#define IRQ_VEC0_CMPL_ACK						(0x2000U)
/* AN Rate detect */
#define IRQ_VEC0_AN_RATE_DET					(0x4000U)
/* AN Train */
#define IRQ_VEC0_AN_TRAIN						(0x8000U)

/* AN Transmit disable */
#define IRQ_VEC1_AN_XMIT_DISABLE				(0x10000U)
/* DME viol = 1 */
#define IRQ_VEC1_DME_VIOL_1						(0x20000U)
/* DME viol = 0 */
#define IRQ_VEC1_DME_VIOL_0						(0x40000U)
/* FR lock = 1 */
#define IRQ_VEC1_FRLOCK_1						(0x80000U)
/* FR lock = 0 */
#define IRQ_VEC1_FRLOCK_0						(0x100000U)
/* REM RDY = 1 */
#define IRQ_VEC1_REM_RDY_1						(0x200000U)
/* REM RDY = 0 */
#define IRQ_VEC1_REM_RDY_0						(0x400000U)
/* BER BUSY = 1 */
#define IRQ_VEC1_BER_BUSY_1						(0x800000U)
/* BER BUSY = 0 */
#define IRQ_VEC1_BER_BUSY_0						(0x1000000U)
/* Max wait timer done */
#define IRQ_VEC1_MW_DONE						(0x2000000U)
/* wait timer done */
#define IRQ_VEC1_WT_DONE						(0x4000000U)
/* New coefficient update request received from LP */
#define IRQ_VEC1_LPCVALID						(0x8000000U)
/* New status report request received from LP */
#define IRQ_VEC1_LPSVALID						(0x10000000U)
/* KR Active bit has toggled */
#define IRQ_VEC1_KR_ACTV						(0x20000000U)
/* AN assertion of ack finished signal */
#define IRQ_VEC1_ACK_FIN						(0x40000000U)

/*
* AN New request to configure NP_TX registers
* Asserted along with CMPL_ACK, if next page is to be transmitted
*/
#define IRQ_VEC1_NP_REQ							(0x80000000U) // no need

#define  AN_BP0_Selector						0x001F
#define  AN_BP0_Echoed_Nonce					0x03E0
#define  AN_BP0_Pause_Ability				0x0C00
#define  AN_BP0_Reserved						0x1000
#define  AN_BP0_RF							0x2000
#define  AN_BP0_Ack							0x4000
#define  AN_BP0_NP							0x8000
#define  AN_BP1_Transmitted_Nonce			0x000f
#define  AN_BP2_FEC_Capability				0xF000
#define  AN_BP1_TA_25G						0x8000
#define  AN_BP1_TA_10G						0x0080
#define  AN_BP1_TA_1G						0x0020
#define  AN_NP1_25GKR						0x0010
#define  AN_NP1_25GCR						0x0020
#define  AN_NP2_F1							0x0100
#define  AN_NP2_F2							0x0200
#define  AN_NP2_F3							0x0400
#define  AN_NP2_F4							0x0800
#define  AN_NP0_MCF							0x0005
#define  AN_NP0_UCF							0x0003

/*AN status*/
#define AN_STS0                                           (0x1)
#define AN_COMPLETE										  (0x20)

#define ETH_STS                                           (0x30)
/* BP_ETH_STS */
#define AN_BP_ETH_STS_BP_ABLE			(0x0001U)
#define AN_BP_ETH_STS_NEG_1G_KX			(0x0002U)
#define AN_BP_ETH_STS_NEG_10G_KX4		(0x0004U)
#define AN_BP_ETH_STS_NEG_10G_KR		(0x0008U)
#define AN_BP_ETH_STS_NEG_R_FEC			(0x0010U)
#define AN_BP_ETH_STS_NEG_RS_FEC		(0x0080U)
#define AN_BP_ETH_STS_NEG_25G_KR_S		(0x1000U)
#define AN_BP_ETH_STS_NEG_25G_KR		(0x2000U)
#define AN_BP_ETH_STS_NEG_2P5G_KX		(0x4000U)
#define AN_BP_ETH_STS_NEG_5G_KR			(0x8000U)

#define KR_LINEID 0x1       /* Device ID for Line KR */
#define KR_HOSTID 0x9       /* Device ID for Host KR */
#define GRP0_LANE_21 0xF121 // GRP0: LN_CFG_VGA_CTRL_BYP_4_0
#define GRP0_LANE_22 0xF122 // GRP0: LN_CFG_EQR_FORCE_3_0
#define GRP1_LANE_D0 0xF1D0 // GRP0: LN_CFG_EQR_FORCE_3_0
#define GRP1_LANE_DD 0xF1DD

#define LINE_PCS_CFG            0x3	/* Device ID for LINE PCS CFG */
#define HOST_PCS_CFG            0xB	/* Device ID for HOST PCS CFG */

#define HOST_KR_ANEG_DEVICE                   (0x0F)
#define LINE_KR_ANEG_DEVICE                   (0x07)


typedef enum {
	BER_GO_TO_MIN,
	BER_CALCULATE_BER,
	BER_MOVE_TO_MID_MARK,
	BER_LOCAL_RX_TRAINED
} ber_stage_t;

typedef enum {
	eKR_COEF_PRESET,
	eKR_COEF_INIT,
	eKR_COEF_CP1,
	eKR_COEF_C0,
	eKR_COEF_CM1,
} CoeffType_t;

typedef enum {
	KR_COEF_NOT_UPDATED,
	KR_COEF_UPDATED,
	KR_COEF_MINIMUM,
	KR_COEF_MAXIMUM,
} kr_rx_lps_t;

/*Tx_Lpc values*/
typedef enum {
	eKR_COEF_HOLD,
	eKR_COEF_INCR,
	eKR_COEF_DECR
} CoeffUpdate_t;

typedef struct {
	uint32_t timestamp : 32;   //(FreeRTOS Tick value)
	uint32_t IRQ : 32;         //(IRQ mapped this field)
	uint32_t CM1 : 8;          //(Coefficient CM1 value)
	uint32_t C0 : 8;           //(Coefficient C0 value)
	uint32_t CP1 : 8;          //(Coefficient CP1 value)
	uint32_t EYE_HIGHT : 8;    //(BER value)
	uint32_t CommandSentToLP : 4;          /*Command Sent to LP((PRESET/INIT/CM1/C0/CP1)) NA-0xff*/
	uint32_t LD_TAPRecieved : 3;          /* TAP Received (CM1/C0/CP1)*/
	uint32_t LD_CommandReceived : 3;          /*Recived Command from LP(INIT/PRESET/INCR/DECR/HOLD)*/
	uint32_t BER_STATE : 2;    //(GO_TO_MIN/CALC_BER/MOVE_TO_MID/RX_TRAINED)
	uint32_t StatusReportSentToLP : 2;       /*Status Report Sent to LP (UPDATED/NOTUPDATED/MIN/MAX)*/
	uint32_t StatusReportReceivedFromLP : 3;       /* Status Report Received From LP (UPDATED/NOTUPDATED/MIN/MAX)*/
	uint32_t Statemachine : 4; // (14 State Machine States)
	uint32_t TAPSentToLP : 3;          /* TAP Sent to LP (CM1/C0/CP1)*/
} KR_Log;


typedef struct
{
	uint32_t timestamp : 32;   /* (Time Stamp value in micro seconds)  */
	uint32_t IRQ : 32;         /* (IRQ0 to IRQ31 mapped to this field) */
	uint32_t LPage_BP0 : 16;   /* (D[15]: D[0])                        */
	uint32_t LPage_BP1 : 16;   /* (D[31]: D[16])                       */
	uint32_t LPage_BP2 : 16;   /* (D[47]: D[32])                       */
	uint32_t LPage_NP0 : 16;   /* (Next Page 0 value)                  */
	uint32_t LPage_NP1 : 16;   /* (Next Page 1 value)                  */
	uint32_t LPage_NP2 : 16;   /* (Next Page 2 value)                  */
	uint32_t Statemachine : 4; /* (14 ANEG State Machine States)       */
}ANEG_Log;

typedef struct {
	uint16_t wAneg_status;
	uint16_t timeinsec;
	uint16_t LP_CM1_MAX;
	uint16_t LP_CM1_END;
	uint16_t LP_C0_MAX;
	uint16_t LP_C0_END;
	uint16_t LP_CP1_MAX;
	uint16_t LP_CP1_END;

	uint16_t BER_COUNT_CM1[64];
	uint16_t BER_COUNT_C0[64];
	uint16_t BER_COUNT_CP1[64];   

	uint16_t EYE_HT_CM1[64];
	uint16_t EYE_HT_C0[64];
	uint16_t EYE_HT_CP1[64];
              
	uint16_t LD_CM1;
	uint16_t LD_C0; 
	uint16_t LD_CP; 

    uint16_t LD_VGA;
	uint16_t LD_EDC;
	uint16_t LD_EQR;

	uint16_t FEC_CORRECTION;
	uint16_t FEC_UNCORRECTION;

	uint16_t CURR_EYE_HT;
	uint16_t TRAINING_TIME_MS; 
} KR_Summary_Log;

static const char *enum_irq_string[] = {
	"AN_RATE",
	"GEN1_DONE",
	"GEN0_DONE",
	"INCP_LINK",
	"NP_RX",
	"NP_FAIL",
	"ACK_FAIL",
	"ABD_FAIL",
	"LINK_FAIL",
	"AN_GOOD_IRQ",
	"CMPL_ACK",
	"AN_RATE_DET",
	"AN_TRAIN",
	"AN_XMIT_DISABLE",
	"DME_VIOL_1",
	"DME_VIOL_0",
	"FRLOCK_1",
	"FRLOCK_0",
	"REM_RDY_1",
	"REM_RDY_0",
	"BER_BUSY_1",
	"BER_BUSY_0",
	"MW_DONE",
	"WT_DONE",
	"LPCVALID",
	"LPSVALID",
	"KR_ACTV",
	"ACK_FIN",
	"NP_REQ",
};

static const char *enum_state_machine_string[] = {
	"AN_ENABLE",
	"XMT_DISABLE",
	"ABILITY_DET",
	"ACK_DET",
	"COMPLETE_ACK",
	"TRAIN",
	"AN_GOOD_CHK",
	"AN_GOOD",
	"RATE_DET",
	"",
	"",
	"LINK_STAT_CHK",
	"PARLL_DET_FAULT",
	"WAIT_RATE_DONE",
	"NXTPG_WAIT",
	"WAIT_FW_RES",
};


