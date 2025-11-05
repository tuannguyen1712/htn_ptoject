#include "Libs_BleGattClient.h"
#include "Configuration.h"

/* ==== PROTOTYPES ==== */
static void start_scan(void);
static void try_connect_more(void);
static void on_found_target(const esp_bd_addr_t bda, esp_ble_addr_type_t type);
static int  find_free_slot(void);
static int  find_slot_by_bda(const esp_bd_addr_t bda);
static int  find_slot_by_conn(uint16_t conn_id);
static inline esp_bt_uuid_t UUID16(uint16_t u);
static bool find_handles_for_char(esp_gatt_if_t gi, uint16_t cid,
                                  uint16_t svc_start, uint16_t svc_end,
                                  uint16_t char_uuid16,
                                  uint16_t *out_char_val, uint16_t *out_cccd);
static void discover_and_subscribe_slot(int idx);

static const char *TAG = "GATTC_MULTI";

static peer_t peers[MAX_PEERS];
static device_data_t sensors[MAX_PEERS];

static esp_gatt_if_t gattc_if_global = ESP_GATT_IF_NONE;

static device_data_t devices_data[MAX_PEERS];

/* ==== UTILS ==== */
static inline esp_bt_uuid_t UUID16(uint16_t u) {
    esp_bt_uuid_t x = { .len = ESP_UUID_LEN_16, .uuid = { .uuid16 = u } };
    return x;
}

static int find_free_slot(void) {
    for (int i=0;i<MAX_PEERS;i++) if (!peers[i].in_use) return i;
    return -1;
}
static int find_slot_by_bda(const esp_bd_addr_t bda) {
    for (int i=0;i<MAX_PEERS;i++)
        if (peers[i].in_use && memcmp(peers[i].bda, bda, sizeof(esp_bd_addr_t))==0)
            return i;
    return -1;
}
static int find_slot_by_conn(uint16_t conn_id) {
    for (int i=0;i<MAX_PEERS;i++)
        if (peers[i].in_use && peers[i].conn_id == conn_id)
            return i;
    return -1;
}

/* =========================================================
 *                       GAP CALLBACK
 * ========================================================= */
static void gap_cb(esp_gap_ble_cb_event_t e, esp_ble_gap_cb_param_t *p)
{
    switch (e) {
    case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT:
        ESP_LOGI(TAG, "Scan params set -> start scanning");
        esp_ble_gap_start_scanning(0);
        break;

    case ESP_GAP_BLE_SCAN_RESULT_EVT:
        if (p->scan_rst.search_evt == ESP_GAP_SEARCH_INQ_RES_EVT) {
            uint8_t name_len = 0;
            uint8_t *name = esp_ble_resolve_adv_data(p->scan_rst.ble_adv,
                                                     ESP_BLE_AD_TYPE_NAME_CMPL, &name_len);
            if (name && name_len && strncmp((const char*)name, TARGET_NAME, name_len) == 0) {
                on_found_target(p->scan_rst.bda, p->scan_rst.ble_addr_type);
            }
        }
        break;

    case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT:
        // scan again if still need more connections
        try_connect_more();
        break;

    default:
        break;
    }
}

static void start_scan(void)
{
    esp_ble_scan_params_t params = {
        .scan_type              = BLE_SCAN_TYPE_ACTIVE,
        .own_addr_type          = BLE_ADDR_TYPE_PUBLIC,
        .scan_filter_policy     = BLE_SCAN_FILTER_ALLOW_ALL,
        .scan_interval          = 0x50,  // 80 * 0.625ms
        .scan_window            = 0x30,  // 48 * 0.625ms
        .scan_duplicate         = BLE_SCAN_DUPLICATE_DISABLE
    };
    ESP_ERROR_CHECK(esp_ble_gap_set_scan_params(&params));
}

