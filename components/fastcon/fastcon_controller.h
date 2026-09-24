#pragma once

#include <array>
#include <mutex>
#include <queue>
#include <string>
#include <vector>

#include <esp_gap_ble_api.h>

#include "esphome/components/ble_device_base/ble_device.h"
#include "esphome/components/esp32_ble/ble.h"
#include "esphome/components/light/light_state.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/core/component.h"

namespace esphome {
namespace fastcon {

class FastconKeyListenerSwitch;

class FastconController : public Component, public ble_device_base::ESPBTDeviceListener {
 public:
  FastconController() = default;

  void setup() override;
  void loop() override;

  void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
  bool parse_device(const ble_device_base::ESPBTDevice &device) override;

  std::vector<uint8_t> get_light_data(light::LightState *state);
  std::vector<uint8_t> get_white_light_data(light::LightState *state);
  std::vector<uint8_t> single_control(uint32_t addr, const std::vector<uint8_t> &light_data,
                                      const std::array<uint8_t, 4> &mesh_key);

  void queueCommand(uint32_t light_id_, const std::vector<uint8_t> &data);

  void clear_queue();
  bool is_queue_empty() const {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    return queue_.empty();
  }
  size_t get_queue_size() const {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    return queue_.size();
  }
  void set_max_queue_size(size_t size) { max_queue_size_ = size; }

  void set_adv_interval_min(uint16_t val) { adv_interval_min_ = val; }
  void set_adv_interval_max(uint16_t val) {
    adv_interval_max_ = val;
    if (adv_interval_max_ < adv_interval_min_) {
      adv_interval_max_ = adv_interval_min_;
    }
  }
  void set_adv_duration(uint16_t val) { adv_duration_ = val; }
  void set_adv_gap(uint16_t val) { adv_gap_ = val; }

  void set_key_listener_enabled(bool enabled) { key_listener_enabled_ = enabled; }
  bool is_key_listener_enabled() const { return key_listener_enabled_; }
  void set_key_listener_switch(FastconKeyListenerSwitch *listener) { key_listener_switch_ = listener; }
  void set_detected_key_text_sensor(text_sensor::TextSensor *sensor) { detected_key_text_sensor_ = sensor; }

 protected:
  struct Command {
    std::vector<uint8_t> data;
    uint32_t timestamp;
    uint8_t retries{0};
    static constexpr uint8_t MAX_RETRIES = 3;
  };

  std::queue<Command> queue_;
  mutable std::mutex queue_mutex_;
  size_t max_queue_size_{100};

  enum class AdvertiseState {
    IDLE,
    CONFIGURING,
    STARTING,
    ADVERTISING,
    STOPPING,
    GAP,
  };

  AdvertiseState adv_state_{AdvertiseState::IDLE};
  uint32_t state_start_time_{0};
  esp_ble_adv_params_t adv_params_{};

  std::vector<uint8_t> generate_command(uint8_t n, uint32_t light_id_, const std::vector<uint8_t> &data,
                                        const std::array<uint8_t, 4> &mesh_key, bool forward = true);

  bool decode_fastcon_rf_body_(const std::vector<uint8_t> &rf_payload, std::array<uint8_t, 16> &body) const;
  bool decrypt_and_validate_body_(const std::array<uint8_t, 16> &body, const std::array<uint8_t, 4> &key,
                                  std::array<uint8_t, 4> &header, std::array<uint8_t, 12> &payload) const;
  bool recover_provision_key_(const std::array<uint8_t, 16> &body, std::array<uint8_t, 4> &key) const;
  bool recover_control_key_(const std::array<uint8_t, 16> &body, std::array<uint8_t, 4> &key) const;
  void publish_detected_key_(const std::array<uint8_t, 4> &key, const char *source);

  uint16_t adv_interval_min_{0x20};
  uint16_t adv_interval_max_{0x40};
  uint16_t adv_duration_{50};
  uint16_t adv_gap_{10};

  bool key_listener_enabled_{false};
  FastconKeyListenerSwitch *key_listener_switch_{nullptr};
  text_sensor::TextSensor *detected_key_text_sensor_{nullptr};

  static const uint16_t MANUFACTURER_DATA_ID = 0xfff0;
};

}  // namespace fastcon
}  // namespace esphome
