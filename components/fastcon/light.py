"""Light platform for FastCon BLE lights."""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import light
from esphome.const import CONF_COLOR_INTERLOCK, CONF_LIGHT_ID, CONF_OUTPUT_ID
from esphome.core import HexInt
from .fastcon_controller import FastconController

CONF_CONTROLLER_ID = "controller_id"
CONF_MESH_KEY = "mesh_key"
CONF_SUPPORTS_CWWW = "supports_cwww"
CONF_REFRESH_INTERVAL = "refresh_interval"

DEPENDENCIES = ["esp32_ble"]
AUTO_LOAD = ["light"]

fastcon_ns = cg.esphome_ns.namespace("fastcon")
FastconLight = fastcon_ns.class_("FastconLight", light.LightOutput, cg.Component)


def validate_hex_bytes(value):
    if not isinstance(value, str):
        raise cv.Invalid("Mesh key must be a string")
    value = value.replace(" ", "")
    if len(value) != 8:
        raise cv.Invalid("Mesh key must be exactly 8 hex characters")
    try:
        return HexInt(int(value, 16))
    except ValueError as err:
        raise cv.Invalid(f"Invalid hex value: {err}")


CONFIG_SCHEMA = cv.All(
    light.BRIGHTNESS_ONLY_LIGHT_SCHEMA
    .extend(
        {
            cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(FastconLight),
            cv.Required(CONF_LIGHT_ID): cv.int_range(min=1, max=255),
            cv.Required(CONF_MESH_KEY): validate_hex_bytes,
            cv.Optional(CONF_CONTROLLER_ID, default="fastcon_controller"): cv.use_id(
                FastconController
            ),
            cv.Optional(CONF_SUPPORTS_CWWW, default=False): cv.boolean,
            cv.Optional(CONF_COLOR_INTERLOCK, default=False): cv.boolean,
            cv.Optional(CONF_REFRESH_INTERVAL, default="never"): cv.update_interval,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID], config[CONF_LIGHT_ID])

    await cg.register_component(var, config)
    await light.register_light(var, config)

    controller = await cg.get_variable(config[CONF_CONTROLLER_ID])
    cg.add(var.set_controller(controller))

    mesh_key = config[CONF_MESH_KEY]
    key_bytes = [(mesh_key >> (i * 8)) & 0xFF for i in range(3, -1, -1)]
    cg.add(var.set_mesh_key(key_bytes))

    if config.get(CONF_SUPPORTS_CWWW):
        cg.add(var.set_supports_cwww(True))

    if config.get(CONF_COLOR_INTERLOCK):
        cg.add(var.set_color_interlock(True))

    cg.add(var.set_refresh_interval(config[CONF_REFRESH_INTERVAL]))