static void on_found_target(const esp_bd_addr_t bda, esp_ble_addr_type_t type)
{
    if (find_slot_by_bda(bda) >= 0) return;   // đã biết
    int idx = find_free_slot();
    if (idx < 0) return;                      // hết slot

    peers[idx].in_use = true;
    peers[idx].connected = false;
    peers[idx].conn_id = 0;
    peers[idx].sensor_start = peers[idx].sensor_end = 0;
    peers[idx].led_start    = peers[idx].led_end    = 0;
    peers[idx].h_temp_val = peers[idx].h_humi_val = peers[idx].h_co2_val = peers[idx].h_mode_val = peers[idx].h_state_val = peers[idx].h_hum_thres_val  = 0;
    peers[idx].h_temp_cccd = peers[idx].h_humi_cccd = peers[idx].h_co2_cccd = peers[idx].h_mode_cccd = peers[idx].h_state_cccd = peers[idx].h_hum_thres_cccd = 0;
    peers[idx].h_led_val = peers[idx].h_led_mode_val = peers[idx].h_led_hum_thres_val = 0;
    memcpy(peers[idx].bda, bda, sizeof(esp_bd_addr_t));
    peers[idx].bda_type = type;

    ESP_LOGI(TAG, "Target found -> connect slot %d", idx);
    // Dừng scan rồi connect (an toàn cho controller)
    esp_ble_gap_stop_scanning();
    esp_ble_gattc_open(gattc_if_global, peers[idx].bda, peers[idx].bda_type, true);
}

static void try_connect_more(void)
{
    // Nếu còn slot trống và chưa scan → bật lại scan
    for (int i=0;i<MAX_PEERS;i++) {
        if (!peers[i].in_use) {
            esp_ble_gap_start_scanning(0);
            break;
        }
    }
}

/* =========================================================
 *                      GATTC CALLBACK
 * ========================================================= */
