#include "esphome/core/component_iterator.h"
#include "esphome/core/log.h"
#include "esphome/components/light/color_mode.h"
#include "esphome/components/light/light_state.h"
#include "fastcon_controller.h"
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
