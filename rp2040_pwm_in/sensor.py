import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import sensor
from esphome.const import (
    #CONF_COUNT_MODE,
    #CONF_FALLING_EDGE,
    #CONF_ID,
    CONF_NUMBER,
    CONF_PIN,
    #CONF_RISING_EDGE,
    #CONF_TOTAL,
    #CONF_VALUE,
    ICON_PULSE,
    STATE_CLASS_MEASUREMENT,
    #STATE_CLASS_TOTAL_INCREASING,
    UNIT_PULSES,
    UNIT_PULSES_PER_MINUTE,
)

ns = cg.esphome_ns.namespace("rp2040_pwm_in")

Rp2040PwmInSensor = ns.class_("Rp2040PwmInSensor", sensor.Sensor, cg.PollingComponent)

def validate_pin(value):
    value = pins.internal_gpio_input_pin_schema(value)
    if value[CONF_NUMBER] % 2 != 1:
        raise cv.Invalid("Pin must be odd for PWM input.")
    return value

CONFIG_SCHEMA = cv.All(
    sensor.sensor_schema(
        Rp2040PwmInSensor,
        unit_of_measurement=UNIT_PULSES_PER_MINUTE,
        icon=ICON_PULSE,
        accuracy_decimals=2,
        state_class=STATE_CLASS_MEASUREMENT,
    )
    .extend(
        {
            cv.Required(CONF_PIN): validate_pin,
        },
    )
    .extend(cv.polling_component_schema("60s")),
)

async def to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)

    pin = await cg.gpio_pin_expression(config[CONF_PIN])
    cg.add(var.set_pin(pin))
