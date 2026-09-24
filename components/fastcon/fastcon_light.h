#pragma once

#include <array>
#include <cstdint>

#include "esphome/components/light/light_output.h"
#include "esphome/core/component.h"
#include "refresh_control.h"

namespace esphome {
namespace fastcon {

class FastconController;

class FastconLight : public Component,
                     public light::LightOutput,
                     public RefreshableFastconEntity {
 public:
  FastconLight() = default;
  explicit FastconLight(int light_id) { this->light_id_ = static_cast<uint8_t>(light_id); }

  void set_controller(FastconController *c) { controller_ = c; }
  void set_mesh_key(std::array<uint8_t, 4> key) { mesh_key_ = key; }
  void set_light_id(uint8_t id) { light_id_ = id; }
  void set_supports_cwww(bool v) { supports_cwww_ = v; }
  void set_color_interlock(bool v) { color_interlock_ = v; }

  void set_refresh_interval(uint32_t interval_ms) {
    refresh_interval_ = interval_ms;
    refresh_enabled_ = interval_ms != SCHEDULER_DONT_RUN;
  }

  void set_refresh_enabled(bool enabled) override;
  bool get_refresh_enabled() const override { return refresh_enabled_; }

  void set_refresh_interval_minutes(float minutes) override;
  float get_refresh_interval_minutes() const override;

  light::LightTraits get_traits() override;
  void setup_state(light::LightState *state) override;
  void write_state(light::LightState *state) override;

 protected:
  void update_refresh_schedule_();

  FastconController *controller_{nullptr};
  light::LightState *state_{nullptr};
  std::array<uint8_t, 4> mesh_key_{};

  uint32_t refresh_interval_{SCHEDULER_DONT_RUN};
  uint8_t light_id_{0};
  bool supports_cwww_{false};
  bool color_interlock_{false};
  bool refresh_enabled_{false};
};

}  // namespace fastcon
}  // namespace esphome
