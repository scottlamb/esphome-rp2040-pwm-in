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
            pwm_clear_irq(i);
        }
    }
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
  pwm_set_enabled(slice, true);
  gpio_set_function(gpio, GPIO_FUNC_PWM);
}

void Rp2040PwmInSensor::dump_config() {
  ESP_LOGCONFIG(TAG, "RP2040 PWM IN:");
  LOG_PIN("  Pin: ", this->pin_);
  LOG_UPDATE_INTERVAL(this);
}

void Rp2040PwmInSensor::update() {
  int slice = pwm_gpio_to_slice_num(this->pin_->get_pin());

  // Let's try to minimize skew between read of `now_micros`, `hi`, and `lo`.
  uint32_t now_micros = micros();
  uint32_t hi = wraparounds[slice];
  uint16_t lo = pwm_get_counter(slice);
  uint32_t delta_micros = now_micros - this->last_read_micros_;

  if (lo < this->last_lo_ && hi == this->last_hi_) {
      // `lo` appears to have wrapped around but `wraparound_intr` has not run yet.
      if (pwm_get_irq_status_mask() & (1 << slice)) {
          hi++;
          ESP_LOGW(TAG, "saw queued wraparound, surprising! now=%u then=%u hi=%u lo=%u last_lo_=%u", now_micros, this->last_read_micros_, hi, lo, this->last_lo_);
      } else {
          int new_hi = wraparounds[slice];
          if (new_hi == hi) {
              ESP_LOGW(TAG, "repeat hi! now=%u then=%u hi=%u lo=%u last_lo_=%u", now_micros, this->last_read_micros_, hi, lo, this->last_lo_);
              hi++;
          } else {
              // This is the unsurprising case.
              ESP_LOGD(TAG, "wraparound irq lagged behind, now=%u then=%u hi=%u lo=%u last_lo_=%u", now_micros, this->last_read_micros_, hi, lo, this->last_lo_);
              hi = new_hi;
          }
      }
  }

  // The intermediate assignment of `lo_delta` isn't just for readability; it avoids incorrect wraparound due to C++'s automatic promotion of `uint16_t` to `uint32_t`.
  uint32_t hi_delta = hi - this->last_hi_;
  uint16_t lo_delta = lo - this->last_lo_;

  this->publish_state(
    static_cast<float>((static_cast<uint64_t>(hi_delta) << 16) | static_cast<uint64_t>(lo_delta)) /
    (static_cast<float>(delta_micros) / 60000000.)
  );
  this->last_hi_ = hi;
  this->last_lo_ = lo;
  this->last_read_micros_ = now_micros;
}

}
}
