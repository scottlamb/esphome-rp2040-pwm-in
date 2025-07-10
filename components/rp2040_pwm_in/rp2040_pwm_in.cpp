#include "rp2040_pwm_in.h"

#include "esphome/core/log.h"
#include "hardware/pwm.h"

namespace esphome {
namespace rp2040_pwm_in {

namespace {

char const TAG[] = "rp2040_pwm_in";

#ifndef RP2040_PWM_IN_PINS
#error "RP2040_PWM_IN_PINS must be defined to use this component"
#endif

constexpr uint16_t PINS[] = { RP2040_PWM_IN_PINS };
uint32_t slices_in_use = 0;
volatile uint32_t wraparounds[sizeof(PINS)/sizeof(PINS[0])] = {0};

void wraparound_helper(uint32_t mask, int i) {}

template<class... Pins> void wraparound_helper(uint32_t mask, int i, uint16_t pin, Pins... rest) {
    const uint slice = PWM_GPIO_SLICE_NUM(pin);
    if (mask & (1 << slice)) {
        wraparounds[i]++;
    }
    wraparound_helper(mask, i+1, rest...);
}

/// RP2040 shared interrupt handler which counts wraparounds on all PWM slices of interest.
void wraparound_intr() {
    uint32_t mask = slices_in_use & pwm_get_irq_status_mask();

    // Use a template helper to do compile-time expansion of each slice to check.
    // This appears to produce good assembly, as verified with `disassemble --name wraparound_intr` in lldb.
    wraparound_helper(mask, 0, RP2040_PWM_IN_PINS);

    pwm_hw->intr = mask; // clear all IRQs.
}

}  // anonymous namespace

void Rp2040PwmInSensor::setup() {
  int pin = PINS[this->i_];
  if (pwm_gpio_to_channel(pin) != PWM_CHAN_B) {
      this->mark_failed("must be a PWM B channel (odd) pin");
      return;
  }
  int slice = pwm_gpio_to_slice_num(pin);
  if (pwm_hw->en & (1 << slice)) {
      this->mark_failed("PWM slice already in use");
      return;
  }

  pwm_config cfg = pwm_get_default_config();
  pwm_config_set_clkdiv_mode(&cfg, PWM_DIV_B_RISING);
  if (slices_in_use == 0) {
    irq_add_shared_handler(PWM_IRQ_WRAP, wraparound_intr, PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY);
    irq_set_enabled(PWM_IRQ_WRAP, true);
  }
  slices_in_use |= 1 << slice;
  pwm_init(slice, &cfg, true);
  pwm_clear_irq(slice);
  pwm_set_irq_enabled(slice, true);
  gpio_set_function(pin, GPIO_FUNC_PWM);

  pwm_set_enabled(slice, true);

  // `wakeups` and the PWM counter are now 0 and `last_hi_` and `last_lo_`
  // already match that. Set the `last_read_micros_` so the first update's
  // deltas will be correct.
  this->last_read_micros_ = micros();
}

void Rp2040PwmInSensor::dump_config() {
  ESP_LOGCONFIG(TAG, "RP2040 PWM IN:");
  ESP_LOGCONFIG(TAG, "  Pin: %u", PINS[this->i_]);
  LOG_UPDATE_INTERVAL(this);
}

void Rp2040PwmInSensor::update() {
  uint slice = pwm_gpio_to_slice_num(PINS[this->i_]);
  uint32_t slice_mask = 1u << slice;
  uint32_t volatile& w = wraparounds[this->i_];

  // Loop until we get a "good" read of `hi`, `lo`, and `now_micros` (see loop condition below).
  uint32_t hi;
  uint32_t next_hi = w;
  uint16_t lo;
  uint32_t now_micros;
  uint32_t next_now_micros = micros();
  do {
    now_micros = next_now_micros;
    hi = next_hi;
    lo = pwm_get_counter(slice);
    next_hi = w;
    next_now_micros = micros();
  } while (
    // If `hi` changed during the loop iteration, we don't know if the `lo` read
    // is consistent with the new or old value.
    hi != next_hi ||

    // As far as I can tell, the `CC` and `INTS` registers are updated
    // atomically, but the IRQ handler doesn't always preempt immediately, so
    // also check for pending IRQ. Again, we don't know if this wraparound
    // happened before or after the `lo` read.
    (pwm_get_irq_status_mask() & slice_mask) != 0 ||

    // If the time between the two reads is too long, try again to get a more
    // precise timestamp. This can happen because an IRQ handler (not
    // necessarily "ours") ran, possibly including an FreeRTOS task switch.
    // Typical elapsed time in the happy path is <5µs.
    (next_now_micros - now_micros >= 20)
  );

  uint64_t pulses = (static_cast<uint64_t>(hi) << 16) + lo;
  uint64_t last_pulses = (static_cast<uint64_t>(this->last_hi_) << 16) + this->last_lo_;
  uint64_t delta = (pulses - last_pulses) & ((1uLL << 48) - 1);
  uint32_t delta_micros = now_micros - this->last_read_micros_;

  float new_state = static_cast<float>(delta) / (static_cast<float>(delta_micros) / 1000000.);
  this->publish_state(new_state);
  this->last_hi_ = hi;
  this->last_lo_ = lo;
  this->last_read_micros_ = now_micros;
}

}  // namespace rp2040_pwm_in
}  // namespace esphome
