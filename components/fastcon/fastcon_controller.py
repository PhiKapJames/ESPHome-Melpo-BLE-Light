import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import ble_device_base, esp32_ble, switch, text_sensor
from esphome.const import CONF_ID, ENTITY_CATEGORY_DIAGNOSTIC

DEPENDENCIES = ["esp32_ble"]

CONF_ADV_INTERVAL_MIN = "adv_interval_min"
CONF_ADV_INTERVAL_MAX = "adv_interval_max"
CONF_ADV_DURATION = "adv_duration"
CONF_ADV_GAP = "adv_gap"
CONF_MAX_QUEUE_SIZE = "max_queue_size"

CONF_DIAGNOSTICS = "diagnostics"
CONF_MESH_KEY_LISTENER = "mesh_key_listener"
CONF_DETECTED_MESH_KEY = "detected_mesh_key"

DEFAULT_ADV_INTERVAL_MIN = 0x20
DEFAULT_ADV_INTERVAL_MAX = 0x40
DEFAULT_ADV_DURATION = 50
DEFAULT_ADV_GAP = 10
DEFAULT_MAX_QUEUE_SIZE = 100

def AUTO_LOAD(config):
    auto_load = ["ble_device_base"]
    if CONF_DIAGNOSTICS in config:
        auto_load.extend(["switch", "text_sensor"])
    return auto_load


fastcon_ns = cg.esphome_ns.namespace("fastcon")
FastconController = fastcon_ns.class_(
    "FastconController",
    cg.Component,
    ble_device_base.ESPBTDeviceListener,
)
FastconKeyListenerSwitch = fastcon_ns.class_(
    "FastconKeyListenerSwitch",
    switch.Switch,
    cg.Component,
)

DIAGNOSTICS_SCHEMA = (
    cv.Schema(
        {
            cv.Required(CONF_MESH_KEY_LISTENER): switch.switch_schema(
                FastconKeyListenerSwitch,
                block_inverted=True,
                default_restore_mode="ALWAYS_OFF",
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ).extend(cv.COMPONENT_SCHEMA),
            cv.Required(CONF_DETECTED_MESH_KEY): text_sensor.text_sensor_schema(
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
        }
    )
    .extend(ble_device_base.BLE_DEVICE_SCHEMA)
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_ID, default="fastcon_controller"): cv.declare_id(
            FastconController
        ),
        cv.GenerateID(esp32_ble.CONF_BLE_ID): cv.use_id(esp32_ble.ESP32BLE),
        cv.Optional(
            CONF_ADV_INTERVAL_MIN, default=DEFAULT_ADV_INTERVAL_MIN
        ): cv.uint16_t,
        cv.Optional(
            CONF_ADV_INTERVAL_MAX, default=DEFAULT_ADV_INTERVAL_MAX
        ): cv.uint16_t,
        cv.Optional(CONF_ADV_DURATION, default=DEFAULT_ADV_DURATION): cv.uint16_t,
        cv.Optional(CONF_ADV_GAP, default=DEFAULT_ADV_GAP): cv.uint16_t,
        cv.Optional(
            CONF_MAX_QUEUE_SIZE, default=DEFAULT_MAX_QUEUE_SIZE
        ): cv.positive_int,
        cv.Optional(CONF_DIAGNOSTICS): DIAGNOSTICS_SCHEMA,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])

    parent = await cg.get_variable(config[esp32_ble.CONF_BLE_ID])
    esp32_ble.register_gap_event_handler(parent, var)

    await cg.register_component(var, config)

    if config[CONF_ADV_INTERVAL_MAX] < config[CONF_ADV_INTERVAL_MIN]:
        raise cv.Invalid(
            f"adv_interval_max ({config[CONF_ADV_INTERVAL_MAX]}) must be >= "
            f"adv_interval_min ({config[CONF_ADV_INTERVAL_MIN]})"
        )

    cg.add(var.set_adv_interval_min(config[CONF_ADV_INTERVAL_MIN]))
    cg.add(var.set_adv_interval_max(config[CONF_ADV_INTERVAL_MAX]))
    cg.add(var.set_adv_duration(config[CONF_ADV_DURATION]))
    cg.add(var.set_adv_gap(config[CONF_ADV_GAP]))
    cg.add(var.set_max_queue_size(config[CONF_MAX_QUEUE_SIZE]))

    if diagnostics := config.get(CONF_DIAGNOSTICS):
        cg.add_define("USE_FASTCON_KEY_DIAGNOSTICS")
        await ble_device_base.register_ble_device(var, diagnostics)

        listener_conf = diagnostics[CONF_MESH_KEY_LISTENER]
        listener = await switch.new_switch(listener_conf, var)
        await cg.register_component(listener, listener_conf)
        cg.add(var.set_key_listener_switch(listener))

        detected_key = await text_sensor.new_text_sensor(
            diagnostics[CONF_DETECTED_MESH_KEY]
        )
        cg.add(var.set_detected_key_text_sensor(detected_key))