static void gattc_cb(esp_gattc_cb_event_t e, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *p)
{
    switch (e) {
    case ESP_GATTC_REG_EVT:
        gattc_if_global = gattc_if;
        ESP_LOGI(TAG, "Registered gattc_if=%d, start scan", gattc_if);
        start_scan();
        break;

    case ESP_GATTC_OPEN_EVT: {
        int idx = find_slot_by_bda(p->open.remote_bda);
        if (p->open.status == ESP_GATT_OK && idx >= 0) {
            peers[idx].connected = true;
            peers[idx].conn_id = p->open.conn_id;
            ESP_LOGI(TAG, "Connected slot=%d conn_id=%d -> MTU req", idx, peers[idx].conn_id);
            esp_ble_gattc_send_mtu_req(gattc_if, peers[idx].conn_id);
        } else {
            ESP_LOGE(TAG, "Open failed status=0x%x slot=%d -> rescan", p->open.status, idx);
            // clear slot nếu open fail
            if (idx >= 0) memset(&peers[idx], 0, sizeof(peer_t));
            try_connect_more();
        }
        break;
    }

    case ESP_GATTC_CFG_MTU_EVT: {
        ESP_LOGI(TAG, "MTU configured: %d", p->cfg_mtu.mtu);
        esp_ble_gattc_search_service(gattc_if, p->cfg_mtu.conn_id, NULL);
        break;
    }

    case ESP_GATTC_SEARCH_RES_EVT: {
        int idx = find_slot_by_conn(p->search_res.conn_id);
        if (idx < 0) break;
        const esp_gatt_id_t *sid = &p->search_res.srvc_id; // v5.1.1
        if (sid->uuid.len == ESP_UUID_LEN_16) {
            uint16_t su = sid->uuid.uuid.uuid16;
            if (su == SENSOR_SVC_UUID16) {
                peers[idx].sensor_start = p->search_res.start_handle;
                peers[idx].sensor_end   = p->search_res.end_handle;
                ESP_LOGI(TAG, "[%d] SENSOR svc start=0x%04X end=0x%04X", idx, peers[idx].sensor_start, peers[idx].sensor_end);
            } else if (su == LED_SVC_UUID16) {
                peers[idx].led_start = p->search_res.start_handle;
                peers[idx].led_end   = p->search_res.end_handle;
                ESP_LOGI(TAG, "[%d] LED svc start=0x%04X end=0x%04X", idx, peers[idx].led_start, peers[idx].led_end);
            }
        }
        break;
    }

    case ESP_GATTC_SEARCH_CMPL_EVT: {
        int idx = find_slot_by_conn(p->search_cmpl.conn_id);
        if (idx < 0) break;
        ESP_LOGI(TAG, "[%d] Search complete -> discover & subscribe", idx);
        discover_and_subscribe_slot(idx);
        // Sau khi kết nối xong node này, tiếp tục scan để tìm node khác
        try_connect_more();
        break;
    }

    case ESP_GATTC_REG_FOR_NOTIFY_EVT:
        ESP_LOGI(TAG, "Register for notify status=0x%x", p->reg_for_notify.status);
        break;

    case ESP_GATTC_NOTIFY_EVT: {
        int idx = find_slot_by_conn(p->notify.conn_id);
        if (idx < 0 || !p->notify.is_notify) break;
        ESP_LOGI(TAG, "[%d] NOTIFY handle=0x%04X len=%d", idx, p->notify.handle, p->notify.value_len);
        if (p->notify.handle == peers[idx].h_temp_val && p->notify.value_len >= 2) {
            int16_t t; memcpy(&t, p->notify.value, 2);
            ESP_LOGI(TAG, "[%d] Temp = %.1f C", idx, t/10.0f);
            sensors[idx].temp = t;
        } else if (p->notify.handle == peers[idx].h_humi_val && p->notify.value_len >= 2) {
            int16_t h; memcpy(&h, p->notify.value, 2);
            ESP_LOGI(TAG, "[%d] Humi = %.1f %%", idx, h/10.0f);
            sensors[idx].humi = h;
        } else if (p->notify.handle == peers[idx].h_co2_val && p->notify.value_len >= 2) {
            uint16_t c; memcpy(&c, p->notify.value, 2);
            ESP_LOGI(TAG, "[%d] CO2  = %u ppm", idx, c);
            sensors[idx].co2 = c;
        }
        else if (p->notify.handle == peers[idx].h_mode_val && p->notify.value_len >= 2) {
            uint16_t m; memcpy(&m, p->notify.value, 2);
            ESP_LOGI(TAG, "[%d] Mode = %u C", idx, m);
            sensors[idx].mode = m;
        }
        else if (p->notify.handle == peers[idx].h_state_val && p->notify.value_len >= 2) {
            uint16_t s; memcpy(&s, p->notify.value, 2);
            ESP_LOGI(TAG, "[%d] State = %u C", idx, s);
            sensors[idx].state = s;
        }
        else if (p->notify.handle == peers[idx].h_hum_thres_val && p->notify.value_len >= 2) {
            uint16_t ht; memcpy(&ht, p->notify.value, 2);
            ESP_LOGI(TAG, "[%d] Hum threshold = %u C", idx, ht);
            sensors[idx].hum_thres = ht;

            xEventGroupSetBits(xEventGroup, DATA_COMMING_EVENT_BIT);
            ESP_LOGI(TAG, "[%d] >>> All sensor data received, ready to upload", idx);
        }
        break;
    }

    case ESP_GATTC_WRITE_CHAR_EVT: {
        int idx = find_slot_by_conn(p->write.conn_id);
        ESP_LOGI(TAG, "[%d] Write complete handle=0x%04X status=0x%x", idx, p->write.handle, p->write.status);
        break;
    }

    case ESP_GATTC_DISCONNECT_EVT: {
        int idx = find_slot_by_conn(p->disconnect.conn_id);
        ESP_LOGW(TAG, "[%d] Disconnected, reason=0x%x", idx, p->disconnect.reason);
        if (idx >= 0) memset(&peers[idx], 0, sizeof(peer_t));
        try_connect_more();
        break;
    }

    default:
        break;
    }
}

