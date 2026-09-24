#include "esphome/core/component_iterator.h"
#include "esphome/core/log.h"
#include "esphome/components/light/color_mode.h"
#include "esphome/components/light/light_state.h"
#include "fastcon_controller.h"
#include "key_diagnostics.h"
#include "protocol.h"

#ifndef FASTCON_VERSION
#define FASTCON_VERSION "0.3.2-dev"
#endif

namespace esphome {
namespace fastcon {

static const char *const TAG = "fastcon.controller";

void FastconController::queueCommand(uint32_t light_id_, const std::vector<uint8_t> &data) {
  std::lock_guard<std::mutex> lock(queue_mutex_);
  if (queue_.size() >= max_queue_size_) {
    ESP_LOGW(TAG, "Command queue full (size=%d), dropping command for light %d", (int) queue_.size(),
             (int) light_id_);
    return;
  }

  Command cmd;
  cmd.data = data;
  cmd.timestamp = millis();
  cmd.retries = 0;

  queue_.push(cmd);
  ESP_LOGV(TAG, "Command queued, queue size: %d", (int) queue_.size());
}

void FastconController::clear_queue() {
  std::lock_guard<std::mutex> lock(queue_mutex_);
  std::queue<Command> empty;
  std::swap(queue_, empty);
}

void FastconController::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Fastcon BLE Controller...");
  ESP_LOGCONFIG(TAG, "  Advertisement interval: %d-%d", this->adv_interval_min_, this->adv_interval_max_);
  ESP_LOGCONFIG(TAG, "  Advertisement duration: %dms", this->adv_duration_);
  ESP_LOGCONFIG(TAG, "  Advertisement gap: %dms", this->adv_gap_);
  ESP_LOGCONFIG(TAG, "  MELPO GAP-driven advertising: enabled");
}

void FastconController::loop() {
  const uint32_t now = millis();

  switch (adv_state_) {
    case AdvertiseState::IDLE: {
      std::lock_guard<std::mutex> lock(queue_mutex_);
      if (queue_.empty())
        return;

      Command cmd = queue_.front();
      queue_.pop();

      this->adv_params_ = {
          .adv_int_min = adv_interval_min_,
          .adv_int_max = adv_interval_max_,
          .adv_type = ADV_TYPE_IND,
          .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
          .peer_addr = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
          .peer_addr_type = BLE_ADDR_TYPE_PUBLIC,
          .channel_map = ADV_CHNL_ALL,
          .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
      };

      uint8_t adv_data_raw[31] = {0};
      uint8_t adv_data_len = 0;

      // Flags. 0x02 matches the BRmesh/MELPO packet shape validated on hardware.
      adv_data_raw[adv_data_len++] = 2;
      adv_data_raw[adv_data_len++] = ESP_BLE_AD_TYPE_FLAG;
      adv_data_raw[adv_data_len++] = 0x02;

      // Manufacturer data.
      // BLE AD length includes: AD type (1) + manufacturer ID (2) + payload.
      adv_data_raw[adv_data_len++] = cmd.data.size() + 3;
      adv_data_raw[adv_data_len++] = ESP_BLE_AD_MANUFACTURER_SPECIFIC_TYPE;
      adv_data_raw[adv_data_len++] = MANUFACTURER_DATA_ID & 0xFF;
      adv_data_raw[adv_data_len++] = (MANUFACTURER_DATA_ID >> 8) & 0xFF;

      memcpy(&adv_data_raw[adv_data_len], cmd.data.data(), cmd.data.size());
      adv_data_len += cmd.data.size();

#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERY_VERBOSE
      std::string adv_hex;
      adv_hex.reserve(adv_data_len * 2);
      char byte_hex[3];
      for (uint8_t i = 0; i < adv_data_len; i++) {
        snprintf(byte_hex, sizeof(byte_hex), "%02X", adv_data_raw[i]);
        adv_hex += byte_hex;
      }
      ESP_LOGVV(TAG, "BLE advertisement (%u bytes): %s", adv_data_len, adv_hex.c_str());
#endif

      ESP_LOGVV(TAG, "CONFIG_RAW request t=%u len=%u interval=%u-%u type=%d", millis(), adv_data_len,
                adv_interval_min_, adv_interval_max_, (int) this->adv_params_.adv_type);

      const esp_err_t err = esp_ble_gap_config_adv_data_raw(adv_data_raw, adv_data_len);

      ESP_LOGVV(TAG, "CONFIG_RAW returned t=%u err=%d (%s)", millis(), err, esp_err_to_name(err));

      if (err != ESP_OK) {
        ESP_LOGW(TAG, "Error setting raw advertisement data (err=%d): %s", err, esp_err_to_name(err));
        return;
      }

      // esp_ble_gap_config_adv_data_raw() is asynchronous. Wait for
      // ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT before starting advertising.
      adv_state_ = AdvertiseState::CONFIGURING;
      state_start_time_ = millis();
      break;
    }

    case AdvertiseState::CONFIGURING:
    case AdvertiseState::STARTING:
    case AdvertiseState::STOPPING:
      // Completion is driven by gap_event_handler().
      break;

    case AdvertiseState::ADVERTISING: {
      if (now - state_start_time_ >= adv_duration_) {
        ESP_LOGVV(TAG, "STOP_ADV request t=%u elapsed=%u", millis(), millis() - state_start_time_);

        const esp_err_t err = esp_ble_gap_stop_advertising();

        ESP_LOGVV(TAG, "STOP_ADV returned t=%u err=%d (%s)", millis(), err, esp_err_to_name(err));

        if (err == ESP_OK) {
          adv_state_ = AdvertiseState::STOPPING;
          state_start_time_ = millis();
        } else {
          ESP_LOGW(TAG, "Error stopping advertisement (err=%d): %s", err, esp_err_to_name(err));
          adv_state_ = AdvertiseState::IDLE;
        }
      }
      break;
    }

    case AdvertiseState::GAP: {
      if (now - state_start_time_ >= adv_gap_) {
        adv_state_ = AdvertiseState::IDLE;
        ESP_LOGVV(TAG, "Advertisement gap complete t=%u", millis());
      }
      break;
    }
  }
}

void FastconController::gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
  switch (event) {
    case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT: {
      if (adv_state_ != AdvertiseState::CONFIGURING)
        break;

      ESP_LOGVV(TAG, "RAW_SET_COMPLETE t=%u", millis());

      const esp_err_t err = esp_ble_gap_start_advertising(&this->adv_params_);

      ESP_LOGVV(TAG, "START_ADV request returned t=%u err=%d (%s)", millis(), err, esp_err_to_name(err));

      if (err != ESP_OK) {
        ESP_LOGW(TAG, "Error starting advertisement (err=%d): %s", err, esp_err_to_name(err));
        adv_state_ = AdvertiseState::IDLE;
        break;
      }

      adv_state_ = AdvertiseState::STARTING;
      state_start_time_ = millis();
      break;
    }

    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT: {
      if (adv_state_ != AdvertiseState::STARTING)
        break;

      const auto status = param->adv_start_cmpl.status;
      ESP_LOGVV(TAG, "START_COMPLETE t=%u status=%d", millis(), (int) status);

      if (status != ESP_BT_STATUS_SUCCESS) {
        ESP_LOGE(TAG, "Advertising failed to start, status=%d", (int) status);
        adv_state_ = AdvertiseState::IDLE;
        break;
      }

      adv_state_ = AdvertiseState::ADVERTISING;
      state_start_time_ = millis();
      ESP_LOGVV(TAG, "Advertising active t=%u duration=%ums", state_start_time_, adv_duration_);
      break;
    }

    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT: {
      if (adv_state_ != AdvertiseState::STOPPING)
        break;

      const auto status = param->adv_stop_cmpl.status;
      ESP_LOGVV(TAG, "STOP_COMPLETE t=%u status=%d", millis(), (int) status);

      if (status != ESP_BT_STATUS_SUCCESS) {
        ESP_LOGE(TAG, "Advertising failed to stop cleanly, status=%d", (int) status);
        adv_state_ = AdvertiseState::IDLE;
        break;
      }

      adv_state_ = AdvertiseState::GAP;
      state_start_time_ = millis();
      ESP_LOGVV(TAG, "Advertisement gap started t=%u gap=%ums", state_start_time_, adv_gap_);
      break;
    }

    default:
      break;
  }
}


