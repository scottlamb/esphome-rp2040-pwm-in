esphome component to use the PWM B slices as input on the RP2040/RP2350 to measure frequency of a PWM signal. This can measure frequencies well beyond 1 MHz without any negative impact on the main loop or other components.

In contrast, the `pulse_counter` component uses an interrupt per pulse, which will cause watchdog timeouts by 100 kHz and milder negative impact at lower frequencies.

Limitations:

*   Only usable on PWM B channels (odd GPIO pin numbers).
*   Requires exclusive use of the PWM slice in question. For example, using GPIO 1 with this component means you can't use GPIOs 0, 16, or 17 with the `rp2040_pwm` (output) component, because these four pins share PWM slice 0.
*   There's no build-time enforcement that the same PWM slice isn't used by `rp2040_pwm_in` and `rp2040_pwm` components. There probably should be some shared notion managed by `components/rp2040`.
*   The pulse count and timestamp aren't checked atomically; we allow up to 20µs skew.
*   It could in theory miss 65,536 edges at a time if the interrupt handler doesn't run before the next interrupt fires. This doesn't seem likely but could be avoided with strategies as in some of the links below.
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