/* =========================================================
 *                  DISCOVERY + SUBSCRIBE (per slot)
 * ========================================================= */
static bool find_handles_for_char(esp_gatt_if_t gi, uint16_t cid,
                                  uint16_t svc_start, uint16_t svc_end,
                                  uint16_t char_uuid16,
                                  uint16_t *out_char_val, uint16_t *out_cccd)
{
    esp_bt_uuid_t cu = UUID16(char_uuid16);

    // Đếm char trong khoảng service
    uint16_t count = 0;
    if (esp_ble_gattc_get_attr_count(gi, cid, ESP_GATT_DB_CHARACTERISTIC,
                                     svc_start, svc_end, 0, &count) != ESP_GATT_OK || !count) {
        return false;
    }

    // Lấy char theo UUID
    esp_gattc_char_elem_t *chars = calloc(count, sizeof(*chars));
    if (!chars) return false;

    uint16_t num = count;
    esp_gatt_status_t st = esp_ble_gattc_get_char_by_uuid(gi, cid, svc_start, svc_end,
                                                          cu, chars, &num);
    if (st != ESP_GATT_OK || num == 0) { free(chars); return false; }

    uint16_t char_val = chars[0].char_handle;
    if (out_char_val) *out_char_val = char_val;
    free(chars);

    // Nếu cần CCCD (notify)
    if (out_cccd) {
        esp_bt_uuid_t du = UUID16(CCCD_UUID16);
        uint16_t dcnt = 0;
        if (esp_ble_gattc_get_attr_count(gi, cid, ESP_GATT_DB_DESCRIPTOR,
                                         svc_start, svc_end, char_val, &dcnt) == ESP_GATT_OK && dcnt) {
            esp_gattc_descr_elem_t *descrs = calloc(dcnt, sizeof(*descrs));
            if (descrs) {
                uint16_t dnum = dcnt;
                if (esp_ble_gattc_get_descr_by_char_handle(gi, cid, char_val, du, descrs, &dnum) == ESP_GATT_OK && dnum) {
                    *out_cccd = descrs[0].handle;
                }
                free(descrs);
            }
        }
    }
    return true;
}

