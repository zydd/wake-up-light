/* A implementar.....

uint button_pressed(int pin_button, boolean state){
    if (digitalRead(pin_button) != last_button_state) {
        lastDebounceTime = millis();
    }

    if ((millis() - lastDebounceTime) > debounceDelay) {
        if (digitalRead(pin_button) == state){
            presscnt++;
            lastDebounceTime = millis();
        }
        else
            presscnt = 0;
    }
    last_button_state = digitalRead(pin_button);
    return presscnt;
}*/


 
#include "button_fcn.h"
 
button_fcn::button_fcn(uint8_t button_pin)
{
    pinMode(button_pin, INPUT);

    b_pin = button_pin;

}
 
boolean button_fcn::pressed_btn(boolean state)
{
    boolean nstate;
    if (digitalRead(b_pin) != last_button_state) {
        lastDebounceTime = millis();
    }

    if ((millis() - lastDebounceTime) > debounceDelay) {
        if (digitalRead(b_pin) != actual_button_state && digitalRead(b_pin) == state)
            nstate = true;
        else
            nstate = false;
        actual_button_state = digitalRead(b_pin);
    }
    else
        nstate = false;
    last_button_state = digitalRead(b_pin);
    return nstate;
}

boolean button_fcn::pressed_btn_tone(boolean state,int pin_tone)
{
    boolean nstate;
    if (digitalRead(b_pin) != last_button_state) 
        lastDebounceTime = millis();

    if ((millis() - lastDebounceTime) > debounceDelay) {
        if (digitalRead(b_pin) != actual_button_state && digitalRead(b_pin) == state)
        {
            nstate = true;
            tone(pin_tone, 1200,50);
        }    
        else
            nstate = false;
        actual_button_state = digitalRead(b_pin);
    }
    else
        nstate = false;
    last_button_state = digitalRead(b_pin);
    return nstate;
}

boolean button_fcn::pressed_times(boolean state,uint8_t times)
{
    boolean nstate;
    if (digitalRead(b_pin) != last_button_state) {
        lastDebounceTime = millis();
    }

    if ((millis() - lastDebounceTime) > debounceDelay) {
        if (digitalRead(b_pin) != actual_button_state && digitalRead(b_pin) == state)
            presscnt++;
        actual_button_state = digitalRead(b_pin);
    }
    if (presscnt >= times){
        nstate = 1;
        presscnt = 0;
    }
    else
        nstate = 0;

    last_button_state = digitalRead(b_pin);
    return nstate;
}
 