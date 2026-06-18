#include "SerialKeyboard.h"
#include "configuration.h"
#include <Throttle.h>

SerialKeyboard *globalSerialKeyboard = nullptr;

#ifdef INPUTBROKER_SERIAL_TYPE

#if INPUTBROKER_SERIAL_TYPE == 1 // It's a Chatter
// 3 SHIFT level (lower case, upper case, numbers), up to 4 repeated presses, button number
unsigned char KeyMap[3][4][10] = {{{'.', 'a', 'd', 'g', 'j', 'm', 'p', 't', 'w', ' '},
                                   {',', 'b', 'e', 'h', 'k', 'n', 'q', 'u', 'x', ' '},
                                   {'?', 'c', 'f', 'i', 'l', 'o', 'r', 'v', 'y', ' '},
                                   {'1', '2', '3', '4', '5', '6', 's', '8', 'z', ' '}}, // low case
                                  {{'!', 'A', 'D', 'G', 'J', 'M', 'P', 'T', 'W', ' '},
                                   {'+', 'B', 'E', 'H', 'K', 'N', 'Q', 'U', 'X', ' '},
                                   {'-', 'C', 'F', 'I', 'L', 'O', 'R', 'V', 'Y', ' '},
                                   {'1', '2', '3', '4', '5', '6', 'S', '8', 'Z', ' '}}, // upper case
                                  {{'1', '2', '3', '4', '5', '6', '7', '8', '9', '0'},
                                   {'1', '2', '3', '4', '5', '6', '7', '8', '9', '0'},
                                   {'1', '2', '3', '4', '5', '6', '7', '8', '9', '0'},
                                   {'1', '2', '3', '4', '5', '6', '7', '8', '9', '0'}}}; // numbers

#endif

SerialKeyboard::SerialKeyboard(const char *name) : concurrency::OSThread(name)
{
    this->_originName = name;

    globalSerialKeyboard = this;
}

void SerialKeyboard::erase()
{
    InputEvent e = {};
    e.inputEvent = INPUT_BROKER_BACK;
    e.kbchar = 0x08;
    e.source = this->_originName;
    this->notifyObservers(&e);
}

