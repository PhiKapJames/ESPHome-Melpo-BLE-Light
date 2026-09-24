#pragma once

#ifdef USE_FASTCON_KEY_DIAGNOSTICS

#include "fastcon_controller.h"
#include "esphome/components/switch/switch.h"
#include "esphome/core/component.h"
#include "esphome/core/log.h"

namespace esphome {
namespace fastcon {

class FastconKeyListenerSwitch : public Component, public switch_::Switch {
 public:
  explicit FastconKeyListenerSwitch(FastconController *parent) : parent_(parent) {}

  void setup() override {
    this->parent_->set_key_listener_enabled(false);
    this->publish_state(false);
  }

  void dump_config() override {
    ESP_LOGCONFIG("fastcon.key_listener", "FastCon Mesh Key Listener '%s'", this->get_name().c_str());
  }

 protected:
  void write_state(bool state) override {
    this->parent_->set_key_listener_enabled(state);
    const bool actual_state = this->parent_->is_key_listener_enabled();
    this->publish_state(actual_state);
    ESP_LOGI("fastcon.key_listener", "FastCon mesh-key listener %s", actual_state ? "enabled" : "disabled");
  }

  FastconController *parent_;
};

}  // namespace fastcon
}  // namespace esphome

#endif  // USE_FASTCON_KEY_DIAGNOSTICS
