#pragma once

#include <cstdint>

#include "esphome/components/number/number.h"
#include "esphome/components/switch/switch.h"
#include "esphome/core/component.h"
#include "esphome/core/preferences.h"

namespace esphome {
namespace fastcon {

class RefreshableFastconEntity {
 public:
  virtual ~RefreshableFastconEntity() = default;

  virtual void set_refresh_enabled(bool enabled) = 0;
  virtual bool get_refresh_enabled() const = 0;

  virtual void set_refresh_interval_minutes(float minutes) = 0;
  virtual float get_refresh_interval_minutes() const = 0;
};

class FastconRefreshSwitch : public Component, public switch_::Switch {
 public:
  explicit FastconRefreshSwitch(RefreshableFastconEntity *parent) : parent_(parent) {}

  void setup() override;
  void dump_config() override;

 protected:
  void write_state(bool state) override;

  RefreshableFastconEntity *parent_;
};

class FastconRefreshNumber : public Component, public number::Number {
 public:
  explicit FastconRefreshNumber(RefreshableFastconEntity *parent) : parent_(parent) {}

  void setup() override;
  void dump_config() override;

 protected:
  void control(float value) override;

  RefreshableFastconEntity *parent_;
  ESPPreferenceObject pref_;
};

}  // namespace fastcon
}  // namespace esphome
