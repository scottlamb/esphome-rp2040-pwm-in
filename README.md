Proof-of-concept esphome component to use the PWM B slices as input on the RP2040/RP2350 to measure frequency of a PWM signal. Written to test my theory that the frequent watchdog restarts I'm seeing are due to the `pulse_counter` component's every-edge interrupt handler monopolizing the CPU at high (> MHz) frequencies, and using the hardware will solve this.

Limitations:

*   This will go badly if you try to use the same PWM slice twice, either via this component alone or
    via this and the `rp2040_pwm` (output) component. Maybe a PWM slice should be represented by some shared class?
    Or at least it should check if the PWM slice is already enabled?
*   It will have a discontinuity every 2^48 cycles because it subtracts `int64_t` values representing modular 48-bit values.
*   It can have a "torn read" problem between the PWM wraparounds (high 32 bits of the 48-bit values) and the low 16 bits.
    The interrupt handler doesn't run atomically with the PWM wraparound and there's no attempt yet to handle this.
*   It could in theory miss 65,536 values at a time if the interrupt handler doesn't run before the next interrupt fires.
    This doesn't seem likely but could be avoided with strategies as in some of the components below.
*   Inflexible. It could support rising vs trailing edge vs duty cycle measurement, dividers, total, etc.

License: MIT/Apache-2.0

Official Raspberry Pi documentation:

*   [measure_duty_cycle.c example](https://github.com/raspberrypi/pico-examples/blob/master/pwm/measure_duty_cycle/measure_duty_cycle.c)
*   [RP2040 PWM documentation](https://datasheets.raspberrypi.com/rp2040/rp2040-datasheet.pdf#section_pwm)
*   [Hardware APIs](https://www.raspberrypi.com/documentation/pico-sdk/hardware.html)

esphome components and documentation:

*   [external_components](https://esphome.io/components/external_components.html)
*   [rp2040_pio](https://github.com/esphome/esphome/tree/dev/esphome/components/rp2040_pio) and [rp2040_pio_led_strip](https://github.com/esphome/esphome/tree/dev/esphome/components/rp2040_pio_led_strip)
*   [rp2040_pwm](https://github.com/esphome/esphome/tree/dev/esphome/components/rp2040_pwm) — this is for output only at present
*   [pulse_counter](https://github.com/esphome/esphome/tree/dev/esphome/components/pulse_counter) — this has "software" (interrupt per pulse) and ESP-based hardware pulse counting implementations
*   [pulse_meter](https://github.com/esphome/esphome/tree/dev/esphome/components/pulse_meter) — also software, meant to be higher-resolution per [documentation](https://esphome.io/components/sensor/pulse_meter.html)

other:

*   [picofreq](https://iosoft.blog/2023/07/30/picofreq/)
*   [Building a frequency counter](https://rjk.codes/post/building-a-frequency-counter/)
*   [forum thread](https://forums.raspberrypi.com/viewtopic.php?t=367235#p2202421) on DMA transfers—has a novel approach to unlimited ring buffer
