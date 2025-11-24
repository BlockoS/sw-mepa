// Copyright (c) 2004-2020 Microchip Technology Inc. and its subsidiaries.
// SPDX-License-Identifier: MIT

#include <stdio.h>
#include <unistd.h>
#include <ctype.h>
#include <inttypes.h>
#include "microchip/ethernet/switch/api.h"
#include "microchip/ethernet/board/api.h"
#include "main.h"
#include "trace.h"
#include "port.h"
#include "cli.h"
#include "phy_demo_apps.h"
#include "mepa_os_linux.h"
#include <arpa/inet.h>

static meba_inst_t meba_macsec_rollover_instance;

static mscc_appl_trace_module_t trace_module = {
    .name = "macsec-rollover-demo"
};

enum {
    TRACE_GROUP_DEFAULT,
    TRACE_GROUP_CNT
};

static mscc_appl_trace_group_t trace_groups[TRACE_GROUP_CNT] = {
    {
        .name = "default",
        .level = MESA_TRACE_LEVEL_ERROR
    },
};

#define  RETIRE_DELAY_MS               (6000)   // 6 seconds retireDelay
#define  MACSEC_NON_XPN_CIPHER_SUIT    (0)
#define  MACSEC_XPN_CIPHER_SUIT        (1)
#define  MACSEC_SECY_ID                (3)
#define  MAX_ROLLOVER_LOGS             (128)

static uint8_t cipher_suit = MACSEC_NON_XPN_CIPHER_SUIT;

static uint64_t delete_schedule_time[4] = {0, 0, 0, 0}; // timestamps when old ANs can be deleted
static uint64_t rx_delete_schedule_time[4] = {0, 0, 0, 0}; // timestamps when old ANs can be deleted

static mepa_bool_t sa_created[4] = {FALSE, FALSE, FALSE, FALSE};
static mepa_bool_t rx_sa_created[4] = {FALSE, FALSE, FALSE, FALSE};

static uint8_t rollover_direction_egress = 0;
static uint64_t rollover_threshold = 0;
static uint64_t old_tx_pn = 0;

static uint8_t  rollover_egress_config_port_cnt = 0;
static uint8_t  rollover_ingress_config_port_cnt = 0;
static uint8_t  rollover_egress_port_no = 0;
static uint8_t  rollover_ingress_port_no = 0;
static uint8_t  rollover_egress_cipher_suit = MACSEC_NON_XPN_CIPHER_SUIT;
static uint8_t  rollover_ingress_cipher_suit = MACSEC_NON_XPN_CIPHER_SUIT;
static uint8_t macsec_tx_sa_an_activated = 0;
static uint8_t rx_sa_an_activated = 0;

static const mepa_mac_t peer_macaddress = { .addr = {0xff, 0xff, 0xff, 0xff, 0xff, 0xf1}};
static const mepa_mac_t mku_mac_address = { .addr = {0x01, 0x80, 0xC2, 0x00, 0x00, 0x03}};

static uint8_t aes_key_256[4][32] = {
    {
        74, 222, 11, 46, 177, 29, 199, 212, 39, 237, 18, 224, 133, 129, 139, 163,
        117, 155, 113, 46, 179, 233, 205, 85, 249, 34, 181, 243, 173, 238, 75, 198
    },
    {
        13, 185, 118, 116, 92, 59, 149, 183, 96, 213, 111, 118, 217, 171, 166, 146,
        159, 90, 255, 47, 151, 116, 207, 48, 207, 92, 112, 181, 148, 157, 106, 121
    },
    {
        180, 44, 216, 75, 15, 203, 249, 244, 4, 44, 65, 82, 37, 175, 125, 163,
        3, 7, 87, 254, 255, 13, 155, 120, 61, 239, 110, 140, 93, 156, 231, 170
    },
    {
        45, 67, 98, 100, 19, 198, 142, 243, 49, 75, 223, 139, 65, 20, 226, 43,
        253, 168, 173, 213, 49, 224, 225, 209, 224, 235, 44, 75, 227, 199, 124, 134
    }
};

static uint8_t hash_key_128[4][16] = {
    {
        220, 174, 108, 119, 252, 109, 128, 255, 84, 13, 206, 234, 205, 26, 138, 52
    },
    {
        152, 153, 43, 84, 244, 119, 93, 138, 182, 189, 182, 181, 72, 230, 32, 123
    },
    {
        194, 142, 24, 160, 25, 1, 234, 67, 73, 164, 240, 235, 226, 166, 210, 173
    },
    {
        23, 204, 86, 119, 6, 255, 57, 9, 170, 158, 144, 186, 159, 142, 11, 53
    }
};

static uint8_t macsec_salts[4][12] = {
    {0xA1, 0xB2, 0xC3, 0xD4, 0xE5, 0xF6, 0x01, 0x12, 0x23, 0x34, 0x45, 0x56},
    {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC},
    {0xFF, 0xEE, 0xDD, 0xCC, 0xBB, 0xAA, 0x99, 0x88, 0x77, 0x66, 0x55, 0x44},
    {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0, 0x0F, 0x1E, 0x2D, 0x3C}
};


static uint8_t ssci_array[4][4] = {
    {0x00, 0x00, 0xFF, 0x01}, // SSCI_0
    {0x12, 0x34, 0x56, 0x78}, // SSCI_1
    {0x00, 0x00, 0x01, 0xFF}, // SSCI_2
    {0xAB, 0xCD, 0xEF, 0xFF}  // SSCI_3
};

typedef enum {
    MACSEC_ROLLOVER_TX = 0,
    MACSEC_ROLLOVER_RX = 1
} macsec_rollover_dir_t;

