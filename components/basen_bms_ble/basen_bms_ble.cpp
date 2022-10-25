#include "basen_bms_ble.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace basen_bms_ble {

static const char *const TAG = "basen_bms_ble";

static const uint16_t BASEN_BMS_SERVICE_UUID = 0xFA00;
static const uint16_t BASEN_BMS_NOTIFY_CHARACTERISTIC_UUID = 0xFA01;   // handle 0x12
static const uint16_t BASEN_BMS_CONTROL_CHARACTERISTIC_UUID = 0xFA02;  // handle 0x15

static const uint16_t MAX_RESPONSE_SIZE = 42 + 2;

static const uint8_t BASEN_PKT_START_A = 0x3A;
static const uint8_t BASEN_PKT_START_B = 0x3B;
static const uint8_t BASEN_ADDRESS = 0x16;
static const uint8_t BASEN_PKT_END_1 = 0x0D;
static const uint8_t BASEN_PKT_END_2 = 0x0A;

static const uint8_t BASEN_FRAME_TYPE_CELL_VOLTAGES_1_12 = 0x24;
static const uint8_t BASEN_FRAME_TYPE_CELL_VOLTAGES_13_24 = 0x25;
static const uint8_t BASEN_FRAME_TYPE_CELL_VOLTAGES_25_34 = 0x26;
static const uint8_t BASEN_FRAME_TYPE_STATUS = 0x2A;
static const uint8_t BASEN_FRAME_TYPE_GENERAL_INFO = 0x2B;
static const uint8_t BASEN_FRAME_TYPE_SETTINGS = 0xE8;
static const uint8_t BASEN_FRAME_TYPE_SETTINGS_ALTERNATIVE = 0xEA;
static const uint8_t BASEN_FRAME_TYPE_PROTECT_IC = 0xFE;

static const uint8_t BASEN_COMMANDS_SIZE = 5;
static const uint8_t BASEN_COMMANDS[BASEN_COMMANDS_SIZE] = {
    BASEN_FRAME_TYPE_STATUS,
    BASEN_FRAME_TYPE_GENERAL_INFO,
    BASEN_FRAME_TYPE_CELL_VOLTAGES_1_12,
    BASEN_FRAME_TYPE_CELL_VOLTAGES_13_24,
    BASEN_FRAME_TYPE_PROTECT_IC,
};

static const uint8_t CHARGING_STATES_SIZE = 8;
static const char *const CHARGING_STATES[CHARGING_STATES_SIZE] = {
    "Overcurrent protection (SOCC)",  // 0000 0001
    "Over temperature (OTC)",         // 0000 0010
    "Undertemperature (UTC)",         // 0000 0100
    "Cell overvoltage (COV)",         // 0000 1000
    "Battery overvoltage (FC)",       // 0001 0000
    "Reserved",                       // 0010 0000
    "Reserved",                       // 0100 0000
    "Charging MOS (CHG)",             // 1000 0000
};

static const uint8_t CHARGING_WARNINGS_SIZE = 8;
static const char *const CHARGING_WARNINGS[CHARGING_WARNINGS_SIZE] = {
    "Overcurrent (OCC1)",          // 0000 0001
    "Over temperature (OTC)",      // 0000 0010
    "Undertemperature (UTC1)",     // 0000 0100
    "Differential Pressure (DP)",  // 0000 1000
    "Fully charged (FC)",          // 0001 0000
    "Reserved",                    // 0010 0000
    "Reserved",                    // 0100 0000
    "Reserved",                    // 1000 0000
};

static const uint8_t DISCHARGING_STATES_SIZE = 8;
static const char *const DISCHARGING_STATES[DISCHARGING_STATES_SIZE] = {
    "Overcurrent protection (SOCD)",    // 0000 0001
    "Over temperature (OTD)",           // 0000 0010
    "Undertemperature (UTD)",           // 0000 0100
    "Battery undervoltage (CUV)",       // 0000 1000
    "Battery empty (FD)",               // 0001 0000
    "Short circuit protection (ASCD)",  // 0010 0000
    "Termination of discharge (TDA)",   // 0100 0000
    "Discharging MOS (DSG)",            // 1000 0000
};