static void discover_and_subscribe_slot(int idx)
{
    if (peers[idx].sensor_start && peers[idx].sensor_end) {
        // Temp
        if (find_handles_for_char(gattc_if_global, peers[idx].conn_id,
                                  peers[idx].sensor_start, peers[idx].sensor_end,
                                  TEMP_CHAR_UUID16, &peers[idx].h_temp_val, &peers[idx].h_temp_cccd)) {
            esp_ble_gattc_register_for_notify(gattc_if_global, peers[idx].bda, peers[idx].h_temp_val);
            uint8_t en[2] = {0x01, 0x00};
            if (peers[idx].h_temp_cccd) {
                esp_ble_gattc_write_char_descr(gattc_if_global, peers[idx].conn_id, peers[idx].h_temp_cccd, 2, en,
                                               ESP_GATT_WRITE_TYPE_RSP, ESP_GATT_AUTH_REQ_NONE);
            }
        }
        // Humi
        if (find_handles_for_char(gattc_if_global, peers[idx].conn_id,
                                  peers[idx].sensor_start, peers[idx].sensor_end,
                                  HUMI_CHAR_UUID16, &peers[idx].h_humi_val, &peers[idx].h_humi_cccd)) {
            esp_ble_gattc_register_for_notify(gattc_if_global, peers[idx].bda, peers[idx].h_humi_val);
            uint8_t en[2] = {0x01, 0x00};
            if (peers[idx].h_humi_cccd) {
                esp_ble_gattc_write_char_descr(gattc_if_global, peers[idx].conn_id, peers[idx].h_humi_cccd, 2, en,
                                               ESP_GATT_WRITE_TYPE_RSP, ESP_GATT_AUTH_REQ_NONE);
            }
        }
        // CO2
        if (find_handles_for_char(gattc_if_global, peers[idx].conn_id,
                                  peers[idx].sensor_start, peers[idx].sensor_end,
                                  CO2_CHAR_UUID16, &peers[idx].h_co2_val, &peers[idx].h_co2_cccd)) {
            esp_ble_gattc_register_for_notify(gattc_if_global, peers[idx].bda, peers[idx].h_co2_val);
            uint8_t en[2] = {0x01, 0x00};
            if (peers[idx].h_co2_cccd) {
                esp_ble_gattc_write_char_descr(gattc_if_global, peers[idx].conn_id, peers[idx].h_co2_cccd, 2, en,
                                               ESP_GATT_WRITE_TYPE_RSP, ESP_GATT_AUTH_REQ_NONE);
            }
        }
        // mode
        if (find_handles_for_char(gattc_if_global, peers[idx].conn_id,
                                  peers[idx].sensor_start, peers[idx].sensor_end,
                                  DEVICE_MODE, &peers[idx].h_mode_val, &peers[idx].h_mode_cccd)) {
            esp_ble_gattc_register_for_notify(gattc_if_global, peers[idx].bda, peers[idx].h_mode_val);
            uint8_t en[2] = {0x01, 0x00};
            if (peers[idx].h_mode_cccd) {
                esp_ble_gattc_write_char_descr(gattc_if_global, peers[idx].conn_id, peers[idx].h_mode_cccd, 2, en,
                                               ESP_GATT_WRITE_TYPE_RSP, ESP_GATT_AUTH_REQ_NONE);
            }
        }

        // state
        if (find_handles_for_char(gattc_if_global, peers[idx].conn_id,
                                  peers[idx].sensor_start, peers[idx].sensor_end,
                                  DEVICE_STATE, &peers[idx].h_state_val, &peers[idx].h_state_cccd)) {
            esp_ble_gattc_register_for_notify(gattc_if_global, peers[idx].bda, peers[idx].h_state_val);
            uint8_t en[2] = {0x01, 0x00};
            if (peers[idx].h_state_cccd) {
                esp_ble_gattc_write_char_descr(gattc_if_global, peers[idx].conn_id, peers[idx].h_state_cccd, 2, en,
                                               ESP_GATT_WRITE_TYPE_RSP, ESP_GATT_AUTH_REQ_NONE);
            }
        }

        // hum threshold 
        if (find_handles_for_char(gattc_if_global, peers[idx].conn_id,
                                  peers[idx].sensor_start, peers[idx].sensor_end,
                                  DEVICE_HUM_THRES, &peers[idx].h_hum_thres_val, &peers[idx].h_hum_thres_cccd)) {
            esp_ble_gattc_register_for_notify(gattc_if_global, peers[idx].bda, peers[idx].h_hum_thres_val);
            uint8_t en[2] = {0x01, 0x00};
            if (peers[idx].h_hum_thres_cccd) {
                esp_ble_gattc_write_char_descr(gattc_if_global, peers[idx].conn_id, peers[idx].h_hum_thres_cccd, 2, en,
                                               ESP_GATT_WRITE_TYPE_RSP, ESP_GATT_AUTH_REQ_NONE);
            }
        }
    }

    if (peers[idx].led_start && peers[idx].led_end) {
        find_handles_for_char(gattc_if_global, peers[idx].conn_id,
                              peers[idx].led_start, peers[idx].led_end,
                              LED_CHAR_UUID16, &peers[idx].h_led_val, NULL);
    }

    if (peers[idx].led_start && peers[idx].led_end) {
        find_handles_for_char(gattc_if_global, peers[idx].conn_id,
                              peers[idx].led_start, peers[idx].led_end,
                              MODE_CHAR_UUID16, &peers[idx].h_led_mode_val, NULL);
    }
    if (peers[idx].led_start && peers[idx].led_end) {
        find_handles_for_char(gattc_if_global, peers[idx].conn_id,
                              peers[idx].led_start, peers[idx].led_end,
                              HUM_THRES_CHAR_UUID16, &peers[idx].h_led_hum_thres_val, NULL);
    }
}

