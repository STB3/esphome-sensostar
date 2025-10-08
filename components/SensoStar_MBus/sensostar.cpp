#include "sensostar.h"
#include <vector>
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace sensostar {

static const char *const TAG = "SensoStar";

void SensoStarComponent::setup() { }

void SensoStarComponent::dump_config() {
    ESP_LOGCONFIG(TAG, "SensoStar M-Bus:");
    this->check_uart_settings(2400, 1, uart::UART_CONFIG_PARITY_EVEN, 8);
    if (this->data_led_ != nullptr) {
        ESP_LOGCONFIG(TAG, "  Data LED: Present");
    }	
}

void SensoStarComponent::update() {
    this->trigger_next_ = true;
}

static char format_hex_pretty_char(uint8_t v) { return v >= 10 ? 'A' + (v - 10) : '0' + v; }
static double convert_value(uint32_t data, int8_t decimals){
    double value = (int32_t)data;
    for (int8_t i=0; i<decimals; i++) value *= 10;
    for (int8_t i=decimals; i<0; i++) value /= 10;
    return value;
}

void SensoStarComponent::publish_nans_(){
#ifdef USE_TEXT_SENSOR
    if (this->status_text_sensor_)
        this->status_text_sensor_->publish_state("No readout from heat meter");
#endif
#ifdef USE_SENSOR
    if (this->energy_sensor_)
        this->energy_sensor_->publish_state(NAN);
    if (this->volume_sensor_)
        this->volume_sensor_->publish_state(NAN);
    if (this->power_sensor_)
        this->power_sensor_->publish_state(NAN);
    if (this->flow_sensor_)
        this->flow_sensor_->publish_state(NAN);
    if (this->temperature_flow_sensor_)
        this->temperature_flow_sensor_->publish_state(NAN);
    if (this->temperature_return_sensor_)
        this->temperature_return_sensor_->publish_state(NAN);
    if (this->temperature_diff_sensor_)
        this->temperature_diff_sensor_->publish_state(NAN);
    if (this->calculated_power_sensor_)
        this->calculated_power_sensor_->publish_state(NAN);
#endif
}

void SensoStarComponent::flash_data_led_() {
    if (this->data_led_ != nullptr) {
        this->data_led_->turn_on();
        this->data_led_off_time_ = millis() + 200; // Set turn-off time 200ms in the future
    }
}

void SensoStarComponent::loop() {
    const uint32_t now = millis();

    // Turn off data LED if the time has passed
    if (this->data_led_ != nullptr && this->data_led_off_time_ != 0 && now >= this->data_led_off_time_) {
        this->data_led_->turn_off();
        this->data_led_off_time_ = 0;
    }

    if ( this->receiving_ > 0 && ( now - this->last_transmission_ >= 500 ) ) {
        ESP_LOGW(TAG, "Last transmission too long ago. Reset RX index.");
        this->publish_nans_();
        this->data_.clear();
        this->receiving_ = 0;
        if (this->init_state_ < 5) {
            this->trigger_next_ = true;
        }
    }
  
    if ( available() )
        this->last_transmission_ = now;
   
    while ( available() ) {
        uint8_t c;
        read_byte(&c);
        
        if (this->init_state_ < 5) {
            ESP_LOGV(TAG, "M-Bus received: %02x", c);
            continue;
        }
        
        if (this->receiving_ != 2) {
            if (c != 0x68)
                continue;
                
            this->receiving_ = 2;
        }
        this->data_.push_back(c);
        
        
        if ( this->data_.size() > 4 && this->data_.size() == this->data_[1]+6 ) {
                       
            ESP_LOGV(TAG, "M-Bus received: %s", format_hex_pretty(this->data_).c_str());
            
            if (this->data_[0] != 0x68 || this->data_[3] != 0x68){
                this->publish_nans_();
                ESP_LOGW(TAG, "Invalid frame start");
            }
            else if (this->data_[1] != this->data_[2]){
                this->publish_nans_();
                ESP_LOGW(TAG, "Invalid length");
            }
            else if (this->data_[this->data_.size()-1] != 0x16){
                this->publish_nans_();
                ESP_LOGW(TAG, "Invalid stop");
            }
            else {
                uint8_t checksum = 0;
                for (uint8_t i = 4; i < this->data_.size()-2; i++)
                    checksum += this->data_[i];
                
                if (checksum != this->data_[this->data_.size()-2]){
                    this->publish_nans_();
                    ESP_LOGW(TAG, "Invalid checksum");
                }
                else if (this->data_[4] != 0x08 || this->data_[6] != 0x72 ){
                    this->publish_nans_();
                    ESP_LOGW(TAG, "Unknown frame");
                }
                else {
                    // Store values for calculated_power
                    double flow = -127;
                    double tdiff = -127;
                    // Decode
					this->flash_data_led_(); // Flash LED when new data arrived
                    uint8_t i = 19; // Skip start header and fixed data header
                    while(i < this->data_.size()-3){
                        
                        // Get all indexes
                        uint8_t i_DIF = i;
                        while( (this->data_[i] & 0x80) == 0x80)
                            i++;
                        i++;
                        uint8_t i_VIF = i;
                        while( (this->data_[i] & 0x80) == 0x80)
                            i++;
                        i++;
                        
                        // Read data
                        uint8_t lc = this->data_[i_DIF]&0x0f; // Length and coding
                        uint32_t result = 0;
                        if (lc == 0x01){ // 8 Bit Integer
                            result = this->data_[i];
                            if ((this->data_[i]&0x80) == 0x80) // Negative
                                result |= 0xffffff00;
                            i += 1;
                        }
                        else if (lc == 0x02){ // 16 Bit Integer	
                            result = this->data_[i] | this->data_[i+1]<<8;
                            if ((this->data_[i+1]&0x80) == 0x80) // Negative
                                result |= 0xffff0000;
                            i += 2;
                        }
                        else if (lc == 0x03){ // 24 Bit Integer	
                            result = this->data_[i] | this->data_[i+1]<<8 | this->data_[i+2]<<16;
                            if ((this->data_[i+2]&0x80) == 0x80) // Negative
                                result |= 0xff000000;
                            i += 3;
                        }
                        else if (lc == 0x04){ // 32 Bit Integer
                            result = this->data_[i] | this->data_[i+1]<<8 | this->data_[i+2]<<16 | this->data_[i+3]<<24;
                            i += 4;
                        }
                        else {
                            ESP_LOGW(TAG, "Unsupported coding 0x%02x", lc);
                            break;
                        }
                        // Parce
                        // DIF
                        uint8_t f = ( this->data_[i_DIF]&0x30 ) >> 4; // Function (normal 0x00 for instantaneous value)
                        // VIF
                        uint8_t vif = this->data_[i_VIF] & 0x7f;
                        if ( (vif&0x78) == 0x00 && f == 0x00){ // Energy (Wh)
#ifdef USE_SENSOR
                            if (this->energy_sensor_)
                                this->energy_sensor_->publish_state(convert_value(result, (vif&0x07) - 3) / 1000);
#endif
                        }
                        else if ( (vif&0x78) == 0x10 && f == 0x00){ // Volume (m3)
#ifdef USE_SENSOR
                            if (this->volume_sensor_)
                                this->volume_sensor_->publish_state(convert_value(result, (vif&0x07) - 6));
#endif
                        }
                        else if ( (vif&0x78) == 0x28 && f == 0x00){ // Power (W)
                            int8_t decimals = (vif&0x07) - 3;
                            double val = ((int32_t)result) * std::pow(10, decimals);
#ifdef USE_SENSOR
                            if (this->power_sensor_)
                                this->power_sensor_->publish_state(convert_value(result, (vif&0x07) - 3));
#endif
                        }
                        else if ( (vif&0x78) == 0x38 && f == 0x00){ // Volume Flow (m3/h)
#ifdef USE_SENSOR
                            flow = convert_value(result, (vif&0x07) - 6);
                            if (this->flow_sensor_)
                                this->flow_sensor_->publish_state(flow);
#endif
                        }
                        else if ( (vif&0x7C) == 0x58 && f == 0x00){ // Flow Temperature (C)
#ifdef USE_SENSOR
                            if (this->temperature_flow_sensor_)
                                this->temperature_flow_sensor_->publish_state(convert_value(result, (vif&0x03) - 3));
#endif
                        }
                        else if ( (vif&0x7C) == 0x5c && f == 0x00){ // Return Temperature (C)
#ifdef USE_SENSOR
                            if (this->temperature_return_sensor_)
                                this->temperature_return_sensor_->publish_state(convert_value(result, (vif&0x03) - 3));
#endif
                        }
                        else if ( (vif&0x7C) == 0x60 && f == 0x00){ // Temperature Difference (K)
#ifdef USE_SENSOR
                            tdiff = convert_value(result, (vif&0x03) - 3);
                            if (this->temperature_diff_sensor_)
                                this->temperature_diff_sensor_->publish_state(tdiff);
#endif
                        }
                        else if ( this->data_[i_VIF] == 0xfd && this->data_[i_VIF+1] == 0x17 ) { // Error flags (binary)
#ifdef USE_TEXT_SENSOR
                            if (this->status_text_sensor_){
                                if (result == 0x00)
                                    this->status_text_sensor_->publish_state("OK");
                                else {
                                    bool append = false;
                                    std::string state;
                                    state.reserve(256);
                                    if (result&0x01){
                                        if (append)
                                            state += " | ";
                                        state += "Temperature Sensor 1: Cable Break";
                                        append = true;
                                    }
                                    if (result&0x02){
                                        if (append)
                                            state += " | ";
                                        state += "Temperature Sensor 1: Short Circuit";
                                        append = true;
                                    }
                                    if (result&0x04){
                                        if (append)
                                            state += " | ";
                                        state += "Temperature Sensor 2: Cable Break";
                                        append = true;
                                    }
                                    if (result&0x08){
                                        if (append)
                                            state += " | ";
                                        state += "Temperature Sensor 2: Short Circuit";
                                        append = true;
                                    }
                                    if (result&0x10){
                                        if (append)
                                            state += " | ";
                                        state += "Error at Flow Measurement System";
                                        append = true;
                                    }
                                    if (result&0x20){
                                        if (append)
                                            state += " | ";
                                        state += "Electronic Defect";
                                        append = true;
                                    }
                                    if (result&0x40){
                                        if (append)
                                            state += " | ";
                                        state += "Reset";
                                        append = true;
                                    }
                                    if (result&0x80){
                                        if (append)
                                            state += " | ";
                                        state += "Low Battery";
                                        append = true;
                                    }
                                    this->status_text_sensor_->publish_state(state);
                                }
                            }
#endif
                        }
                        else {
                            // Unknown
                            ESP_LOGW(TAG, "Unsupported value type 0x%02x", vif);
                            break;
                        }
                    }
                    
#ifdef USE_SENSOR
                    if (this->calculated_power_sensor_) {
                        if (tdiff == -127 || flow == -127)
                            this->calculated_power_sensor_->publish_state(NAN);
                        else if (flow > 0)
                            this->calculated_power_sensor_->publish_state(flow / 3.6 * 4193 * tdiff);
                        else
                            this->calculated_power_sensor_->publish_state(0);
                    }
#endif

                }
            }
            
            this->data_.clear();
            this->receiving_ = 0;
        }
    }
    
    if (this->trigger_next_) {
        this->trigger_next_ = false;
        this->receiving_ = 1;
        this->last_transmission_ = now;

        this->flush();
        this->data_.clear();
        
        if (this->init_state_ == 0){
            uint8_t request_message[] = { 0x68, 0x08, 0x08, 0x68, 0x53, 0xFE, 0x51, 0x0F, 0x00, 0x00, 0x04, 0x5C, 0x11, 0x16 };
            ESP_LOGV(TAG, "M-Bus write: %s", format_hex_pretty(request_message, sizeof(request_message)).c_str());
            this->write_array(request_message, sizeof(request_message));
            this->init_state_ += 1;
        }
        else if (this->init_state_ == 1){
            uint8_t request_message[] = { 0x68, 0x09, 0x09, 0x68, 0x53, 0xFE, 0x51, 0x0F, 0x00, 0x00, 0x00, 0x59, 0x2D, 0x37, 0x16 };
            ESP_LOGV(TAG, "M-Bus write: %s", format_hex_pretty(request_message, sizeof(request_message)).c_str());
            this->write_array(request_message, sizeof(request_message));
            this->init_state_ += 1;
        }
        else if (this->init_state_ == 2){
            uint8_t request_message[] = { 0x68, 0x08, 0x08, 0x68, 0x73, 0xFE, 0x51, 0x0F, 0x00, 0x00, 0x00, 0x5D, 0x2E, 0x16 };
            ESP_LOGV(TAG, "M-Bus write: %s", format_hex_pretty(request_message, sizeof(request_message)).c_str());
            this->write_array(request_message, sizeof(request_message));
            this->init_state_ += 1;
        }
        else if (this->init_state_ == 3){
            uint8_t request_message[] = { 0x68, 0x73, 0x73, 0x68, 0x53, 0xFE, 0x51, 0x0F, 0x00, 0x00, 0x06, 0x5C, 0x09, 0x02, 0x00, 0x00, 0x8f, 0xad, 0xce, 0xe5, 0xc7, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x99, 0x16};
            // Checksum
            uint8_t checksum = 0;
            for (uint8_t i = 4; i < sizeof(request_message)-2; i++)
                checksum += request_message[i];
            request_message[sizeof(request_message)-2] = checksum;
            
            ESP_LOGV(TAG, "M-Bus write: %s", format_hex_pretty(request_message, sizeof(request_message)).c_str());
            this->write_array(request_message, sizeof(request_message));
            this->init_state_ += 1;
        }
        else if (this->init_state_ == 4){
            uint8_t request_message[] = { 0x68, 0x09, 0x09, 0x68, 0x53, 0xFE, 0x51, 0x0F, 0x00, 0x00, 0x00, 0x59, 0x0C, 0x16, 0x16};
            ESP_LOGV(TAG, "M-Bus write: %s", format_hex_pretty(request_message, sizeof(request_message)).c_str());
            this->write_array(request_message, sizeof(request_message));
            this->init_state_ += 1;
        }
        else {
            this->init_state_ = 0xff;
            uint8_t request_message[] = { 0x68, 0x11, 0x11, 0x68, 0x53, 0xFE, 0x51, 0x0F, 0x00, 0x00, 0x01, 0x59, 0x02, 0x03, 0x04, 0x06, 0x05, 0x07, 0x08, 0x09, 0x0B, 0x00, 0x16 };
        

            // FCB bit
            if (this->FCB_)
                request_message[4] |= 0x10;
            this->FCB_ = !this->FCB_;

            // Checksum
            uint8_t checksum = 0;
            for (uint8_t i = 4; i < sizeof(request_message)-2; i++)
                checksum += request_message[i];
            request_message[sizeof(request_message)-2] = checksum;

            ESP_LOGV(TAG, "M-Bus write: %s", format_hex_pretty(request_message, sizeof(request_message)).c_str());
            this->write_array(request_message, sizeof(request_message));
        }

    }
}

float SensoStarComponent::get_setup_priority() const { return setup_priority::DATA; }


}  // namespace sensostar
}  // namespace esphome