bool FastconController::decode_fastcon_rf_body_(const std::vector<uint8_t> &rf_payload,
                                                std::array<uint8_t, 16> &body) const {
  // Standard FastCon commands expose 24 bytes after the 0xFFF0 manufacturer ID:
  // 6-byte RF wrapper + 16-byte encoded body + 2-byte CRC.
  if (rf_payload.size() != 24)
    return false;

  // prepare_payload() whitens the entire RF buffer and then discards the first
  // 15 bytes. Whitening state evolution is data-independent, so prepend 15
  // dummy bytes to advance the same LFSR state and apply the XOR stream again.
  std::vector<uint8_t> work(15 + rf_payload.size(), 0);
  std::copy(rf_payload.begin(), rf_payload.end(), work.begin() + 15);

  WhiteningContext context;
  whitening_init(0x25, context);
  whitening_encode(work, context);

  const uint8_t *decoded = work.data() + 15;

  static const uint8_t expected_wrapper[6] = {0x8E, 0xF0, 0xAA, 0xC3, 0x43, 0x83};
  for (size_t i = 0; i < 6; i++) {
    if (decoded[i] != expected_wrapper[i])
      return false;
  }

  std::copy(decoded + 6, decoded + 22, body.begin());

  std::vector<uint8_t> addr = {DEFAULT_BLE_FASTCON_ADDRESS.begin(), DEFAULT_BLE_FASTCON_ADDRESS.end()};
  std::vector<uint8_t> encoded_body(body.begin(), body.end());
  const uint16_t expected_crc = static_cast<uint16_t>(decoded[22]) |
                                (static_cast<uint16_t>(decoded[23]) << 8);

  return crc16(addr, encoded_body) == expected_crc;
}

