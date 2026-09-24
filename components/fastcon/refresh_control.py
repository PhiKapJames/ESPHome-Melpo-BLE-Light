import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import number, switch
from esphome.const import (
    CONF_MAX_VALUE,
    CONF_MIN_VALUE,
    CONF_STEP,
    ENTITY_CATEGORY_CONFIG,
    UNIT_MINUTE,
)

CONF_ENABLED = "enabled"
CONF_INTERVAL = "interval"

fastcon_ns = cg.esphome_ns.namespace("fastcon")
FastconRefreshSwitch = fastcon_ns.class_(
    "FastconRefreshSwitch", switch.Switch, cg.Component
)
FastconRefreshNumber = fastcon_ns.class_(
    "FastconRefreshNumber", number.Number, cg.Component
)


def _validate_interval_number(config):
    if config[CONF_MAX_VALUE] <= config[CONF_MIN_VALUE]:
        raise cv.Invalid("max_value must be greater than min_value")
    if config[CONF_STEP] <= 0:
        raise cv.Invalid("step must be greater than zero")
    return config


INTERVAL_SCHEMA = cv.All(
    number.number_schema(
        FastconRefreshNumber,
        entity_category=ENTITY_CATEGORY_CONFIG,
        unit_of_measurement=UNIT_MINUTE,
    ).extend(
        {
            cv.Optional(CONF_MIN_VALUE, default=1): cv.All(
                cv.float_, cv.Range(min=1)
            ),
            cv.Optional(CONF_MAX_VALUE, default=1440): cv.All(
                cv.float_, cv.Range(min=1)
            ),
            cv.Optional(CONF_STEP, default=1): cv.positive_float,
        }
    ),
    _validate_interval_number,
)

REFRESH_CONTROL_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ENABLED): switch.switch_schema(
            FastconRefreshSwitch,
            block_inverted=True,
            default_restore_mode="RESTORE_DEFAULT_ON",
            entity_category=ENTITY_CATEGORY_CONFIG,
        ),
        cv.Required(CONF_INTERVAL): INTERVAL_SCHEMA,
    }
)


async def register_refresh_control(parent, config):
    switch_conf = config[CONF_ENABLED]
    enabled = await switch.new_switch(switch_conf, parent)
    await cg.register_component(enabled, switch_conf)

    number_conf = config[CONF_INTERVAL]
    interval = await number.new_number(
        number_conf,
        parent,
        min_value=number_conf[CONF_MIN_VALUE],
        max_value=number_conf[CONF_MAX_VALUE],
        step=number_conf[CONF_STEP],
    )
    await cg.register_component(interval, number_conf)
