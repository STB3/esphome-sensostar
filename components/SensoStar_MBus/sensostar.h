#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif
#include "esphome/components/uart/uart.h"
#include "esphome/components/output/binary_output.h" // LED related

#include <vector>

namespace esphome {
namespace sensostar {

class SensoStarComponent : public PollingComponent, public uart::UARTDevice {
 public:
  SensoStarComponent() = default;

#ifdef USE_SENSOR
  SUB_SENSOR(energy)
  SUB_SENSOR(volume)
  SUB_SENSOR(power)
  SUB_SENSOR(flow)
  SUB_SENSOR(temperature_flow)
  SUB_SENSOR(temperature_return)
  SUB_SENSOR(temperature_diff)
  SUB_SENSOR(calculated_power)
#endif

#ifdef USE_TEXT_SENSOR
  SUB_TEXT_SENSOR(status)
#endif

  void set_data_led(output::BinaryOutput *data_led) { this->data_led_ = data_led; } // flash LED 
  
  void setup() override;
  void dump_config() override;
  void update() override;
  void loop() override;

  float get_setup_priority() const override;

 protected:
  void publish_nans_();
  void flash_data_led_(); // function for flashing the LED on updated values
    
  std::vector<uint8_t> data_;
  uint32_t last_transmission_{0};
  uint16_t last_send_tx_offset_{0};
  uint8_t receiving_{0};
  uint8_t init_state_{0};
  bool trigger_next_{true};
  bool FCB_;

  output::BinaryOutput *data_led_{nullptr}; // LED related
  uint32_t data_led_off_time_{0}; // LED related
};

}  // namespace sensostar
}  // namespace esphome