bool FastconController::decrypt_and_validate_body_(const std::array<uint8_t, 16> &body,
                                                   const std::array<uint8_t, 4> &key,
                                                   std::array<uint8_t, 4> &header,
                                                   std::array<uint8_t, 12> &payload) const {
  for (size_t i = 0; i < header.size(); i++)
    header[i] = body[i] ^ DEFAULT_ENCRYPT_KEY[i];

  for (size_t i = 0; i < payload.size(); i++)
    payload[i] = body[4 + i] ^ key[i & 3];

  uint8_t checksum = 0;
  checksum = static_cast<uint8_t>(checksum + header[0]);
  checksum = static_cast<uint8_t>(checksum + header[1]);
  checksum = static_cast<uint8_t>(checksum + header[2]);
  for (uint8_t value : payload)
    checksum = static_cast<uint8_t>(checksum + value);

  return checksum == header[3];
}

bool FastconController::recover_provision_key_(const std::array<uint8_t, 16> &body,
                                               std::array<uint8_t, 4> &key) const {
  std::array<uint8_t, 4> header{};
  std::array<uint8_t, 12> payload{};

  if (!this->decrypt_and_validate_body_(body, DEFAULT_ENCRYPT_KEY, header, payload))
    return false;

  const uint8_t command_type = (header[0] >> 4) & 0x07;
  const bool forward = (header[0] & 0x80) != 0;

  if (command_type != 2 || forward)
    return false;

  if (header[2] != DEFAULT_ENCRYPT_KEY[3])
    return false;

  // Provision payload:
  // device_id[6], light_id, group_id, new_mesh_key[4]
  if (payload[6] == 0)
    return false;

  std::copy(payload.begin() + 8, payload.begin() + 12, key.begin());
  return true;
}

