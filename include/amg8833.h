#pragma once
#include <Arduino.h>
#include <Wire.h> 
#include <array>
#define AMG8833_ADDRESS 0x69
#define I2C_SDA_PIN 14
#define I2C_SCL_PIN 15
#define I2C_SPEED 400000

#define POWER_CONTROL_REGISTER 0x00
#define RESET_REGISTER 0x01
#define FRAMERATE_REGISTER 0x02
#define INT_CONTROL_REGISTER 0x03
#define STATUS_REGISTER 0x04
#define STATUS_CLEAR_REGISTER 0x05
#define AVERAGE_REGISTER 0x07
#define INT_LEVEL_REGISTER_UPPER_LSB 0x08
#define INT_LEVEL_REGISTER_UPPER_MSB 0x09
#define INT_LEVEL_REGISTER_LOWER_LSB 0x0A
#define INT_LEVEL_REGISTER_LOWER_MSB 0x0B
#define INT_LEVEL_REGISTER_HYST_LSB 0x0C
#define INT_LEVEL_REGISTER_HYST_MSB 0x0D
#define THERMISTOR_REGISTER_LSB 0x0E
#define THERMISTOR_REGISTER_MSB 0x0F
#define INT_TABLE_REGISTER_INT0 0x10
#define RESERVED_AVERAGE_REGISTER 0x1F
#define TEMPERATURE_REGISTER_START 0x80


enum class I2C_STATUS: int8_t{
    OK = 0,
    ERROR = -1
};

enum class FRAMERATE: bool{
    FPS_10 = 0,
    FPS_1 = 1
};

enum class POWER_MODE: uint8_t{
    WAKE = 0x00,
    SLEEP = 0x10,
    STAND_BY_60_SEC = 0x20,
    STAND_BY_10_SEC = 0x21
};

class AMG8833{
public:
    AMG8833(){};
    void init(int SDA = I2C_SDA_PIN, int SCL = I2C_SCL_PIN){
        Serial.println("Initializing I2C");
        i2c = &Wire;
        bool status = i2c->begin(SDA, SCL, I2C_SPEED);
        if (!status) {
            Serial.println("I2C initialization failed!");
        } else {
            Serial.println("I2C initialization successful!");
        }
    }
    void reset(){
        Serial.print("Resetting sensor ... ");
        bool result=write_register(RESET_REGISTER, 0x3F);
        Serial.println(result);
    }

    void set_framerate(FRAMERATE framerate){
        write_register(FRAMERATE_REGISTER, static_cast<bool>(framerate));
    }
    bool get_framerate(){
        return read_register(FRAMERATE_REGISTER);
    }
    void set_mode(POWER_MODE mode){
        write_register(POWER_CONTROL_REGISTER,static_cast<uint8_t>(mode));
    }
    void set_settings(const char* key,int value){
        if(strcmp(key,"framerate") == 0){
            switch (value)
            {
            case static_cast<int>(FRAMERATE::FPS_10):
            case static_cast<int>(FRAMERATE::FPS_1):
                set_framerate(static_cast<FRAMERATE>(value));
                break;
            default:
                Serial.println("Invalid framerate");
                break;
            }
            
        }
        if(strcmp(key,"mode") == 0){
            switch (value)
            {
            case static_cast<int>(POWER_MODE::WAKE):
            case static_cast<int>(POWER_MODE::SLEEP):
            case static_cast<int>(POWER_MODE::STAND_BY_60_SEC):
            case static_cast<int>(POWER_MODE::STAND_BY_10_SEC):
                set_mode(static_cast<POWER_MODE>(value));
                break;
            default:
                Serial.println("Invalid mode");
                break;
            }
        }
    };
    std::array<std::array<float, 8>, 8> get_matrix(){

        std::array<std::array<float, 8>, 8> temp_matrix;
        for(int i = 0; i < 8; i++){

            for(int j = 0; j < 8; j++){

                //Serial.printf("Register: %x\n", TEMPERATURE_REGISTER_START + (i*2 * 8) + j*2);
                temp_matrix[i][j] = convert12bit(read_register16(TEMPERATURE_REGISTER_START + (i*2 * 8) + j*2))* 0.25;
            }
        }
        return temp_matrix;
    }
    float get_thermistor_temperature(){
        return convert12bit(read_register16(THERMISTOR_REGISTER_LSB));
    }
private:
    int8_t read_register(uint8_t address){
        i2c->beginTransmission(AMG8833_ADDRESS);
        i2c->write(address);
        if(i2c->endTransmission() != 0){
            return static_cast<int16_t>(I2C_STATUS::ERROR);
        }
        bool result = i2c->requestFrom(AMG8833_ADDRESS, 1);
        if(result){
            return i2c->read();
        }
        return static_cast<int16_t>(I2C_STATUS::ERROR);
    }
    bool write_register(uint8_t address, uint8_t value){
        i2c->beginTransmission(AMG8833_ADDRESS);
        i2c->write(address);
        i2c->write(value);
        if(i2c->endTransmission() != 0){
            return static_cast<int16_t>(I2C_STATUS::ERROR);
        }
        return static_cast<int16_t>(I2C_STATUS::OK);
    }
    uint16_t read_register16(uint8_t address){
        i2c->beginTransmission(AMG8833_ADDRESS);
        i2c->write(address);
        if(i2c->endTransmission() != 0){
            Serial.println("Error");
            return static_cast<int16_t>(I2C_STATUS::ERROR);
        }

        bool result = i2c->requestFrom(AMG8833_ADDRESS, 2);
        if(result){

            uint8_t lsb = i2c->read();
            uint8_t msb = i2c->read();
            return (msb << 8) | lsb;
        }
        Serial.println("Error");
        return static_cast<int16_t>(I2C_STATUS::ERROR);
        
    }

    float convert12bit(uint16_t value){

        if(value & (1 << 11)){
            value |= 0xF000;
        }else{
            value &= 0x07FF;
        }
 
        return static_cast<float>(static_cast<int16_t>(value));
        
    }
private:
    TwoWire *i2c;
    std::array<std::array<float, 8>, 8> matrix;
};