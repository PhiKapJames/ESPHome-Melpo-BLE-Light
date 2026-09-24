#include "key_diagnostics.h"

#include "fastcon_controller.h"
#include "esphome/core/log.h"

namespace esphome {
namespace fastcon {

static const char *const TAG = "fastcon.key_listener";

void FastconKeyListenerSwitch::setup() {
  this->parent_->set_key_listener_enabled(false);
  this->publish_state(false);
}

void FastconKeyListenerSwitch::dump_config() {
  LOG_SWITCH("", "FastCon Mesh Key Listener", this);
}

void FastconKeyListenerSwitch::write_state(bool state) {
  this->parent_->set_key_listener_enabled(state);
  this->publish_state(state);

  if (state) {
    ESP_LOGI(TAG, "FastCon mesh-key listener enabled");
  } else {
    ESP_LOGI(TAG, "FastCon mesh-key listener disabled");
  }
}

}  // namespace fastcon
}  // namespace esphome