typedef struct {
    uint64_t               timestamp_ms;
    uint64_t               pn_value;
    uint8_t                old_an;
    uint8_t                new_an;
    uint8_t                port_no;
    macsec_rollover_dir_t  dir;
    uint64_t               out_pkts_encrypted;
} macsec_rollover_log_t;

static macsec_rollover_log_t rollover_log[MAX_ROLLOVER_LOGS];
static uint32_t rollover_log_write_idx = 0;
static uint32_t rollover_log_count = 0;


static inline void store_rollover_log(uint8_t port_no,
                                      uint8_t old_an,
                                      uint8_t new_an,
                                      uint64_t pn,
                                      macsec_rollover_dir_t dir,
                                      uint64_t out_pkts_encrypted)
{
    uint32_t idx = rollover_log_write_idx;

    rollover_log[idx].timestamp_ms = MEPA_UPTIME_MSECONDS();
    rollover_log[idx].pn_value     = pn;
    rollover_log[idx].old_an       = old_an;
    rollover_log[idx].new_an       = new_an;
    rollover_log[idx].port_no      = port_no + 1;
    rollover_log[idx].dir          = dir;
    rollover_log[idx].out_pkts_encrypted = out_pkts_encrypted;

    rollover_log_write_idx = (rollover_log_write_idx + 1) % MAX_ROLLOVER_LOGS;

    if (rollover_log_count < MAX_ROLLOVER_LOGS)
        rollover_log_count++;
}


int send_mkpdu(meba_inst_t inst, mepa_port_no_t port_no, uint32_t active_an, uint64_t latest_pn)
{
    uint8_t frame_send[512];
    uint32_t frame_length = 0;
    mesa_packet_tx_info_t tx_info;

    // --------------------------------------------------------------------
    // 1. Ethernet Header
    // --------------------------------------------------------------------
    uint8_t dst_mac[6] = {0x01, 0x80, 0xC2, 0x00, 0x00, 0x03};
    uint8_t src_mac[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x03};
    uint16_t ethertype  = htons(0x888e);

    memcpy(frame_send + frame_length, dst_mac, sizeof(dst_mac));
    frame_length += 6;

    memcpy(frame_send + frame_length, src_mac, sizeof(src_mac));
    frame_length += 6;

    memcpy(frame_send + frame_length, &ethertype, 2);
    frame_length += 2;

    // --------------------------------------------------------------------
    // 2. MKPDU Payload (Version + Basic Parameter Set)
    // --------------------------------------------------------------------
    frame_send[frame_length++] = 0x01;  // Version 1
    frame_send[frame_length++] = 0x01;  // Body Type = 1 (Basic Parameter Set)
    frame_send[frame_length++] = 0x00;  // Length MSB
    frame_send[frame_length++] = 0x22;  // Length LSB (example 32 bytes)

    uint8_t body_data[32] = {0};

    // SCI - can use local MAC + port number
	memcpy(body_data, peer_macaddress.addr, 6);
    body_data[6] = 0x00;               // Port ID MSB (example)
    body_data[7] = MACSEC_SECY_ID & 0xFF;     // Port ID LSB

    body_data[8]  = 0x00; // Priority
    body_data[9]  = 0x00; // Key-server flag
    body_data[10] = 0x01; // MACsec desired
    body_data[11] = 0x02; // Capability
	
    // Latest Key Number (AN) -- this is important
    body_data[12] = (uint8_t)active_an;

    // Latest PN (packet number) - 8 bytes for XPN
    body_data[13] = (latest_pn >> 56) & 0xFF;
    body_data[14] = (latest_pn >> 48) & 0xFF;
    body_data[15] = (latest_pn >> 40) & 0xFF;
    body_data[16] = (latest_pn >> 32) & 0xFF;
    body_data[17] = (latest_pn >> 24) & 0xFF;
    body_data[18] = (latest_pn >> 16) & 0xFF;
    body_data[19] = (latest_pn >> 8)  & 0xFF;
    body_data[20] = latest_pn & 0xFF;

    // Remaining bytes padded with 0
    memset(body_data + 21, 0x00, 32 - 21);

    memcpy(frame_send + frame_length, body_data, sizeof(body_data));
    frame_length += sizeof(body_data);

    // --------------------------------------------------------------------
    // 3. Initialize TX info and send
    // --------------------------------------------------------------------
    mesa_packet_tx_info_init(NULL, &tx_info);
    tx_info.dst_port_mask = (1 << port_no);
    tx_info.dst_port = port_no;

    if (mesa_packet_tx_frame(NULL, &tx_info, frame_send, frame_length) != MESA_RC_OK) {
        T_E("Error sending MKPDU on port %d\n", (port_no + 1));
        return -1;
    }

    T_D("[DEBUG] MKPDU frame sent successfully (length=%u bytes) from port : %d\n", frame_length, (port_no + 1));
    return 0;
}