int32_t SerialKeyboard::runOnce()
{
    if (!INPUTBROKER_SERIAL_TYPE) {
        // Input device is not requested.
        return disable();
    }

    if (firstTime) {
        // This is the first time the OSThread library has called this function, so do port setup
        firstTime = 0;
        pinMode(KB_LOAD, OUTPUT);
        pinMode(KB_CLK, OUTPUT);
        pinMode(KB_DATA, INPUT);
        digitalWrite(KB_LOAD, HIGH);
        digitalWrite(KB_CLK, LOW);
        prevKeys = 0b1111111111111111;
        LOG_DEBUG("Serial Keyboard setup");
    }

    if (INPUTBROKER_SERIAL_TYPE == 1) { // Chatter V1.0 & V2.0 keypads
        // scan for keypresses
        // Write pulse to load pin
        digitalWrite(KB_LOAD, LOW);
        delayMicroseconds(5);
        digitalWrite(KB_LOAD, HIGH);
        delayMicroseconds(5);

        // Get data from 74HC165
        byte shiftRegister1 = shiftIn(KB_DATA, KB_CLK, LSBFIRST);
        byte shiftRegister2 = shiftIn(KB_DATA, KB_CLK, LSBFIRST);

#if defined(CUSTOM_CHATTER_KEYBOARD) && defined(CUSTOM_CHATTER_SWAP_SHIFT_REGISTERS)
        byte shiftRegisterSwap = shiftRegister1;
        shiftRegister1 = shiftRegister2;
        shiftRegister2 = shiftRegisterSwap;
#endif

        keys = (shiftRegister1 << 8) + shiftRegister2;

#if defined(CUSTOM_CHATTER_KEYBOARD) && defined(CUSTOM_CHATTER_KEYBOARD_DEBUG)
        if (keys != prevKeys) {
            LOG_DEBUG("Custom keyboard SR1=0x%02x SR2=0x%02x", shiftRegister1, shiftRegister2);
        }
#endif

        // Print to serial monitor
        // Serial.print (shiftRegister1, BIN);
        // Serial.print ("X");
        // Serial.println (shiftRegister2, BIN);

        if (!Throttle::isWithinTimespanMs(lastPressTime, 500)) {
            quickPress = 0;
        }

        if (keys < prevKeys) { // a new key has been pressed (and not released), doesn't works for multiple presses at once but
                               // shouldn't be a limitation
            InputEvent e = {};
            e.inputEvent = INPUT_BROKER_NONE;
            e.source = this->_originName;
            // SELECT OR SEND OR CANCEL EVENT
#if defined(CUSTOM_CHATTER_KEYBOARD)
            if (!(shiftRegister1 & (1 << 5))) { // SR1 D2, SW14: LEFT
                if (shift > 0) {
                    e.inputEvent = INPUT_BROKER_ANYKEY;
                    e.kbchar = 0x09;
                    shift = 0;
                } else {
                    e.inputEvent = INPUT_BROKER_LEFT;
                }
            } else if (!(shiftRegister1 & (1 << 6))) { // SR1 D1, SW15: RIGHT
                if (shift > 0) {
                    e.inputEvent = INPUT_BROKER_ANYKEY;
                    e.kbchar = 0x09;
                    shift = 0;
                } else {
                    e.inputEvent = INPUT_BROKER_RIGHT;
                }
            } else if (!(shiftRegister1 & (1 << 7))) { // SR1 D0, SW16: SELECT
                e.inputEvent = INPUT_BROKER_SELECT;
            } else if (!(shiftRegister1 & (1 << 4))) { // SR1 D3, SW13: CANCEL
                e.inputEvent = INPUT_BROKER_CANCEL;
            }

            // TEXT INPUT EVENT
            else if (!(shiftRegister2 & (1 << 7))) { // SR2 D0, SW1: key 1
                keyPressed = 0;
            } else if (!(shiftRegister2 & (1 << 6))) { // SR2 D1, SW2: key 2
                keyPressed = 1;
            } else if (!(shiftRegister2 & (1 << 5))) { // SR2 D2, SW3: key 3
                keyPressed = 2;
            } else if (!(shiftRegister2 & (1 << 4))) { // SR2 D3, SW4: key 4
                keyPressed = 3;
            } else if (!(shiftRegister2 & (1 << 3))) { // SR2 D4, SW5: key 5
                keyPressed = 4;
            } else if (!(shiftRegister1 & (1 << 2))) { // SR1 D5, SW10: key 6
                keyPressed = 5;
            } else if (!(shiftRegister1 & (1 << 3))) { // SR1 D4, SW9: key 7
                keyPressed = 6;
            } else if (!(shiftRegister2 & (1 << 0))) { // SR2 D7, SW8: key 8
                keyPressed = 7;
            } else if (!(shiftRegister2 & (1 << 1))) { // SR2 D6, SW7: key 9
                keyPressed = 8;
            } else if (!(shiftRegister1 & (1 << 1))) { // SR1 D6, SW11: key 0 / space
                keyPressed = 9;
            }
            // BACKSPACE or TAB
            else if (!(shiftRegister2 & (1 << 2))) { // SR2 D5, SW6
                if (shift == 0 || shift == 2) {
                    e.inputEvent = INPUT_BROKER_BACK;
                    e.kbchar = 0x08;
                } else {
                    e.inputEvent = INPUT_BROKER_ANYKEY;
                    e.kbchar = 0x09;
                }
            }
            // SHIFT
            else if (!(shiftRegister1 & (1 << 0))) { // SR1 D7, SW12
                keyPressed = 10;
            }
#else
            if (!(shiftRegister2 & (1 << 3))) {
                if (shift > 0) {
                    e.inputEvent = INPUT_BROKER_ANYKEY; // REQUIRED
                    e.kbchar = 0x09;                    // TAB
                    shift = 0;                          // reset shift after TAB
                } else {
                    e.inputEvent = INPUT_BROKER_LEFT;
                }
            } else if (!(shiftRegister2 & (1 << 2))) {
                if (shift > 0) {
                    e.inputEvent = INPUT_BROKER_ANYKEY; // REQUIRED
                    e.kbchar = 0x09;                    // TAB
                    shift = 0;                          // reset shift after TAB
                } else {
                    e.inputEvent = INPUT_BROKER_RIGHT;
                }
                e.kbchar = 0;
            } else if (!(shiftRegister2 & (1 << 1))) {
                e.inputEvent = INPUT_BROKER_SELECT;
            } else if (!(shiftRegister2 & (1 << 0))) {
                e.inputEvent = INPUT_BROKER_CANCEL;
            }

            // TEXT INPUT EVENT
            else if (!(shiftRegister1 & (1 << 4))) {
                keyPressed = 0;
            } else if (!(shiftRegister1 & (1 << 3))) {
                keyPressed = 1;
            } else if (!(shiftRegister2 & (1 << 4))) {
                keyPressed = 2;
            } else if (!(shiftRegister1 & (1 << 5))) {
                keyPressed = 3;
            } else if (!(shiftRegister1 & (1 << 2))) {
                keyPressed = 4;
            } else if (!(shiftRegister2 & (1 << 5))) {
                keyPressed = 5;
            } else if (!(shiftRegister1 & (1 << 6))) {
                keyPressed = 6;
            } else if (!(shiftRegister1 & (1 << 1))) {
                keyPressed = 7;
            } else if (!(shiftRegister2 & (1 << 6))) {
                keyPressed = 8;
            } else if (!(shiftRegister1 & (1 << 0))) {
                keyPressed = 9;
            }
            // BACKSPACE or TAB
            else if (!(shiftRegister1 & (1 << 7))) {
                if (shift == 0 || shift == 2) { // BACKSPACE
                    e.inputEvent = INPUT_BROKER_BACK;
                    e.kbchar = 0x08;
                } else { // shift = 1 => TAB
                    e.inputEvent = INPUT_BROKER_ANYKEY;
                    e.kbchar = 0x09;
                }
            }
            // SHIFT
            else if (!(shiftRegister2 & (1 << 7))) {
                keyPressed = 10;
            }
#endif

            if (keyPressed < 11) {
                if (keyPressed == lastKeyPressed && millis() - lastPressTime < 500) {
                    quickPress += 1;
                    if (quickPress > 3) {
                        quickPress = 0;
                    }
                }
                if (keyPressed != lastKeyPressed) {
                    quickPress = 0;
                }
                if (keyPressed < 10) { // if it's a letter
                    if (keyPressed == lastKeyPressed && millis() - lastPressTime < 500) {
                        erase();
                    }
                    e.inputEvent = INPUT_BROKER_ANYKEY;
                    e.kbchar = char(KeyMap[shift][quickPress][keyPressed]);
                } else { // then it's shift
                    shift += 1;
                    if (shift > 2) {
                        shift = 0;
                    }
                }
                lastPressTime = millis();
                lastKeyPressed = keyPressed;
                keyPressed = 13;
            }

            if (e.inputEvent != INPUT_BROKER_NONE) {
                this->notifyObservers(&e);
            }
        }
        prevKeys = keys;
    }
    return 50;
}

#endif // INPUTBROKER_SERIAL_TYPE
