#include "esphome/core/log.h"
#include "esphome/components/light/light_state.h"
#include "fastcon_controller.h"
#include "fastcon_light.h"

#ifndef FASTCON_VERSION
#define FASTCON_VERSION "0.3.3-dev"
#endif

namespace esphome {
namespace fastcon {

static const char *const TAG = "fastcon.light";

light::LightTraits FastconLight::get_traits() {
  light::LightTraits t;
  if (this->color_interlock_) {
    if (this->supports_cwww_) {
      t.set_supported_color_modes({light::ColorMode::RGB, light::ColorMode::COLD_WARM_WHITE});
    } else {
      t.set_supported_color_modes({light::ColorMode::RGB, light::ColorMode::WHITE});
    }
  } else {
    if (this->supports_cwww_) {
      t.set_supported_color_modes({light::ColorMode::RGB_COLD_WARM_WHITE});
    } else {
      t.set_supported_color_modes({light::ColorMode::RGB_WHITE});
    }
  }

  if (this->supports_cwww_) {
    t.set_min_mireds(153.0f);
    t.set_max_mireds(500.0f);
  }

  return t;
}

void FastconLight::setup_state(light::LightState *state) {
  this->state_ = state;

  if (this->refresh_interval_ == SCHEDULER_DONT_RUN) {
    ESP_LOGCONFIG(TAG, "Periodic state refresh disabled for light %u", (unsigned) this->light_id_);
    return;
  }

  ESP_LOGCONFIG(TAG, "Periodic state refresh for light %u: %" PRIu32 "s", (unsigned) this->light_id_,
                this->refresh_interval_ / 1000);

  this->set_interval("state_refresh", this->refresh_interval_, [this]() {
    if (this->state_ == nullptr || this->controller_ == nullptr)
      return;

    // A transition is already driving write_state(); don't inject another
    // hardware write in the middle of it.
    if (this->state_->is_transformer_active()) {
      ESP_LOGV(TAG, "Skipping periodic refresh for light %u: transition active", (unsigned) this->light_id_);
      return;
    }

    // If a real command is queued, it already represents a newer desired
    // state. Avoid adding a stale/duplicate periodic command behind it.
    if (!this->controller_->is_queue_empty()) {
      ESP_LOGV(TAG, "Skipping periodic refresh for light %u: FastCon queue busy", (unsigned) this->light_id_);
      return;
    }

    ESP_LOGD(TAG, "Reasserting current state for light %u", (unsigned) this->light_id_);
    this->write_state(this->state_);
  });
}

void FastconLight::write_state(light::LightState *state) {
  if (this->controller_ == nullptr) {
    ESP_LOGW(TAG, "No controller bound; dropping command");
    return;
  }

  std::vector<uint8_t> light_bytes;
  auto &values = state->current_values;

  // Determine if it's a white-only command.
  const bool is_white_only = values.get_color_mode() == light::ColorMode::WHITE;

  if (is_white_only) {
    ESP_LOGD(TAG, "Sending white-only command for light %u", (unsigned) this->light_id_);
    light_bytes = this->controller_->get_white_light_data(state);
  } else {
    ESP_LOGD(TAG, "Sending RGB/color command for light %u", (unsigned) this->light_id_);
    light_bytes = this->controller_->get_light_data(state);
  }

  std::vector<uint8_t> payload = this->controller_->single_control(this->light_id_, light_bytes);
  this->controller_->queueCommand(this->light_id_, payload);

  ESP_LOGD(TAG, "Queued state v%s: light_id=%u, payload_len=%d", FASTCON_VERSION, (unsigned) this->light_id_,
           (int) payload.size());
}

}  // namespace fastcon
}  // namespace esphome