bool FastconController::recover_control_key_(const std::array<uint8_t, 16> &body,
                                             std::array<uint8_t, 4> &key) const {
  // Normal single-light control packets are padded to 12 data bytes. Their
  // final four plaintext bytes are zero, so the final four encrypted bytes
  // reveal one complete repetition of the XOR mesh key.
  std::copy(body.begin() + 12, body.begin() + 16, key.begin());

  std::array<uint8_t, 4> header{};
  std::array<uint8_t, 12> payload{};

  if (!this->decrypt_and_validate_body_(body, key, header, payload))
    return false;

  const uint8_t command_type = (header[0] >> 4) & 0x07;
  const bool forward = (header[0] & 0x80) != 0;

  if (command_type != 5 || !forward)
    return false;

  if (header[2] != key[3])
    return false;

  // Only accept the well-understood single-light command forms whose padding
  // gives us the key directly.
  if (payload[0] != 0x22 && payload[0] != 0x72)
    return false;

  if (payload[1] == 0)
    return false;

  for (size_t i = 8; i < 12; i++) {
    if (payload[i] != 0)
      return false;
  }

  return true;
}

void FastconController::publish_detected_key_(const std::array<uint8_t, 4> &key, const char *source) {
  char key_hex[9];
  snprintf(key_hex, sizeof(key_hex), "%02X%02X%02X%02X", key[0], key[1], key[2], key[3]);

  char key_ascii[5] = {0};
  bool printable = true;
  for (size_t i = 0; i < key.size(); i++) {
    if (key[i] < 0x20 || key[i] > 0x7E) {
      printable = false;
      break;
    }
    key_ascii[i] = static_cast<char>(key[i]);
  }

  if (printable) {
    ESP_LOGI(TAG, "Validated FastCon mesh key from %s traffic (hex=%s, ascii=%s)", source, key_hex, key_ascii);
  } else {
    ESP_LOGI(TAG, "Validated FastCon mesh key from %s traffic (hex=%s)", source, key_hex);
  }

  if (this->detected_key_text_sensor_ != nullptr)
    this->detected_key_text_sensor_->publish_state(key_hex);

  // Capture is intentionally one-shot. The user can re-enable it for another
  // registration/control event without leaving key parsing enabled indefinitely.
  this->key_listener_enabled_ = false;
  if (this->key_listener_switch_ != nullptr)
    this->key_listener_switch_->publish_state(false);
}

bool FastconController::parse_device(const ble_device_base::ESPBTDevice &device) {
  if (!this->key_listener_enabled_)
    return false;

  for (const auto &manufacturer : device.get_manufacturer_datas()) {
    if (manufacturer.uuid.type() != ble_device_base::ESPBTUUID::Type::UUID16 ||
        manufacturer.uuid.uuid16() != MANUFACTURER_DATA_ID) {
      continue;
    }

    std::array<uint8_t, 16> body{};
    if (!this->decode_fastcon_rf_body_(manufacturer.data, body))
      continue;

    std::array<uint8_t, 4> key{};

    if (this->recover_provision_key_(body, key)) {
      this->publish_detected_key_(key, "provisioning");
      return true;
    }

    if (this->recover_control_key_(body, key)) {
      this->publish_detected_key_(key, "normal control");
      return true;
    }
  }

  return false;
}

// --- helpers for channel resolution ---
static inline uint8_t to8(float v) {
  if (v < 0.0f)
    v = 0.0f;
  if (v > 1.0f)
    v = 1.0f;
  return static_cast<uint8_t>(v * 255.0f + 0.5f);
}

static inline bool all_zero(float r, float g, float b, float cw, float ww) {
  return r == 0.0f && g == 0.0f && b == 0.0f && cw == 0.0f && ww == 0.0f;
}

