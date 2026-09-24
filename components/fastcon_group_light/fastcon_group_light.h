#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "esphome/components/fastcon/fastcon_controller.h"
#include "esphome/components/fastcon/refresh_control.h"
#include "esphome/components/light/light_output.h"
#include "esphome/components/light/light_state.h"
#include "esphome/core/component.h"

namespace esphome {
namespace fastcon_group_light {

class FastconGroupLight : public Component,
                          public light::LightOutput,
                          public fastcon::RefreshableFastconEntity {
 public:
  void set_controller(fastcon::FastconController *controller) { controller_ = controller; }
  void set_mesh_key(std::array<uint8_t, 4> key) { mesh_key_ = key; }
  void set_start_light_id(uint8_t id) { start_light_id_ = id; }
  void set_mask(uint8_t mask) { mask_ = mask; }

  void set_refresh_interval(uint32_t interval_ms) {
    refresh_interval_ = interval_ms;
    refresh_enabled_ = interval_ms != SCHEDULER_DONT_RUN;
  }

  void set_refresh_enabled(bool enabled) override;
  bool get_refresh_enabled() const override { return refresh_enabled_; }

  void set_refresh_interval_minutes(float minutes) override;
  float get_refresh_interval_minutes() const override;

  void dump_config() override;
  light::LightTraits get_traits() override;
  void setup_state(light::LightState *state) override;
  void write_state(light::LightState *state) override;

 protected:
  void update_refresh_schedule_();

  std::vector<uint8_t> build_encrypted_body_(
      uint8_t n, const std::vector<uint8_t> &data, bool forward = true);
  std::vector<uint8_t> prepare_standard_payload_(
      const std::vector<uint8_t> &body);
  std::vector<uint8_t> prepare_long_ble_payload_(
      const std::vector<uint8_t> &body);

  void queue_group_select_();
  void queue_group_control_(const std::vector<uint8_t> &light_data);

  fastcon::FastconController *controller_{nullptr};
  light::LightState *state_{nullptr};
  std::array<uint8_t, 4> mesh_key_{};
  uint32_t refresh_interval_{SCHEDULER_DONT_RUN};
  uint8_t start_light_id_{0};
  uint8_t mask_{0};
  uint8_t sequence_{1};
  bool refresh_enabled_{false};
};

}  // namespace fastcon_group_light
}  // namespace esphome