static const uint8_t DISCHARGING_WARNINGS_SIZE = 8;
static const char *const DISCHARGING_WARNINGS[DISCHARGING_WARNINGS_SIZE] = {
    "Overcurrent (OCD1)",                     // 0000 0001
    "Over temperature (OTD1)",                // 0000 0010
    "Undertemperature (UTD1)",                // 0000 0100
    "Differential Pressure (DP)",             // 0000 1000
    "Not enough time left (RTA)",             // 0001 0000
    "Insufficient capacity remaining (RCA)",  // 0010 0000
    "Battery undervoltage (CUV)",             // 0100 0000
    "Battery empty (FD)",                     // 1000 0000
};

void BasenBmsBle::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                                      esp_ble_gattc_cb_param_t *param) {
  switch (event) {
    case ESP_GATTC_OPEN_EVT: {
      break;
    }
    case ESP_GATTC_DISCONNECT_EVT: {
      this->node_state = espbt::ClientState::IDLE;

      // this->publish_state_(this->voltage_sensor_, NAN);
      break;
    }
    case ESP_GATTC_SEARCH_CMPL_EVT: {
      // [esp32_ble_client:048]: [0] [A4:C1:38:27:48:9A] Found device
      // [esp32_ble_client:064]: [0] [A4:C1:38:27:48:9A] 0x00 Attempting BLE connection
      // [esp32_ble_client:126]: [0] [A4:C1:38:27:48:9A] ESP_GATTC_OPEN_EVT
      // [esp32_ble_client:186]: [0] [A4:C1:38:27:48:9A] ESP_GATTC_SEARCH_CMPL_EVT
      // [esp32_ble_client:189]: [0] [A4:C1:38:27:48:9A] Service UUID: 0x1800
      // [esp32_ble_client:191]: [0] [A4:C1:38:27:48:9A] start_handle: 0x1 end_handle: 0x7
      // [esp32_ble_client:189]: [0] [A4:C1:38:27:48:9A] Service UUID: 0x1801
      // [esp32_ble_client:191]: [0] [A4:C1:38:27:48:9A] start_handle: 0x8 end_handle: 0xb
      // [esp32_ble_client:189]: [0] [A4:C1:38:27:48:9A] Service UUID: 0x180A
      // [esp32_ble_client:191]: [0] [A4:C1:38:27:48:9A] start_handle: 0xc end_handle: 0xe
      // [esp32_ble_client:189]: [0] [A4:C1:38:27:48:9A] Service UUID: 0xFA00
      // [esp32_ble_client:191]: [0] [A4:C1:38:27:48:9A] start_handle: 0xf end_handle: 0x16
      // [esp32_ble_client:193]: [0] [A4:C1:38:27:48:9A] Connected
      // [esp32_ble_client:069]: [0] [A4:C1:38:27:48:9A] characteristic 0xFA01, handle 0x11, properties 0x12
      // [esp32_ble_client:069]: [0] [A4:C1:38:27:48:9A] characteristic 0xFA02, handle 0x15, properties 0x6

      auto *char_notify =
          this->parent_->get_characteristic(BASEN_BMS_SERVICE_UUID, BASEN_BMS_NOTIFY_CHARACTERISTIC_UUID);
      if (char_notify == nullptr) {
        ESP_LOGE(TAG, "[%s] No notify service found at device, not an BASEN BMS..?",
                 this->parent_->address_str().c_str());
        break;
      }
      this->char_notify_handle_ = char_notify->handle;

      auto status = esp_ble_gattc_register_for_notify(this->parent()->get_gattc_if(), this->parent()->get_remote_bda(),
                                                      char_notify->handle);
      if (status) {
        ESP_LOGW(TAG, "esp_ble_gattc_register_for_notify failed, status=%d", status);
      }

      auto *char_command =
          this->parent_->get_characteristic(BASEN_BMS_SERVICE_UUID, BASEN_BMS_CONTROL_CHARACTERISTIC_UUID);
      if (char_command == nullptr) {
        ESP_LOGE(TAG, "[%s] No control service found at device, not an BASEN BMS..?",
                 this->parent_->address_str().c_str());
        break;
      }
      this->char_command_handle_ = char_command->handle;
      break;
    }
    case ESP_GATTC_REG_FOR_NOTIFY_EVT: {
      this->node_state = espbt::ClientState::ESTABLISHED;

      // Write 3b162a010041000d0a to handle 0x15
      // Response 1: 3b162a1843040000cd68000015161919691f0000 + 8080000007020000c2030d0a
      this->send_command_(BASEN_PKT_START_B, BASEN_FRAME_TYPE_STATUS);

      // Write 3b162a010041000d0a to handle 0x15
      // Response 2: 3b162a1843040000cd68000015161919691f0000 + 8080000007020000c2030d0a
      // this->send_command_(BASEN_PKT_START_B, 0x2A);

      // Write 3b162a010041000d0a to handle 0x15
      // Response 3: 3b162a1843040000cd683a162418170d1b0d170d + 190d180d1b0d1c0d1c0d00000000000000008701 + 0d0a
      // this->send_command_(BASEN_PKT_START_B, 0x2A);

      break;
    }
    case ESP_GATTC_NOTIFY_EVT: {
      ESP_LOGVV(TAG, "Notification received (handle 0x%02X): %s", param->notify.handle,
                format_hex_pretty(param->notify.value, param->notify.value_len).c_str());

      this->assemble_(param->notify.value, param->notify.value_len);
      break;
    }
    default:
      break;
  }
}

