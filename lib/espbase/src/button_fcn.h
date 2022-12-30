#ifndef button_fcn_h
#define button_fcn_h
 
#include <Arduino.h>
 
class button_fcn
{
    public:
        button_fcn(uint8_t button_pin);
        boolean pressed_btn_tone(boolean state, int pin_tone);
        boolean pressed_btn(boolean state);
        boolean pressed_times(boolean state, uint8_t times);
        //void released_btn(uint16_t pwm);
        //void hold_down_btn(uint16_t pwm);
        //void hold_up_btn(uint16_t pwm);


    private:
        uint8_t b_pin;
        ulong lastDebounceTime = 0;
        ulong debounceDelay = 50;
        boolean last_button_state = 1;
        boolean actual_button_state = 1;
        uint8_t presscnt = 0;


};
 
#endif