static void cli_cmd_macsec_rollover_demo(cli_req_t *req)
{

    mepa_rc rc = MEPA_RC_OK;

    uint8_t port = req->port_no + 1;

    if (rollover_direction_egress) {
        cli_printf("Configuring Egress Rollover on port : %d\n", port);
        if (rollover_egress_config_port_cnt == 1) {
            T_E("[ERROR] Egress MACsec rollover already configured on port %d\n", rollover_egress_port_no);
            return;
        }
    } else {
        cli_printf("Configuring Ingress Rollover on Port : %d\n", port);
        if (rollover_ingress_config_port_cnt == 1) {
            T_E("[ERROR] Ingress MACsec rollover already configured on port %d\n", rollover_ingress_port_no);
            return;
        }
    }

    /* Re-Init : Application Old Statistics Clear */
    memset(delete_schedule_time, 0, sizeof(delete_schedule_time));
    memset(rx_delete_schedule_time, 0, sizeof(rx_delete_schedule_time));
    memset(sa_created, 0, sizeof(sa_created));
    memset(rx_sa_created, 0, sizeof(rx_sa_created));
    macsec_tx_sa_an_activated = 0;
    rx_sa_an_activated = 0;
    old_tx_pn = 0;
    rollover_log_count = 0;


    /* Enable MAC Block on the Port */
    mepa_macsec_init_t init_data_enable = { .enable = 1,
                                            .dis_ing_nm_macsec_en = 1,
                                            .mac_conf.lmac.dis_length_validate = 0,
                                            .mac_conf.hmac.dis_length_validate = 0,
                                            .bypass = MEPA_MACSEC_INIT_BYPASS_NONE
                                          };

    if ((rc = mepa_macsec_init_set(meba_macsec_rollover_instance->phy_devices[req->port_no], &init_data_enable)) != MEPA_RC_OK) {
        T_E("[ERROR] Enabling MACsec failed on port %d\n", port);
        return;
    }

    /* Default action configuration for non-matching frames */
    mepa_macsec_default_action_policy_t default_action_policy = {
        .ingress_non_control_and_non_macsec = MEPA_MACSEC_DEFAULT_ACTION_BYPASS,
        .ingress_control_and_non_macsec     = MEPA_MACSEC_DEFAULT_ACTION_BYPASS,
        .ingress_non_control_and_macsec     = MEPA_MACSEC_DEFAULT_ACTION_BYPASS,
        .ingress_control_and_macsec         = MEPA_MACSEC_DEFAULT_ACTION_BYPASS,
        .egress_control                     = MEPA_MACSEC_DEFAULT_ACTION_BYPASS,
        .egress_non_control                 = MEPA_MACSEC_DEFAULT_ACTION_BYPASS,
    };

    if ((rc = mepa_macsec_default_action_set(meba_macsec_rollover_instance->phy_devices[req->port_no], req->port_no, &default_action_policy)) != MEPA_RC_OK) {
        T_E("[ERROR] Default action configuration failed on port %d\n", port);
        return;
    }

     mepa_macsec_port_t macsec_port = {
        .port_no = req->port_no,
        .port_id = MACSEC_SECY_ID,
        .service_id = 0
    };

    mepa_macsec_sci_t sci = { .mac_addr = peer_macaddress, .port_id = MACSEC_SECY_ID };

     // Create SecY
    mepa_macsec_secy_conf_t secy_conf = {
        .validate_frames        = MEPA_MACSEC_VALIDATE_FRAMES_STRICT,
        .replay_protect         = TRUE,
        .replay_window          = 10,
        .protect_frames         = TRUE,
        .always_include_sci     = TRUE,
        .use_es                 = FALSE,
        .use_scb                = FALSE,
        .confidentiality_offset = 0,
        .mac_addr               = peer_macaddress,
        .current_cipher_suite   = MEPA_MACSEC_CIPHER_SUITE_GCM_AES_256
    };

    if (cipher_suit == MACSEC_XPN_CIPHER_SUIT) {
        secy_conf.current_cipher_suite = MEPA_MACSEC_CIPHER_SUITE_GCM_AES_XPN_256;
    }

    if ((rc = mepa_macsec_secy_conf_add(meba_macsec_rollover_instance->phy_devices[req->port_no], macsec_port, &secy_conf)) != MEPA_RC_OK) {
        T_E("[ERROR] SecY creation failed on port %d\n", port);
        return;
    }

    // Pattern matching
    mepa_macsec_match_pattern_t pattern_match = {
        .priority            = MEPA_MACSEC_MATCH_PRIORITY_HIGH,
        .match               = MEPA_MACSEC_MATCH_ETYPE,
        .is_control          = TRUE,
        .has_vlan_tag        = FALSE,
        .has_vlan_inner_tag  = FALSE,
        .etype               = rollover_direction_egress ? 0x800 : 0x88e5
    };

    if ((rc = mepa_macsec_pattern_set(meba_macsec_rollover_instance->phy_devices[req->port_no], macsec_port, rollover_direction_egress ? MEPA_MACSEC_DIRECTION_EGRESS : MEPA_MACSEC_DIRECTION_INGRESS,
                                      MEPA_MACSEC_MATCH_ACTION_CONTROLLED_PORT, &pattern_match)) != MEPA_RC_OK) {
        T_E("[ERROR] Pattern matching failed on port %d\n", port);
        return;
    }

    /* SecY Control port enable */
    if ((rc = mepa_macsec_secy_controlled_set(meba_macsec_rollover_instance->phy_devices[req->port_no], macsec_port, TRUE)) != MEPA_RC_OK) {
        T_E("[ERROR] Controlled port enable failed on port %d\n", port);
        return;
    }

    if (rollover_direction_egress) {
        /* Tx Secure channel Creation */
        if ((rc = mepa_macsec_tx_sc_set(meba_macsec_rollover_instance->phy_devices[req->port_no], macsec_port)) != MEPA_RC_OK) {
            T_E("[ERROR] Tx Secure Channel creation failed on port %d\n", port);
            return;
        }
    } else {
        /* Rx Secure channel Creation */
        if ((rc = mepa_macsec_rx_sc_add(meba_macsec_rollover_instance->phy_devices[req->port_no], macsec_port, &sci)) != MEPA_RC_OK) {
            T_E("[ERROR] Rx Secure Channel creation failed on port %d\n", port);
            return;
        }
    }

    /* Configure Paacket with Ethertype as 0x888E and DMAC address as "0x01:0x80:0xC2:0x00:0x00:0x03" as Control packet - MKPDU packet*/
    mepa_macsec_control_frame_match_conf_t conf;
    memcpy(&conf.dmac, mku_mac_address.addr, 6);
    conf.etype = 0x888e;
    conf.match = MEPA_MACSEC_MATCH_DMAC | MEPA_MACSEC_MATCH_ETYPE;
    if ((rc = mepa_macsec_control_frame_match_conf_set(meba_macsec_rollover_instance->phy_devices[req->port_no], req->port_no, &conf, NULL)) != MEPA_RC_OK) {
        T_E("[ERROR] Control frame match config failed on port %d\n", port);
        return;
    }

    // Create and activate SA
    mepa_macsec_pkt_num_t next_pn = { .pn = 1, .xpn = 1 };

    for (uint8_t i = 0; i < 2; i++) {
        mepa_macsec_sak_t sak = {0};
        memcpy(sak.buf, aes_key_256[i], 32);
        memcpy(sak.h_buf, hash_key_128[i], 16);
        sak.len = 32;
        mepa_macsec_ssci_t ssci = {0};

        if (cipher_suit == MACSEC_XPN_CIPHER_SUIT) {
            memcpy(&ssci, ssci_array[0], sizeof(mepa_macsec_ssci_t));
            memcpy(sak.salt.buf, macsec_salts[i], sizeof(mepa_macsec_salt_t));
        }

        if (rollover_direction_egress) {
            if (cipher_suit == MACSEC_XPN_CIPHER_SUIT) {
                /* TX SA Create */
                if ((rc = mepa_macsec_tx_seca_set(meba_macsec_rollover_instance->phy_devices[req->port_no], macsec_port, i, next_pn, TRUE, &sak, &ssci)) != MEPA_RC_OK) {
                    T_E("[ERROR] Tx XPN SA creation failed on port %d\n", port);
                    return;
                }
                rollover_egress_cipher_suit = MACSEC_XPN_CIPHER_SUIT;
            } else {
                /* TX SA Create */
                if ((rc = mepa_macsec_tx_sa_set(meba_macsec_rollover_instance->phy_devices[req->port_no], macsec_port, i, next_pn.pn, TRUE, &sak)) != MEPA_RC_OK) {
                    T_E("[ERROR] Tx NON-XPN SA creation failed on port %d\n", port);
                    return;
                }
                rollover_egress_cipher_suit = MACSEC_NON_XPN_CIPHER_SUIT;
            }
            sa_created[i] = TRUE;
        } else {
            if (cipher_suit == MACSEC_XPN_CIPHER_SUIT) {
                /* RX SA Create */
                if ((rc = mepa_macsec_rx_seca_set(meba_macsec_rollover_instance->phy_devices[req->port_no], macsec_port, &sci, i, next_pn, &sak, &ssci)) != MEPA_RC_OK) {
                    T_E("[ERROR] Rx XPN SA creation failed on port %d\n", port);
                    return;
                }
                rollover_ingress_cipher_suit = MACSEC_XPN_CIPHER_SUIT;
            } else {
                /* RX SA Create */
                if ((rc = mepa_macsec_rx_sa_set(meba_macsec_rollover_instance->phy_devices[req->port_no], macsec_port, &sci, i, next_pn.pn, &sak)) != MEPA_RC_OK) {
                    T_E("[ERROR] Rx NON-XPN SA creation failed on port %d\n", port);
                    return;
                }
                rollover_ingress_cipher_suit = MACSEC_NON_XPN_CIPHER_SUIT;
            }
			rx_sa_created[i] = TRUE;
        }
    }

    if (rollover_direction_egress) {
        /* Activate TX SA, AN= 0 */
        if ((rc = mepa_macsec_tx_sa_activate(meba_macsec_rollover_instance->phy_devices[req->port_no], macsec_port, 0)) != MEPA_RC_OK) {
            T_E("[ERROR] Tx SA activation failed on port %d\n", port);
            return;
        }
        macsec_tx_sa_an_activated = 0;
        rollover_egress_config_port_cnt++;
        rollover_egress_port_no = req->port_no;
    } else {
       /* Activate RX SA,  AN = 0 */
       if ((rc = mepa_macsec_rx_sa_activate(meba_macsec_rollover_instance->phy_devices[req->port_no], macsec_port, &sci, 0)) != MEPA_RC_OK) {
            T_E("[ERROR] Rx SA activation failed on port %d\n", port);
            return;
        }
        rx_sa_an_activated = 0;
        rollover_ingress_config_port_cnt++;
        rollover_ingress_port_no = req->port_no;
    }

    if (cipher_suit == MACSEC_XPN_CIPHER_SUIT) {
        rollover_threshold = 0x1000000000ULL;
#if 0
        /* Configure Sequence Threshold value */
        if ((rc = mepa_macsec_event_xpn_seq_threshold_set(meba_macsec_rollover_instance->phy_devices[req->port_no], req->port_no, rollover_threshold)) != MEPA_RC_OK) {
            T_E("[ERROR] Sequence threshold config failed on port %d\n", port);
            return;
        }
#endif
    } else {
        uint32_t non_xpn_threshold = 0xFFF00000UL;
        rollover_threshold = non_xpn_threshold;
#if 0
        /* Configure Sequence Threshold value */
        if ((rc = mepa_macsec_event_seq_threshold_set(meba_macsec_rollover_instance->phy_devices[req->port_no], req->port_no, non_xpn_threshold)) != MEPA_RC_OK) {
            T_E("[ERROR] Sequence threshold config failed on port %d\n", port);
            return;
        }
#endif
    }

    cli_printf("[INFO] rollover_threshold = 0x%llx (%llu)\n", (unsigned long long)rollover_threshold, (unsigned long long)rollover_threshold);
    T_D("[DEBUG] rollover_threshold = 0x%llx (%llu)\n", (unsigned long long)rollover_threshold, (unsigned long long)rollover_threshold);

#if 0
    /* Enable Sequence Threshold Interupt */
    if ((rc = mepa_macsec_event_enable_set(meba_macsec_rollover_instance->phy_devices[req->port_no], req->port_no, MEPA_MACSEC_SEQ_THRESHOLD_EVENT, TRUE)) != MEPA_RC_OK) {
        T_E("[ERROR] MACsec event enable failed on port %d\n", port);
        return;
    }
#endif

    cli_printf("\n .....[SUCCESS] MACsec rollover demo configured successfully on port %u ....\n", port);
    return;
}

