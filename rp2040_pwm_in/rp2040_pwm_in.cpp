#include "rp2040_pwm_in.h"

#include "esphome/core/log.h"
#include "hardware/pwm.h"

namespace esphome {
namespace rp2040_pwm_in {

namespace {

const char *const TAG = "rp2040_pwm_in";

uint32_t slices_in_use = 0;
volatile uint32_t wraparounds[NUM_PWM_SLICES] = {0};

void wraparound_intr() {
    int mask = slices_in_use | pwm_get_irq_status_mask();
    for (int i = 0; i < NUM_PWM_SLICES; ++i) {
        if (mask & (1 << i)) {
          wraparounds[i]++;
        }
    }
    pwm_hw->intr = mask; // clear all IRQs.
}

}

void Rp2040PwmInSensor::setup() {
  int gpio = this->pin_->get_pin();
  assert(pwm_gpio_to_channel(gpio) == PWM_CHAN_B);

  pwm_config cfg = pwm_get_default_config();
  pwm_config_set_clkdiv_mode(&cfg, PWM_DIV_B_RISING);
  if (slices_in_use == 0) {
    irq_add_shared_handler(PWM_IRQ_WRAP, wraparound_intr, PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY);
    irq_set_enabled(PWM_IRQ_WRAP, true);
  }
  int slice = pwm_gpio_to_slice_num(gpio);
  slices_in_use |= (1 << slice);
  pwm_init(slice, &cfg, true);
  pwm_clear_irq(slice);
  pwm_set_irq_enabled(slice, true);
  gpio_set_function(gpio, GPIO_FUNC_PWM);

  // `wakeups` and the PWM counter are now 0 and `last_hi_` and `last_lo_` already match that.
  // Set the `last_read_micros_` so the first update's deltas will be correct.
  this->last_read_micros_ = micros();
  pwm_set_enabled(slice, true);
}

void Rp2040PwmInSensor::dump_config() {
  ESP_LOGCONFIG(TAG, "RP2040 PWM IN:");
  LOG_PIN("  Pin: ", this->pin_);
  LOG_UPDATE_INTERVAL(this);
}

void Rp2040PwmInSensor::update() {
  int slice = pwm_gpio_to_slice_num(this->pin_->get_pin());
  int slice_mask = 1 << slice;

  uint32_t now_micros = micros();
  uint32_t hi = wraparounds[slice];
  uint16_t lo = pwm_get_counter(slice);
  if (pwm_get_irq_status_mask() & (1 << slice) || wraparounds[slice] != hi) {
      // Haven't caught this in action, but in theory it should happen sometimes.
      // And we probably should use a loop, similar to the SDK's `timer_time_us_64`.
      ESP_LOGW(TAG, "[%s] hi correction", get_name().c_str());
      hi++;
  }

  uint64_t pulses = (static_cast<uint64_t>(hi) << 16) + lo;
  uint64_t last_pulses = (static_cast<uint64_t>(this->last_hi_) << 16) + this->last_lo_;
  uint64_t delta = pulses - last_pulses & 0xFFFFFFFFFFFFULL;
  uint32_t delta_micros = now_micros - this->last_read_micros_;

  float new_state = static_cast<float>(delta) / (static_cast<float>(delta_micros) / 60000000.);
  this->publish_state(new_state);
  this->last_hi_ = hi;
  this->last_lo_ = lo;
  this->last_read_micros_ = now_micros;
}

}
}