void BasenBmsBle::assemble_(const uint8_t *data, uint16_t length) {
  if (this->frame_buffer_.size() > MAX_RESPONSE_SIZE) {
    ESP_LOGW(TAG, "Maximum response size exceeded");
    this->frame_buffer_.clear();
  }

  // Flush buffer on every preamble
  if (data[0] == BASEN_PKT_START_A || data[0] == BASEN_PKT_START_B) {
    this->frame_buffer_.clear();
  }

  this->frame_buffer_.insert(this->frame_buffer_.end(), data, data + length);

  if (this->frame_buffer_.back() == BASEN_PKT_END_2) {
    const uint8_t *raw = &this->frame_buffer_[0];

    uint16_t data_len = raw[3];
    uint16_t frame_len = 4 + data_len + 4;
    if (frame_len != this->frame_buffer_.size()) {
      ESP_LOGW(TAG, "Invalid frame length");
      this->frame_buffer_.clear();
      return;
    }

    uint16_t computed_crc = chksum_(raw + 1, data_len + 3);
    uint16_t remote_crc = uint16_t(raw[frame_len - 3]) << 8 | (uint16_t(raw[frame_len - 4]) << 0);
    if (computed_crc != remote_crc) {
      ESP_LOGW(TAG, "CRC check failed! 0x%04X != 0x%04X", computed_crc, remote_crc);
      this->frame_buffer_.clear();
      return;
    }

    std::vector<uint8_t> data(this->frame_buffer_.begin(), this->frame_buffer_.end() - 4);

    this->on_basen_bms_ble_data_(data);
    this->frame_buffer_.clear();
  }
}