static int inline rx_mkpdu_packet_poll(mepa_port_no_t port_no, int *an_to_activate, uint64_t *packet_pn)
{
    uint8_t frame[1600];
    mesa_packet_rx_info_t rx_info = {0};
    mesa_port_no_t iport = MESA_PORT_NO_NONE;

    // Check for Packet received to CPU
    mesa_rc rc = mesa_packet_rx_frame(NULL, frame, sizeof(frame), &rx_info);
    if (rc != MESA_RC_OK) {
        // No packet received
        return 0;
    }
    T_D("[DEBUG] Received frame on CPU of length %u on port %d\n", rx_info.length, (rx_info.port_no + 1));

    // Get ingress port
    iport = rx_info.port_no;
    if (iport != port_no) {
        return 0;
    }
    uint64_t latest_pn = 0;

    if (rx_info.length >= 14 + 4) { // Ethernet header + minimal payload
        uint16_t ethertype = (frame[12] << 8) | frame[13];

        if (ethertype == 0x888e) { // MKA Ethertype
            T_D("Received MKPDU on port %d\n", (iport + 1));

            // Extract MKPDU fields, e.g., AN number
            *an_to_activate = frame[30]; // Example: latest AN field in MKPDU
            T_D("Peer active AN = %d\n", *an_to_activate);

            for (int i = 0; i < 8; i++) {
                latest_pn = (latest_pn << 8) | frame[31 + i];
            }
            *packet_pn = latest_pn;
            T_D("Peer latest PN = %llu\n", latest_pn);
            return 1;
        }
    }
    return 0;
}


