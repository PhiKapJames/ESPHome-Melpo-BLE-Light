#pragma once

#ifdef USE_FASTCON_KEY_DIAGNOSTICS

#include "esphome/components/switch/switch.h"
#include "esphome/core/component.h"

namespace esphome {
namespace fastcon {

class FastconController;

class FastconKeyListenerSwitch : public Component, public switch_::Switch {
 public:
  explicit FastconKeyListenerSwitch(FastconController *parent) : parent_(parent) {}

  void setup() override;
  void dump_config() override;

 protected:
  void write_state(bool state) override;

  FastconController *parent_;
};

}  // namespace fastcon
}  // namespace esphome

#endif  // USE_FASTCON_KEY_DIAGNOSTICS
