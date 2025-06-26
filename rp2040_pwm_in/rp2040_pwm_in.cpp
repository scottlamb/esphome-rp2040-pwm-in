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
  pwm_config_set_clkdiv_int(&cfg, 1);
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
  int now_micros = micros();
  int slice = pwm_gpio_to_slice_num(this->pin_->get_pin());

  // XXX: possibility of torn read.
  // XXX: incorrect wraparounds wraparound.
  uint64_t pulses =
      (static_cast<uint64_t>(wraparounds[slice]) << 16) |
      pwm_get_counter(slice);
  int delta_micros = now_micros - this->last_read_micros_;
  uint64_t pulses_delta = pulses - this->last_pulses_;
  this->publish_state(
    static_cast<float>(pulses_delta) /
    (static_cast<float>(delta_micros) / 60000000.)
  );
  this->last_pulses_ = pulses;
  this->last_read_micros_ = now_micros;
}

}
}