static inline int mepa_egress_key_rollover(mepa_device_t *dev, mepa_port_no_t port_no)
{
    mepa_macsec_port_t macsec_port = {0};
    macsec_port.port_no = port_no;
    macsec_port.port_id = MACSEC_SECY_ID;
    macsec_port.service_id = 0;

    mepa_rc rc;
    mepa_macsec_tx_sa_status_t status = {0};

    uint32_t active_an = macsec_tx_sa_an_activated;

    //----------------------------------------------------------------------
    // STEP 1: Read Current PN for Active AN
    //----------------------------------------------------------------------
    rc = mepa_macsec_tx_sa_status_get(dev, macsec_port, active_an, &status);
    if (rc != MEPA_RC_OK) {
        T_E("[ERROR] Failed to get Tx SA status on port %u\n", port_no + 1);
        return 1;
    }

    uint64_t pn = 0;

    if (rollover_egress_cipher_suit == MACSEC_XPN_CIPHER_SUIT) {
        pn = status.pn_status.next_pn.xpn;
    } else {
        pn = (uint64_t)status.pn_status.next_pn.pn;
    }

    // Not crossed threshold -> nothing to do
    if (pn <= rollover_threshold)
        goto deletion_check;

    // Prevent repeated triggers
    if (pn == old_tx_pn)
        goto deletion_check;

    old_tx_pn = pn;

    T_D("[DEBUG] TX PN crossed threshold: PN=%llu threshold=%llu AN=%u\n", pn, rollover_threshold, active_an);

    //----------------------------------------------------------------------
    // STEP 2: Compute the AN transitions
    //----------------------------------------------------------------------
    uint32_t new_an     = (active_an + 1) & 0x3;
    uint32_t precreate  = (new_an + 1) & 0x3;
    uint32_t delete_an  = (active_an + 3) & 0x3; /* Example : Delete AN=0 after activating AN=2 */

    send_mkpdu(meba_macsec_rollover_instance, port_no, new_an, pn);
    T_D("[DEBUG]  Sent MKPDU: new AN=%u PN=%llu\n", new_an, pn);

    MEPA_MSLEEP(1);

    //----------------------------------------------------------------------
    // STEP 3: Activate the new SA
    //----------------------------------------------------------------------
    rc = mepa_macsec_tx_sa_activate(dev, macsec_port, new_an);
    if (rc != MEPA_RC_OK) {
        T_E("[ERROR] Failed activating TX SA AN=%u on port %u\n", new_an, port_no + 1);
        return 1;
    }
    T_D("[DEBUG] Activated TX SA AN=%u on port %u\n", new_an, port_no + 1);

    mepa_macsec_tx_sa_counters_t counters = {0};

    if ((rc = mepa_macsec_tx_sa_counters_get(dev, macsec_port, macsec_tx_sa_an_activated, &counters)) != MEPA_RC_OK) {
        T_E("\n Error in getting the Transmit Secure Association Statistics on port : %d \n", (port_no + 1));
        return 1;
    }

    macsec_tx_sa_an_activated = new_an;

    store_rollover_log(port_no, active_an, new_an, pn, MACSEC_ROLLOVER_TX, counters.out_pkts_encrypted);

    //----------------------------------------------------------------------
    // STEP 4: Pre-create the next SA
    //----------------------------------------------------------------------
    if (!sa_created[precreate]) {

        mepa_macsec_sak_t sak = {0};
        memcpy(sak.buf, aes_key_256[precreate], 32);
        memcpy(sak.h_buf, hash_key_128[precreate], 16);
        sak.len = 32;
        mepa_macsec_ssci_t ssci = {0};

        if (rollover_egress_cipher_suit == MACSEC_XPN_CIPHER_SUIT) {
            memcpy(&ssci, ssci_array[precreate], sizeof(mepa_macsec_ssci_t));
            memcpy(sak.salt.buf, macsec_salts[precreate], sizeof(mepa_macsec_salt_t));
            mepa_macsec_pkt_num_t next_pn = { .pn = 1, .xpn = 1 };
            /* TX SA Create */
            if ((rc = mepa_macsec_tx_seca_set(dev, macsec_port, precreate, next_pn, TRUE, &sak, &ssci)) != MEPA_RC_OK) {
                T_E("[ERROR] Tx XPN SA creation failed on port %d\n", port_no + 1);
                return 1;
            }
        } else {
            rc = mepa_macsec_tx_sa_set(dev, macsec_port, precreate, 1, TRUE, &sak);
        }
        if (rc == MEPA_RC_OK) {
            sa_created[precreate] = TRUE;
            T_D("[DEBUG] Pre-created TX SA AN=%u on port %u\n", precreate, port_no + 1);
        }
    }

    //----------------------------------------------------------------------
    // STEP 5: Schedule deletion time for the oldest SA
    //----------------------------------------------------------------------
    if (sa_created[delete_an] && delete_an != new_an) {
        delete_schedule_time[delete_an] = MEPA_UPTIME_MSECONDS() + RETIRE_DELAY_MS;
        T_D("[DEBUG] Scheduled deletion of TX SA AN=%u on port %u\n", delete_an, port_no + 1);
    }

deletion_check:

    uint64_t now = MEPA_UPTIME_MSECONDS();

    //----------------------------------------------------------------------
    // STEP 6: Time-based deletion
    //----------------------------------------------------------------------
    for (int an = 0; an < 4; an++) {

        if (!sa_created[an])
            continue;

        if (delete_schedule_time[an] == 0)
            continue;

        if (an == macsec_tx_sa_an_activated)
            continue;

        if (now < delete_schedule_time[an])
            continue;

        //--------------------------------------------------------------
        // Delete SA when timer expires
        //--------------------------------------------------------------
        rc = mepa_macsec_tx_sa_del(dev, macsec_port, an);
        if (rc == MEPA_RC_OK) {
            T_D("[DEBUG] Deleted old TX SA AN=%d on port %u\n", an, port_no + 1);
            sa_created[an] = FALSE;
            delete_schedule_time[an] = 0;
        } else {
            T_E("[ERROR] Failed to delete TX SA AN=%d on port %u\n", an, port_no + 1);
        }
    }
    return 0;
}