void BasenBmsBle::update() {
  if (this->enable_fake_traffic_) {
    // 0x3a, 0x16, 0x2a, 0x18, 0x43, 0x04, 0x00, 0x00, 0xcc, 0x68, 0x00, 0x00, 0x15, 0x16, 0x19, 0x19, 0x69, 0x1f, 0x00,
    // 0x00, 0x80, 0x80, 0x00, 0x00, 0x07, 0x02, 0x00, 0x00, 0xc1, 0x03, 0x0d, 0x0a
    //
    // 0x3a, 0x16, 0x2b, 0x18, 0xa0, 0x86, 0x01, 0x00, 0x00, 0x64, 0x00, 0x00, 0x91, 0xa0, 0x01, 0x00, 0x00, 0x00, 0x00,
    // 0x00, 0x30, 0x75, 0x00, 0x00, 0x71, 0x53, 0x07, 0x00, 0x86, 0x04, 0x0d, 0x0a
    //
    // 0x3b, 0x16, 0x2a, 0x18, 0x43, 0x04, 0x00, 0x00, 0xcd, 0x68, 0x00, 0x00, 0x15, 0x16, 0x19, 0x19, 0x69, 0x1f, 0x00,
    // 0x00, 0x80, 0x80, 0x00, 0x00, 0x07, 0x02, 0x00, 0x00, 0xc2, 0x03, 0x0d, 0x0a
    //
    // 0x3a, 0x16, 0xfe, 0x13, 0x00, 0xf9, 0x0f, 0x2c, 0x80, 0x80, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    // 0x02, 0x76, 0x53, 0x61, 0x07, 0x05, 0x0d, 0x0a
    //
    // 0x3a, 0x16, 0x25, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    // 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x53, 0x00, 0x0d, 0x0a
    //
    // Current -6909 mAh
    // 0x3a, 0x16, 0x2a, 0x18, 0x03, 0xe5, 0xff, 0xff, 0x06, 0x64, 0x00, 0x00, 0x12, 0x14, 0x19, 0x19, 0x35, 0x3d, 0x00,
    // 0x00, 0x80, 0x80, 0x00, 0x00, 0x0e, 0x02, 0x00, 0x00, 0x82, 0x05, 0x0d, 0x0a
    //
    // Cell voltages 1
    // 0x3a, 0x16, 0x24, 0x18, 0x96, 0x0c, 0x97, 0x0c, 0x98, 0x0c, 0x96, 0x0c, 0x96, 0x0c, 0x98, 0x0c, 0x98, 0x0c, 0x97,
    // 0x0c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x6a, 0x05, 0x0d, 0x0a
    const uint8_t frame[32] = {0x3a, 0x16, 0x2a, 0x18, 0x03, 0xe5, 0xff, 0xff, 0x06, 0x64, 0x00,
                               0x00, 0x12, 0x14, 0x19, 0x19, 0x35, 0x3d, 0x00, 0x00, 0x80, 0x80,
                               0x00, 0x00, 0x0e, 0x02, 0x00, 0x00, 0x82, 0x05, 0x0d, 0x0a};

    this->assemble_(frame, sizeof(frame));
  }

  if (this->node_state != espbt::ClientState::ESTABLISHED) {
    ESP_LOGW(TAG, "[%s] Not connected", this->parent_->address_str().c_str());
    return;
  }

  // Loop through all commands if connected
  // this->next_command_ = 0;
  this->send_command_(BASEN_PKT_START_A, BASEN_COMMANDS[this->next_command_++ % BASEN_COMMANDS_SIZE]);
}

void BasenBmsBle::on_basen_bms_ble_data_(const std::vector<uint8_t> &data) {
  uint8_t frame_type = data[2];

  switch (frame_type) {
    case BASEN_FRAME_TYPE_STATUS:
      this->decode_status_data_(data);
      break;
    case BASEN_FRAME_TYPE_GENERAL_INFO:
      this->decode_general_info_data_(data);
      break;
    case BASEN_FRAME_TYPE_CELL_VOLTAGES_1_12:
    case BASEN_FRAME_TYPE_CELL_VOLTAGES_13_24:
    case BASEN_FRAME_TYPE_CELL_VOLTAGES_25_34:
      this->decode_cell_voltages_data_(data);
      break;
    case BASEN_FRAME_TYPE_PROTECT_IC:
      this->decode_protect_ic_data_(data);
      break;
    default:
      ESP_LOGW(TAG, "Unhandled response received (frame_type 0x%02X): %s", frame_type,
               format_hex_pretty(&data.front(), data.size()).c_str());
  }

  // Loop through all commands if connected
  // this->send_command_(BASEN_PKT_START_A, BASEN_COMMANDS[this->next_command_++ % BASEN_COMMANDS_SIZE]);
}

