#include "refresh_control.h"

#include <algorithm>

#include "esphome/core/log.h"

namespace esphome {
namespace fastcon {

static const char *const TAG = "fastcon.refresh";

void FastconRefreshSwitch::setup() {
  bool enabled = this->parent_->get_refresh_enabled();

  auto restored = this->get_initial_state_with_restore_mode();
  if (restored.has_value())
    enabled = restored.value();

  this->parent_->set_refresh_enabled(enabled);
  this->publish_state(enabled);
}

void FastconRefreshSwitch::dump_config() {
  LOG_SWITCH("", "FastCon Refresh", this);
}

void FastconRefreshSwitch::write_state(bool state) {
  this->parent_->set_refresh_enabled(state);
  this->publish_state(state);
}

void FastconRefreshNumber::setup() {
  float value = this->parent_->get_refresh_interval_minutes();

  this->pref_ = this->make_entity_preference<float>();
  float restored;
  if (this->pref_.load(&restored))
    value = restored;

  value = std::max(this->traits.get_min_value(), std::min(this->traits.get_max_value(), value));

  this->parent_->set_refresh_interval_minutes(value);
  this->publish_state(value);
}

void FastconRefreshNumber::dump_config() {
  LOG_NUMBER("", "FastCon Refresh Interval", this);
}

void FastconRefreshNumber::control(float value) {
  value = std::max(this->traits.get_min_value(), std::min(this->traits.get_max_value(), value));

  this->parent_->set_refresh_interval_minutes(value);
  this->pref_.save(&value);
  this->publish_state(value);
}

}  // namespace fastcon
}  // namespace esphome
