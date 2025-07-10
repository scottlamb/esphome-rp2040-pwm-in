import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import rp2040, sensor
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
    UNIT_HERTZ,
    UNIT_PULSES,
    #UNIT_PULSES_PER_MINUTE,
)
from esphome.core import CORE

ns = cg.esphome_ns.namespace("rp2040_pwm_in")

Rp2040PwmInSensor = ns.class_("Rp2040PwmInSensor", sensor.Sensor, cg.PollingComponent)

TAG_I = "pwm_in_index"
TAG_USED_PWM_SLICES = "used_pwm_slices"

_BANK0_GPIO_PINS_BY_BOARD = {
    "rpipico": 30,
    "rpipico2w": 30,
}


def _pwm_gpio_to_slice_num(pin):
    # This matches the C pico-sdk's `pwm_gpio_to_slice_num` function.
    if pin >= _BANK0_GPIO_PINS_BY_BOARD[CORE.data[rp2040.KEY_RP2040][rp2040.KEY_BOARD]]:
        raise ValueError(f"Pin {pin} is out of range for RP2xxx GPIO pins.")
    if pin < 32:
        return (pin >> 1) & 7
    return 8 + ((pin >> 1) & 3)


def _pwm_gpio_to_channel(pin):
    # This matches the C pico-sdk's `pwm_gpio_to_channel` function.
    if pin >= _BANK0_GPIO_PINS_BY_BOARD[CORE.data[rp2040.KEY_RP2040][rp2040.KEY_BOARD]]:
        raise ValueError(f"Pin {pin} is out of range for RP2xxx GPIO pins.")
    return pin & 1


def _validate(config):
    pins = CORE.data.setdefault("rp2040_pwm_in", [])
    pin = config[CONF_PIN]
    if _pwm_gpio_to_channel(pin) != 1:
        return cv.Invalid(f"Pin {pin} must be a PWM input pin (odd numbered GPIO pin).")
    pwm_slice = _pwm_gpio_to_slice_num(pin)
    used_pwms = CORE.data.setdefault(TAG_USED_PWM_SLICES, set())
    if pwm_slice in used_pwms:
        return cv.Invalid(f"Pin {pin} needs PWM slice {pwm_slice}, which is already in use.")
    used_pwms.add(pwm_slice)
    config[TAG_I] = len(pins)
    pins.append(config[CONF_PIN])
    return config


CONFIG_SCHEMA = cv.All(
    sensor.sensor_schema(
        Rp2040PwmInSensor,
        unit_of_measurement=UNIT_HERTZ,
        icon=ICON_PULSE,
        accuracy_decimals=0,
        state_class=STATE_CLASS_MEASUREMENT,
    )
    .extend(
        {
            cv.Required(CONF_PIN): pins.internal_gpio_input_pin_number,
        },
    )
    .extend(cv.polling_component_schema("60s")),
    _validate,
)


async def to_code(config):
    i = config[TAG_I]
    if i == 0:
        pins = CORE.data["rp2040_pwm_in"]
        cg.add_define("RP2040_PWM_IN_PINS", cg.RawExpression(",".join(str(p) for p in pins)))
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)
    cg.add(var.set_i(i))