static inline int mepa_ingress_key_rollover(mepa_device_t *dev, mepa_port_no_t port_no)
{
    mepa_rc rc;
    int peer_an;
    uint64_t packet_pn = 0;

    mepa_macsec_port_t macsec_port = {
        .port_no = port_no,
        .port_id = MACSEC_SECY_ID,
        .service_id = 0
    };

    mepa_macsec_sci_t sci = { .mac_addr = peer_macaddress, .port_id = MACSEC_SECY_ID };

    // ------------------------------------------------------------
    // STEP 1: Poll MKPDU from peer
    // ------------------------------------------------------------
    int received = rx_mkpdu_packet_poll(port_no, &peer_an, &packet_pn);
    if (!received) {
        // No MKPDU received: delete expired ANs
        uint32_t now = MEPA_UPTIME_MSECONDS();
        for (int an = 0; an < 4; an++) {
            if (rx_sa_created[an] && rx_delete_schedule_time[an] && now >= rx_delete_schedule_time[an]) {
                rc = mepa_macsec_rx_sa_del(dev, macsec_port, &sci, an);
                if (rc == MEPA_RC_OK) {
                    rx_sa_created[an] = FALSE;
                    rx_delete_schedule_time[an] = 0;
                    T_D("[DEBUG] Deleted old RX SA AN=%d on port %u\n", an, port_no + 1);
                }
            }
        }
        return 0;
    }

    uint32_t new_an = peer_an;
    uint32_t precreate = (new_an + 1) & 0x3;
    uint32_t delete_an = (new_an + 3) & 0x3;

    T_D("[DEBUG] Received MKPDU: peer AN=%u on port %u\n", new_an, port_no + 1);

    mepa_macsec_sak_t sak = {0};
    memcpy(sak.buf, aes_key_256[new_an], 32);
    memcpy(sak.h_buf, hash_key_128[new_an], 16);
    sak.len = 32;
    // ------------------------------------------------------------
    // STEP 2: ACTIVATE RX SA immediately
    // ------------------------------------------------------------

    rc = mepa_macsec_rx_sa_activate(dev, macsec_port, &sci, new_an);
    if (rc != MEPA_RC_OK) {
        T_E("[ERROR] Failed activating RX SA AN=%u on port %u\n", new_an, port_no + 1);
        return 1;
    }
    rx_sa_an_activated = new_an;
    T_D("[DEBUG] Activated RX SA AN=%u on port %u\n", new_an, port_no + 1);

    store_rollover_log(port_no, delete_an, new_an, packet_pn, MACSEC_ROLLOVER_RX, 0);
    // ------------------------------------------------------------
    // STEP 3: Pre-create next AN
    // ------------------------------------------------------------

    memcpy(sak.buf, aes_key_256[precreate], 32);
    memcpy(sak.h_buf, hash_key_128[precreate], 16);

    if (!rx_sa_created[precreate]) {

        if (rollover_ingress_cipher_suit == MACSEC_XPN_CIPHER_SUIT) {
            mepa_macsec_ssci_t ssci = {0};
            memcpy(&ssci, ssci_array[precreate], sizeof(mepa_macsec_ssci_t));
            memcpy(sak.salt.buf, macsec_salts[precreate], sizeof(mepa_macsec_salt_t));
            mepa_macsec_pkt_num_t next_pn = { .pn = 1, .xpn = 1 };

            if ((rc = mepa_macsec_rx_seca_set(dev, macsec_port, &sci, precreate, next_pn, &sak, &ssci)) != MEPA_RC_OK) {
                T_E("[ERROR] Rx XPN SA creation failed on port %d\n", port_no + 1);
                return 1;
            }
        } else {
            rc = mepa_macsec_rx_sa_set(dev, macsec_port, &sci, precreate, 1, &sak);
        }
        if (rc == MEPA_RC_OK) {
            rx_sa_created[precreate] = TRUE;
        }
    }

    T_D("[DEBUG] Pre created Next RX AN=%u on port %u\n", precreate, port_no + 1);

    // ------------------------------------------------------------
    // STEP 4: Schedule deletion of old AN (low priority)
    // ------------------------------------------------------------
    if (rx_sa_created[delete_an]) {
        rx_delete_schedule_time[delete_an] = MEPA_UPTIME_MSECONDS() + RETIRE_DELAY_MS;
        T_D("[DEBUG] Scheduled deletion RX AN=%u on port %u\n", delete_an, port_no + 1);
    }

    // ------------------------------------------------------------
    // STEP 5: Delete expired ANs (low priority)
    // ------------------------------------------------------------
    uint32_t now = MEPA_UPTIME_MSECONDS();
    for (int an = 0; an < 4; an++) {
        if (rx_sa_created[an] && rx_delete_schedule_time[an] && now >= rx_delete_schedule_time[an]) {
            rc = mepa_macsec_rx_sa_del(dev, macsec_port, &sci, an);
            if (rc == MEPA_RC_OK) {
                rx_sa_created[an] = FALSE;
                rx_delete_schedule_time[an] = 0;
                T_D("[DEBUG] Deleted old RX SA AN=%d on port %u\n", an, port_no + 1);
            }
        }
    }
    return 0;
}