void BasenBmsBle::decode_status_data_(const std::vector<uint8_t> &data) {
  auto basen_get_16bit = [&](size_t i) -> uint16_t {
    return (uint16_t(data[i + 1]) << 8) | (uint16_t(data[i + 0]) << 0);
  };
  auto basen_get_32bit = [&](size_t i) -> uint32_t {
    return (uint32_t(basen_get_16bit(i + 2)) << 16) | (uint32_t(basen_get_16bit(i + 0)) << 0);
  };

  ESP_LOGI(TAG, "Status frame (%d+4 bytes):", data.size());
  ESP_LOGD(TAG, "  %s", format_hex_pretty(&data.front(), data.size()).c_str());

  // Byte Len Payload              Description                      Unit  Precision
  //  0    1  0x3B                 Start of frame
  //  1    1  0x16                 Address
  //  2    1  0x2A                 Frame type
  //  3    1  0x18                 Data length
  //  4    4  0x00 0x00 0x00 0x00  Current (without calibration)    A     0.001f
  ESP_LOGI(TAG, "  Current: %.3f A", ((int32_t) basen_get_32bit(4)) * 0.001f);

  //  8    4  0xCE 0x61 0x00 0x00  Total voltage                    V     0.001f
  ESP_LOGI(TAG, "  Total voltage: %.3f V", basen_get_32bit(8) * 0.001f);

  //  12   1  0x12                 Temperature 1                    °C    1.0f
  ESP_LOGD(TAG, "  Temperature 1: %.0f °C", data[12] * 1.0f);

  //  13   1  0x14                 Temperature 2                    °C    1.0f
  ESP_LOGD(TAG, "  Temperature 2: %.0f °C", data[13] * 1.0f);

  //  14   1  0x19                 Temperature 3                    °C    1.0f
  ESP_LOGD(TAG, "  Temperature 3: %.0f °C", data[14] * 1.0f);

  //  15   1  0x19                 Temperature 4                    °C    1.0f
  ESP_LOGD(TAG, "  Temperature 4: %.0f °C", data[15] * 1.0f);

  //  16   4  0x63 0x23 0x00 0x00  Capacity remaining               Ah    0.001f
  ESP_LOGI(TAG, "  Capacity remaining: %.3f Ah", basen_get_32bit(16) * 0.001f);

  //  20   1  0x80                 Charging states (Bitmask)
  ESP_LOGD(TAG, "  Charging states (Bitmask): %d (0x%02X)", data[20], data[20]);
  ESP_LOGI(TAG, "  Charging states: %s", this->charging_states_bits_to_string_(data[20]).c_str());

  //  21   1  0x80                 Discharging states (Bitmask)
  ESP_LOGD(TAG, "  Discharging states (Bitmask): %d (0x%02X)", data[21], data[21]);
  ESP_LOGI(TAG, "  Discharging states: %s", this->discharging_states_bits_to_string_(data[21]).c_str());

  //  22   1  0x00                 Charging warnings (Bitmask)
  ESP_LOGD(TAG, "  Charging warnings (Bitmask): %d (0x%02X)", data[22], data[22]);
  ESP_LOGI(TAG, "  Charging warnings: %s", this->charging_warnings_bits_to_string_(data[22]).c_str());

  //  23   1  0x00                 Discharging warnings (Bitmask)
  ESP_LOGD(TAG, "  Discharging warnings (Bitmask): %d (0x%02X)", data[23], data[23]);
  ESP_LOGI(TAG, "  Discharging warnings: %s", this->discharging_warnings_bits_to_string_(data[23]).c_str());

  //  24   1  0x08                 State of charge                  %     1.0f
  ESP_LOGD(TAG, "  State of charge: %.0f %%", data[24] * 1.0f);

  //  25   1  0x19                 Unused
  //  26   1  0x00                 Unused
  //  27   1  0x00                 Unused
  //  28   1  0x6F                 CRC
  //  29   1  0x03                 CRC
  //  30   1  0x0D                 End of frame
  //  31   1  0x0A                 End of frame
}

void BasenBmsBle::decode_general_info_data_(const std::vector<uint8_t> &data) {
  auto basen_get_16bit = [&](size_t i) -> uint16_t {
    return (uint16_t(data[i + 1]) << 8) | (uint16_t(data[i + 0]) << 0);
  };
  auto basen_get_32bit = [&](size_t i) -> uint32_t {
    return (uint32_t(basen_get_16bit(i + 2)) << 16) | (uint32_t(basen_get_16bit(i + 0)) << 0);
  };

  ESP_LOGI(TAG, "General info frame (%d+4 bytes):", data.size());
  ESP_LOGD(TAG, "  %s", format_hex_pretty(&data.front(), data.size()).c_str());

  // Byte Len Payload              Description                      Unit  Precision
  //  0    1  0x3A                 Start of frame
  //  1    1  0x16                 Address
  //  2    1  0x2B                 Frame type
  //  3    1  0x18                 Data length
  //  4    4  0xA0 0x86 0x01 0x00  Nominal capacity                 Ah    0.001f
  ESP_LOGI(TAG, "  Nominal capacity: %.3f Ah", basen_get_32bit(4) * 0.001f);

  //  8    4  0x00 0x64 0x00 0x00  Nominal voltage                  V     0.001f
  ESP_LOGI(TAG, "  Nominal voltage: %.3f V", basen_get_32bit(8) * 0.001f);

  //  12   4  0x91 0xA0 0x01 0x00  Real capacity                    Ah    0.001f
  ESP_LOGI(TAG, "  Real capacity: %.3f Ah", basen_get_32bit(12) * 0.001f);

  //  16   1  0x00                 Unused
  //  17   1  0x00                 Unused
  //  18   1  0x00                 Unused
  //  19   1  0x00                 Unused
  //  20   1  0x30                 Unused
  //  21   1  0x75                 Unused
  //  22   2  0x00 0x00            Serial number
  ESP_LOGI(TAG, "  Serial number: %d", basen_get_16bit(22));

  //  24   2  0x71 0x53            Manufacturing date
  uint16_t raw_date = basen_get_16bit(24);
  ESP_LOGI(TAG, "  Manufacturing date: %d.%d.%d", ((raw_date >> 9) & 127) + 1980, (raw_date >> 5) & 15, 31 & raw_date);

  //  26   2  0x07 0x00            Charging cycles
  ESP_LOGI(TAG, "  Charging cycles: %d", basen_get_16bit(26));

  //  28   1  0x86                 CRC
  //  29   1  0x04                 CRC
  //  30   1  0x0D                 End of frame
  //  31   1  0x0A                 End of frame
}