std::vector<uint8_t> FastconController::get_light_data(light::LightState *state) {
  // Protocol: 6 bytes when ON
  // [0] 0x80 | (brightness 0..127)
  // [1] Blue, [2] Red, [3] Green, [4] Warm, [5] Cold
  // When OFF, a single 0x00 byte is returned.

  auto &values = state->current_values;
  const bool is_on = values.is_on();
  if (!is_on) {
    return std::vector<uint8_t>({0x00});
  }

  // Compute final channel levels from current values so brightness/color_brightness are applied.
  float r = 0, g = 0, b = 0, cw = 0, ww = 0;
  state->current_values_as_rgbww(&r, &g, &b, &cw, &ww, /*constant_brightness=*/false);

  // If color mode is WHITE on RGBW fixtures (no CW/WW), map to RGB white.
  const auto mode = values.get_color_mode();

  // Heuristic: if traits have a valid CT range, treat the device as supporting CW/WW.
  const bool supports_cwww = state->get_traits().get_min_mireds() > 0.0f;

  if ((mode == light::ColorMode::WHITE || mode == light::ColorMode::COLD_WARM_WHITE) && !supports_cwww) {
    float m = (ww > 0 ? ww : cw);
    r = g = b = m;
    cw = ww = 0.0f;
  }

  // Fallback for UNKNOWN color mode / zeroed channels.
  if (all_zero(r, g, b, cw, ww)) {
    if (supports_cwww) {
      ww = 1.0f;
    } else {
      r = g = b = 1.0f;
    }
  }

  const float blevel = std::min(values.get_brightness() * 127.0f, 127.0f);
  return {
      static_cast<uint8_t>(0x80 | static_cast<uint8_t>(blevel)),
      to8(b),
      to8(r),
      to8(g),
      to8(ww),
      to8(cw),
  };
}

// Special payload for white LED with color_interlock.
std::vector<uint8_t> FastconController::get_white_light_data(light::LightState *state) {
  auto &values = state->current_values;
  if (!values.is_on()) {
    return std::vector<uint8_t>({0x00});
  }

  const float blevel = std::min(values.get_brightness() * 127.0f, 127.0f);
  return {
      static_cast<uint8_t>(0x80 | static_cast<uint8_t>(blevel)),
      0,
      0,
      0,
      127,
      127,
  };
}

std::vector<uint8_t> FastconController::single_control(uint32_t light_id_,
                                                       const std::vector<uint8_t> &light_data,
                                                       const std::array<uint8_t, 4> &mesh_key) {
  std::vector<uint8_t> result_data(12);
  result_data[0] = 2 | (((0x0FFFFFF & (light_data.size() + 1)) << 4));
  result_data[1] = light_id_;
  std::copy(light_data.begin(), light_data.end(), result_data.begin() + 2);

  const auto hex_vec = vector_to_hex_string(result_data);
  const std::string hex(hex_vec.begin(), hex_vec.end());
  ESP_LOGVV(TAG, "Inner Payload v%s (%zu bytes): %s", FASTCON_VERSION, result_data.size(), hex.c_str());

  return this->generate_command(5, light_id_, result_data, mesh_key, true);
}

std::vector<uint8_t> FastconController::generate_command(uint8_t n, uint32_t light_id_,
                                                          const std::vector<uint8_t> &data,
                                                          const std::array<uint8_t, 4> &mesh_key,
                                                          bool forward) {
  static uint8_t sequence = 0;

  std::vector<uint8_t> body(data.size() + 4);
  uint8_t i2 = (light_id_ / 256);

  body[0] = (i2 & 0b1111) | ((n & 0b111) << 4) | (forward ? 0x80 : 0);
  body[1] = sequence++;
  if (sequence >= 255)
    sequence = 1;
  body[2] = mesh_key[3];

  std::copy(data.begin(), data.end(), body.begin() + 4);

  uint8_t checksum = 0;
  for (size_t i = 0; i < body.size(); i++) {
    if (i != 3)
      checksum = checksum + body[i];
  }
  body[3] = checksum;

  for (size_t i = 0; i < 4; i++) {
    body[i] = DEFAULT_ENCRYPT_KEY[i & 3] ^ body[i];
  }

  for (size_t i = 0; i < data.size(); i++) {
    body[4 + i] = mesh_key[i & 3] ^ body[4 + i];
  }

  std::vector<uint8_t> addr = {DEFAULT_BLE_FASTCON_ADDRESS.begin(), DEFAULT_BLE_FASTCON_ADDRESS.end()};
  return prepare_payload(addr, body);
}

}  // namespace fastcon
}  // namespace esphome