static void cli_cmd_macsec_rollover_log_print(cli_req_t *req)
{
    mepa_macsec_port_t macsec_port = {0};
    macsec_port.port_id = MACSEC_SECY_ID;
    macsec_port.service_id = 0;

    mepa_rc rc;

    if (rollover_egress_config_port_cnt == 1) {
        macsec_port.port_no = rollover_egress_port_no;
        mepa_macsec_tx_sa_counters_t counters = {0};

        if ((rc = mepa_macsec_tx_sa_counters_get(meba_macsec_rollover_instance->phy_devices[rollover_egress_port_no], macsec_port, macsec_tx_sa_an_activated, &counters)) != MEPA_RC_OK) {
            T_E("Error getting TX SA counters on port %d\n", rollover_egress_port_no + 1);
            return;
        }

        cli_printf("\n================ MACsec TX SA Status ================\n");
        cli_printf("Active AN              : %d\n", macsec_tx_sa_an_activated);
        cli_printf("Packets Encrypted      : %llu\n", counters.out_pkts_encrypted);
    }

    if (rollover_ingress_config_port_cnt == 1) {
        cli_printf("\n================ MACsec RX SA Status ================\n");
        cli_printf("Active AN              : %d\n", rx_sa_an_activated);
    }

    cli_printf("\n================ MACsec Rollover History ================\n");

    if (rollover_log_count == 0) {
        cli_printf(" No rollover events recorded.\n");
        return;
    }	
    uint32_t idx = (rollover_log_write_idx + MAX_ROLLOVER_LOGS - rollover_log_count) % MAX_ROLLOVER_LOGS;
    unsigned long long seconds = 0;
    unsigned long long millis = 0;

    for (uint32_t i = 0; i < rollover_log_count; i++) {
        macsec_rollover_log_t *e = &rollover_log[(idx + i) % MAX_ROLLOVER_LOGS];

        seconds = e->timestamp_ms / 1000;
        millis  = e->timestamp_ms % 1000;

        const char *encrypted_pkts_str;

        if (e->dir == MACSEC_ROLLOVER_TX) {
            static char buf[32];
            snprintf(buf, sizeof(buf), "%" PRIu64, e->out_pkts_encrypted);
            encrypted_pkts_str = buf;
        } else {
            encrypted_pkts_str = "N/A";
        }
        cli_printf("[%03u] Time=%llu.%03llu s | Port=%u | %s | AN %u -> %u | PN=%llu | OutPktsEnc=%s\n",
           i,
           seconds,
           millis,
           e->port_no,
           (e->dir == MACSEC_ROLLOVER_TX) ? "TX" : "RX",
           e->old_an,
           e->new_an,
           e->pn_value,
           encrypted_pkts_str);

    }
}