void BasenBmsBle::decode_cell_voltages_data_(const std::vector<uint8_t> &data) {
  auto basen_get_16bit = [&](size_t i) -> uint16_t {
    return (uint16_t(data[i + 1]) << 8) | (uint16_t(data[i + 0]) << 0);
  };

  uint8_t offset = 12 * (data[2] - 36);
  uint8_t cells = data[3] / 2;

  ESP_LOGI(TAG, "Cell voltages frame (chunk %d, %d+4 bytes):", data[2] - 36, data.size());
  ESP_LOGD(TAG, "  %s", format_hex_pretty(&data.front(), data.size()).c_str());

  // Byte Len Payload              Description                      Unit  Precision
  //  0    1  0x3A                 Start of frame
  //  1    1  0x16                 Address
  //  2    1  0x24                 Frame type
  //  3    1  0x18                 Data length
  //  4    2  0x96 0x0C            Cell voltage 1
  //  6    2  0x97 0x0C            Cell voltage 2
  //  8    2  0x98 0x0C            Cell voltage 3
  //  10   2  0x96 0x0C            Cell voltage 4
  //  12   2  0x96 0x0C            Cell voltage 5
  //  14   2  0x98 0x0C            Cell voltage 6
  //  16   2  0x98 0x0C            Cell voltage 7
  //  18   2  0x97 0x0C            Cell voltage 8
  //  20   2  0x00 0x00            Cell voltage 9
  //  22   1  0x00 0x00            Cell voltage 10
  //  24   1  0x00 0x00            Cell voltage 11
  //  26   1  0x00 0x00            Cell voltage 12
  for (uint8_t i = 0; i < cells; i++) {
    ESP_LOGI(TAG, "  Cell voltage %d: %.3f V", i + 1 + offset, basen_get_16bit((i * 2) + 4) * 0.001f);
  }
  //  28   1  0x6A                 CRC
  //  29   1  0x05                 CRC
  //  30   1  0x0D                 End of frame
  //  31   1  0x0A                 End of frame
}

void BasenBmsBle::decode_protect_ic_data_(const std::vector<uint8_t> &data) {
  ESP_LOGI(TAG, "Protect IC frame (%d+4 bytes):", data.size());
  ESP_LOGI(TAG, "  %s", format_hex_pretty(&data.front(), data.size()).c_str());

  // Byte Len Payload              Description                      Unit  Precision
  //  0    1  0x3A                 Start of frame
  //  1    1  0x16                 Address
  //  2    1  0xFE                 Frame type
  //  3    1  0x13                 Data length
  //  4    1  0x01
  //  5    1  0x75
  //  6    1  0x08
  //  7    1  0x34
  //  8    1  0x80
  //  9    1  0x80
  //  10   1  0x00
  //  11   1  0x00
  //  12   1  0x80
  //  13   1  0x00                 Temperature 3 (if SOF is 0x3B)
  //  14   1  0x00
  //  15   1  0x00
  //  16   1  0x00
  //  17   1  0x00                 Temperature 4 (if SOF is 0x3B)
  //  18   1  0x00
  //  19   1  0x02
  //  20   1  0x76
  //  21   1  0x53
  //  22   1  0x61
  //  23   1  0x85                 CRC
  //  24   1  0x04                 CRC
  //  25   1  0x0D                 End of frame
  //  26   1  0x0A                 End of frame
}

