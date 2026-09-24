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
    LOG_SWITCH("", "FastCon Mesh Key Listener", this);
  }

 protected:
  void write_state(bool state) override {
    this->parent_->set_key_listener_enabled(state);
    this->publish_state(state);
    ESP_LOGI("fastcon.key_listener", "FastCon mesh-key listener %s", state ? "enabled" : "disabled");
  }

  FastconController *parent_;
};

}  // namespace fastcon
}  // namespace esphome

#endif  // USE_FASTCON_KEY_DIAGNOSTICS