static void cli_cmd_macsec_rollover_demo_disable(cli_req_t *req)
{
    mepa_rc rc;
    mepa_macsec_port_t macsec_port = {
        .port_no = rollover_egress_port_no,
        .port_id = MACSEC_SECY_ID,
        .service_id = 0
    };
    if (rollover_egress_config_port_cnt == 1) {
        if ((rc = mepa_macsec_secy_conf_del(meba_macsec_rollover_instance->phy_devices[rollover_egress_port_no], macsec_port)) != MEPA_RC_OK) {
            T_E("[ERROR] SecY Deletion failed\n");
            return;
        }
        /* Disable MAC Block on the Port */
        mepa_macsec_init_t init_data_enable = { .enable = 0,
                                                .dis_ing_nm_macsec_en = 1,
                                                .mac_conf.lmac.dis_length_validate = 0,
                                                .mac_conf.hmac.dis_length_validate = 0,
                                                .bypass = MEPA_MACSEC_INIT_BYPASS_NONE
                                              };
        if ((rc = mepa_macsec_init_set(meba_macsec_rollover_instance->phy_devices[rollover_egress_port_no], &init_data_enable)) != MEPA_RC_OK) {
            T_E("[ERROR] Disabling MACsec failed\n");
            return;
        }
    }
    if (rollover_ingress_config_port_cnt == 1) {
        mepa_macsec_init_t init_get;
        if ((rc = mepa_macsec_init_get(meba_macsec_rollover_instance->phy_devices[rollover_ingress_port_no], &init_get)) != MEPA_RC_OK) {
            T_E("[ERROR] MACsec Get failed\n");
            return;
        }
        if (init_get.enable == 0) {
            goto dis_polling;
        }
        macsec_port.port_no = rollover_ingress_port_no;
        if ((rc = mepa_macsec_secy_conf_del(meba_macsec_rollover_instance->phy_devices[rollover_ingress_port_no], macsec_port)) != MEPA_RC_OK) {
            T_E("[ERROR] SecY Deletion failed\n");
            return;
        }
        /* Disable MAC Block on the Port */
        mepa_macsec_init_t init_data_enable = { .enable = 0,
                                                .dis_ing_nm_macsec_en = 1,
                                                .mac_conf.lmac.dis_length_validate = 0,
                                                .mac_conf.hmac.dis_length_validate = 0,
                                                .bypass = MEPA_MACSEC_INIT_BYPASS_NONE
                                              };
        if ((rc = mepa_macsec_init_set(meba_macsec_rollover_instance->phy_devices[rollover_ingress_port_no], &init_data_enable)) != MEPA_RC_OK) {
            T_E("[ERROR] Disabling MACsec failed\n");
            return;
        }
    }

dis_polling:
    rollover_egress_config_port_cnt = 0;
    rollover_ingress_config_port_cnt = 0;

    cli_printf("\n.....[SUCCESS] Disabled MACsec Rollover Demo ......\n");
    return;
}

static void inline macsec_rollover_poll()
{
    if (rollover_egress_config_port_cnt == 1) {
        mepa_egress_key_rollover(meba_macsec_rollover_instance->phy_devices[rollover_egress_port_no], rollover_egress_port_no);
    }

    if (rollover_ingress_config_port_cnt == 1) {
        mepa_ingress_key_rollover(meba_macsec_rollover_instance->phy_devices[rollover_ingress_port_no], rollover_ingress_port_no);
    }
    return;
}

static int cli_cmd_parse_xpn_param(cli_req_t *req)
{
    cipher_suit = MACSEC_XPN_CIPHER_SUIT;
    return 0;
}

static int cli_cmd_parse_boolean(cli_req_t *req)
{
    if (!strncasecmp(req->cmd, "egress", strlen("egress"))) {
        rollover_direction_egress = 1;
    } else if (!strncasecmp(req->cmd, "ingress", strlen("ingress"))) {
        rollover_direction_egress = 0;
    } else {
		return 1;
    }
    return 0;
}


static cli_cmd_t cli_cmd_table[] = {
    {
        "key rollover demo <port_no> <egress|ingress> [xpn]",
        "\n MACsec Key Rollover Demo Enable",
        cli_cmd_macsec_rollover_demo,
    },

    {
        "key rollover log",
        "\n MACsec Key Rollover Log",
        cli_cmd_macsec_rollover_log_print,
    },

    {
        "key rollover disable",
        "\n MACsec Key Rollover Disable",
        cli_cmd_macsec_rollover_demo_disable,
    },
};

static cli_parm_t cli_parm_table[] = {
    {
        "<egress|ingress>",
        "Traffic Direction Egress or Ingress",
        CLI_PARM_FLAG_SET,
        cli_cmd_parse_boolean,
        cli_cmd_macsec_rollover_demo,
    },

    {
        "xpn",
        "Enable XPN Cipher Suit",
        CLI_PARM_FLAG_SET,
        cli_cmd_parse_xpn_param,
        cli_cmd_macsec_rollover_demo,
    },
};

static void phy_cli_init(void)
{
    int i;

    /* Register CLI Commands */
    for (i = 0; i < sizeof(cli_cmd_table) / sizeof(cli_cmd_t); i++) {
        mscc_appl_cli_cmd_reg(&cli_cmd_table[i]);
    }


    /* Register CLI Params */
    for (i = 0; i < sizeof(cli_parm_table) / sizeof(cli_parm_t); i++) {
        mscc_appl_cli_parm_reg(&cli_parm_table[i]);
    }

}

void mepa_demo_appl_macsec_rollover_demo(mscc_appl_init_t *init)
{
    meba_macsec_rollover_instance = init->board_inst;
    switch (init->cmd) {
    case MSCC_INIT_CMD_REG:
        mscc_appl_trace_register(&trace_module, trace_groups, TRACE_GROUP_CNT);
        break;
    case MSCC_INIT_CMD_INIT:
        phy_cli_init();
        break;
    case MSCC_INIT_CMD_POLL_FASTEST:
        macsec_rollover_poll();
        break;         
    default:
        break;
    }
}