void BasenBmsBle::dump_config() {
  ESP_LOGCONFIG(TAG, "BasenBmsBle:");
  ESP_LOGCONFIG(TAG, "  Fake traffic enabled: %s", YESNO(this->enable_fake_traffic_));

  LOG_SENSOR("", "Total voltage", this->total_voltage_sensor_);
}

void BasenBmsBle::publish_state_(binary_sensor::BinarySensor *binary_sensor, const bool &state) {
  if (binary_sensor == nullptr)
    return;

  binary_sensor->publish_state(state);
}

void BasenBmsBle::publish_state_(sensor::Sensor *sensor, float value) {
  if (sensor == nullptr)
    return;

  sensor->publish_state(value);
}

void BasenBmsBle::publish_state_(text_sensor::TextSensor *text_sensor, const std::string &state) {
  if (text_sensor == nullptr)
    return;

  text_sensor->publish_state(state);
}

void BasenBmsBle::write_register(uint8_t address, uint16_t value) {
  // this->send_command_(BASEN_CMD_WRITE, BASEN_CMD_MOS);  // @TODO: Pass value
}

bool BasenBmsBle::send_command_(uint8_t start_of_frame, uint8_t function, uint8_t value) {
  uint8_t frame[9];
  uint8_t data_len = 1;

  frame[0] = start_of_frame;
  frame[1] = BASEN_ADDRESS;
  frame[2] = function;
  frame[3] = data_len;
  frame[4] = value;
  auto crc = chksum_(frame + 1, 4);
  frame[5] = crc >> 0;
  frame[6] = crc >> 8;
  frame[7] = BASEN_PKT_END_1;
  frame[8] = BASEN_PKT_END_2;

  ESP_LOGV(TAG, "Send command (handle 0x%02X): %s", this->char_command_handle_,
           format_hex_pretty(frame, sizeof(frame)).c_str());

  auto status =
      esp_ble_gattc_write_char(this->parent_->get_gattc_if(), this->parent_->get_conn_id(), this->char_command_handle_,
                               sizeof(frame), frame, ESP_GATT_WRITE_TYPE_NO_RSP, ESP_GATT_AUTH_REQ_NONE);

  if (status) {
    ESP_LOGW(TAG, "[%s] esp_ble_gattc_write_char failed, status=%d", this->parent_->address_str().c_str(), status);
  }
  return (status == 0);
}

std::string BasenBmsBle::charging_states_bits_to_string_(const uint8_t mask) {
  std::string values = "";
  if (mask) {
    for (int i = 0; i < CHARGING_STATES_SIZE; i++) {
      if (mask & (1 << i)) {
        values.append(CHARGING_STATES[i]);
        values.append(";");
      }
    }
    if (!values.empty()) {
      values.pop_back();
    }
  }
  return values;
}

std::string BasenBmsBle::discharging_states_bits_to_string_(const uint8_t mask) {
  std::string values = "";
  if (mask) {
    for (int i = 0; i < DISCHARGING_STATES_SIZE; i++) {
      if (mask & (1 << i)) {
        values.append(DISCHARGING_STATES[i]);
        values.append(";");
      }
    }
    if (!values.empty()) {
      values.pop_back();
    }
  }
  return values;
}

std::string BasenBmsBle::charging_warnings_bits_to_string_(const uint8_t mask) {
  std::string values = "";
  if (mask) {
    for (int i = 0; i < CHARGING_WARNINGS_SIZE; i++) {
      if (mask & (1 << i)) {
        values.append(CHARGING_WARNINGS[i]);
        values.append(";");
      }
    }
    if (!values.empty()) {
      values.pop_back();
    }
  }
  return values;
}

std::string BasenBmsBle::discharging_warnings_bits_to_string_(const uint8_t mask) {
  std::string values = "";
  if (mask) {
    for (int i = 0; i < DISCHARGING_WARNINGS_SIZE; i++) {
      if (mask & (1 << i)) {
        values.append(DISCHARGING_WARNINGS[i]);
        values.append(";");
      }
    }
    if (!values.empty()) {
      values.pop_back();
    }
  }
  return values;
}

}  // namespace basen_bms_ble
}  // namespace esphome
