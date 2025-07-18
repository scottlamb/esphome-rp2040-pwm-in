#pragma once

// #ifdef USE_RP2040

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace rp2040_pwm_in {

class Rp2040PwmInSensor : public sensor::Sensor, public PollingComponent {
 public:
  void setup() override;
  void set_i(uint16_t i) { this->i_ = i; }
  void dump_config() override;
  void update() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

 private:
  uint32_t last_read_micros_ = 0;
  uint32_t last_hi_ = 0;
  uint16_t last_lo_ = 0;
  uint16_t i_ = -1;
};

}
}

// #endif