/* =========================================================
 *                     GLOBAL FUNCTION
 * ========================================================= */
void gateway_set_sampling_interval(int server_idx, uint16_t sampling_interval)
{
    if (server_idx < 0 || server_idx >= MAX_PEERS) return;
    if (!peers[server_idx].connected || !peers[server_idx].h_led_val) {
        ESP_LOGI(TAG, "LED write skipped: connected=%d, h_led_val=0x%04X", peers[server_idx].connected, peers[server_idx].h_led_val);
        return;
    }
    if (peers[server_idx].connected && peers[server_idx].h_led_val) {
        esp_ble_gattc_write_char(gattc_if_global, peers[server_idx].conn_id, peers[server_idx].h_led_val,
                                 2, (uint8_t*)&sampling_interval, ESP_GATT_WRITE_TYPE_RSP, ESP_GATT_AUTH_REQ_NONE);
        ESP_LOGI("DEBUG2", "sampling interval = %d", sampling_interval);
    }
}

void gateway_set_mode(int server_idx, uint8_t on)
{
    if (server_idx < 0 || server_idx >= MAX_PEERS) return;
    if (!peers[server_idx].connected || !peers[server_idx].h_led_mode_val) {
        ESP_LOGI(TAG, "LED write skipped: connected=%d, h_led_mode_val=0x%04X", peers[server_idx].connected, peers[server_idx].h_led_mode_val);
        return;
    }
    if (peers[server_idx].connected && peers[server_idx].h_led_mode_val) {
        esp_ble_gattc_write_char(gattc_if_global, peers[server_idx].conn_id, peers[server_idx].h_led_mode_val,
                                 1, &on, ESP_GATT_WRITE_TYPE_RSP, ESP_GATT_AUTH_REQ_NONE);
    }
}

void gateway_set_humth(int server_idx, uint8_t on)
{
    if (server_idx < 0 || server_idx >= MAX_PEERS) return;
    if (!peers[server_idx].connected || !peers[server_idx].h_led_hum_thres_val) {
        ESP_LOGI(TAG, "LED write skipped: connected=%d, h_led_hum_thres_val=0x%04X", peers[server_idx].connected, peers[server_idx].h_led_hum_thres_val);
        return;
    }
    if (peers[server_idx].connected && peers[server_idx].h_led_hum_thres_val) {
        esp_ble_gattc_write_char(gattc_if_global, peers[server_idx].conn_id, peers[server_idx].h_led_hum_thres_val,
                                 1, &on, ESP_GATT_WRITE_TYPE_RSP, ESP_GATT_AUTH_REQ_NONE);
    }
}

void Libs_BLEGattClient_init(void) 
{
    // BLE init (Bluedroid)
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BLE));
    ESP_ERROR_CHECK(esp_bluedroid_init());
    ESP_ERROR_CHECK(esp_bluedroid_enable());

    // callbacks
    ESP_ERROR_CHECK(esp_ble_gap_register_callback(gap_cb));
    ESP_ERROR_CHECK(esp_ble_gattc_register_callback(gattc_cb));
    ESP_ERROR_CHECK(esp_ble_gattc_app_register(0));

    // MTU
    esp_err_t mtu_ret = esp_ble_gatt_set_local_mtu(500);
    if (mtu_ret) ESP_LOGW(TAG, "set local MTU failed, err=0x%x", mtu_ret);
}

void Libs_BLEGattClientGetValue(uint16_t idx, uint16_t *temp, uint16_t *humi, uint16_t *co2, uint16_t *mode, uint16_t *state, uint16_t *hum_thres)
{
    // Lấy dữ liệu từ tất cả các peer (nếu có)
    *temp = sensors[idx].temp;
    *humi = sensors[idx].humi;
    *co2  = sensors[idx].co2;
    *mode = sensors[idx].mode;
    *state= sensors[idx].state;
    *hum_thres= sensors[idx].hum_thres;
}