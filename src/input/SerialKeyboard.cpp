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

#if defined(CUSTOM_CHATTER_KEYBOARD)

// How often the shift registers are sampled. A key press has to be read twice in
// a row with the same value before it counts, so the effective response time is
// two polls.
#define CUSTOM_CHATTER_POLL_MS 25

// Bit positions inside `keys`, which is (shiftRegister1 << 8) | shiftRegister2.
// Both registers are read LSBFIRST, so physical input D0 lands in bit 7 of its
// register and D7 in bit 0 - see the wiring table in the variant README.
#define CC_SR1(bit) ((uint16_t)(1u << (8 + (bit))))
#define CC_SR2(bit) ((uint16_t)(1u << (bit)))

#define CC_KEY_SHIFT CC_SR1(0)  // SW12
#define CC_KEY_SPACE CC_SR1(1)  // SW11, button 9 ("0" / space)
#define CC_KEY_6 CC_SR1(2)      // SW10, button 5
#define CC_KEY_7 CC_SR1(3)      // SW9,  button 6
#define CC_KEY_CANCEL CC_SR1(4) // SW13
#define CC_KEY_LEFT CC_SR1(5)   // SW14
#define CC_KEY_RIGHT CC_SR1(6)  // SW15
#define CC_KEY_SELECT CC_SR1(7) // SW16
#define CC_KEY_8 CC_SR2(0)      // SW8,  button 7
#define CC_KEY_9 CC_SR2(1)      // SW7,  button 8
#define CC_KEY_BACK CC_SR2(2)   // SW6,  backspace / tab
#define CC_KEY_5 CC_SR2(3)      // SW5,  button 4
#define CC_KEY_4 CC_SR2(4)      // SW4,  button 3
#define CC_KEY_3 CC_SR2(5)      // SW3,  button 2
#define CC_KEY_2 CC_SR2(6)      // SW2,  button 1
#define CC_KEY_1 CC_SR2(7)      // SW1,  button 0

// Cyrillic T9 layout: [shift level 0 = lower, 1 = upper][tap 0..3][button 0..9],
// stored as Unicode code points. A 0 entry means "no cyrillic glyph here" and the
// latin KeyMap above is used instead, which keeps punctuation and space in place.
// Buttons 1..8 are the keys labelled 2..9, button 0 is "1" and button 9 is "0".
static const uint16_t CyrillicKeyMap[2][4][10] = {
    {// lower case
     {0x0000, 0x0430, 0x0434, 0x0438, 0x043C, 0x0440, 0x0444, 0x0448, 0x044C, 0x0000},  // а д и м р ф ш ь
     {0x0000, 0x0431, 0x0435, 0x0439, 0x043D, 0x0441, 0x0445, 0x0449, 0x044D, 0x0000},  // б е й н с х щ э
     {0x0000, 0x0432, 0x0436, 0x043A, 0x043E, 0x0442, 0x0446, 0x044A, 0x044E, 0x0000},  // в ж к о т ц ъ ю
     {0x0451, 0x0433, 0x0437, 0x043B, 0x043F, 0x0443, 0x0447, 0x044B, 0x044F, 0x0000}}, // ё г з л п у ч ы я
    {// upper case
     {0x0000, 0x0410, 0x0414, 0x0418, 0x041C, 0x0420, 0x0424, 0x0428, 0x042C, 0x0000},  // А Д И М Р Ф Ш Ь
     {0x0000, 0x0411, 0x0415, 0x0419, 0x041D, 0x0421, 0x0425, 0x0429, 0x042D, 0x0000},  // Б Е Й Н С Х Щ Э
     {0x0000, 0x0412, 0x0416, 0x041A, 0x041E, 0x0422, 0x0426, 0x042A, 0x042E, 0x0000},  // В Ж К О Т Ц Ъ Ю
     {0x0401, 0x0413, 0x0417, 0x041B, 0x041F, 0x0423, 0x0427, 0x042B, 0x042F, 0x0000}}}; // Ё Г З Л П У Ч Ы Я

#endif // CUSTOM_CHATTER_KEYBOARD

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

#if defined(CUSTOM_CHATTER_KEYBOARD)

void SerialKeyboard::emitCodepoint(InputEvent &e, uint32_t codepoint)
{
    e.inputEvent = INPUT_BROKER_ANYKEY;
    if (codepoint < 0x80) {
        e.kbchar = (unsigned char)codepoint;
        e.codepoint = 0;
    } else {
        // Never put a raw UTF-8 byte into kbchar: values such as 0x90 and 0xAA are
        // reserved command codes (reboot, bluetooth toggle, ...). Non-ASCII travels
        // in its own field instead.
        e.kbchar = 0;
        e.codepoint = codepoint;
    }
}

int32_t SerialKeyboard::scanCustomChatter(uint8_t shiftRegister1, uint8_t shiftRegister2)
{
    const uint16_t sample = (uint16_t)(((uint16_t)shiftRegister1 << 8) | shiftRegister2);

    // Require two identical reads before trusting a sample. This throws away
    // contact bounce and half-shifted reads, which the old `keys < prevKeys`
    // check turned into duplicated key events.
    if (sample != debounceSample) {
        debounceSample = sample;
        return CUSTOM_CHATTER_POLL_MS;
    }

    // All sixteen switches cannot physically be down at once: this is a floating
    // DATA line (registers not powered yet, or a wiring fault). Drop the reading
    // and wait for the keyboard to look idle again before emitting anything.
    if (sample == 0x0000) {
        keyboardSettled = false;
        return CUSTOM_CHATTER_POLL_MS;
    }

    // After boot (and after any bad reading) wait until the chain reports a clean
    // "nothing pressed" before we act on it. Otherwise garbage read while the
    // 74HC165s are still powering up walks through the region / frequency pickers
    // on its own and leaves the device on a screen the user never chose.
    if (!keyboardSettled) {
        if (sample == 0xFFFF) {
            keyboardSettled = true;
            stableKeys = sample;
            debounceSample = sample;
        }
        return CUSTOM_CHATTER_POLL_MS;
    }

    if (sample == stableKeys)
        return CUSTOM_CHATTER_POLL_MS;

    const uint16_t prevPressed = (uint16_t)~stableKeys;
    const uint16_t pressed = (uint16_t)~sample;
    stableKeys = sample;

    // Keep the shared members in sync so the rest of the class (and the debug log)
    // still sees the current state.
    prevKeys = keys;
    keys = sample;

#if defined(CUSTOM_CHATTER_KEYBOARD_DEBUG)
    LOG_DEBUG("Custom keyboard SR1=0x%02x SR2=0x%02x", shiftRegister1, shiftRegister2);
#endif

    // Only keys that went down since the last stable sample generate an event.
    // The old code compared the whole word against the previous one and then
    // walked a fixed-priority if/else chain, so a single stuck or badly soldered
    // switch made *every* later press report that stuck key instead - the
    // keyboard looked dead from the user's side. An edge mask cannot do that:
    // a permanently low bit is simply never "newly pressed" again.
    const uint16_t newlyPressed = (uint16_t)(pressed & ~prevPressed);
    if (newlyPressed == 0)
        return CUSTOM_CHATTER_POLL_MS;

    if (!Throttle::isWithinTimespanMs(lastPressTime, 500)) {
        quickPress = 0;
    }

    // Tapping SHIFT cycles the case level. *Holding* SHIFT turns the next key into
    // a modifier chord instead - scroll, tab, language - and the level is put back
    // afterwards so the case never changes under the user.
    const bool shiftHeld = (pressed & CC_KEY_SHIFT) != 0;

    // SHIFT + "0" cycles the input language. Checked before anything else so the
    // chord never types a space.
    if (shiftHeld && (newlyPressed & CC_KEY_SPACE)) {
        language = (uint8_t)((language + 1) % KB_LANG_COUNT);
        shift = shiftBeforeModifier; // undo the level bump the SHIFT press itself made
        quickPress = 0;
        lastKeyPressed = 13;
        keyPressed = 13;
        LOG_INFO("Custom keyboard language: %s", language == KB_LANG_CYRILLIC ? "cyrillic" : "latin");
        return CUSTOM_CHATTER_POLL_MS;
    }

    InputEvent e = {};
    e.inputEvent = INPUT_BROKER_NONE;
    e.source = this->_originName;

    // NAVIGATION / COMMAND KEYS
    // The board has no dedicated up/down keys, and node lists and the message
    // history scroll on UP/DOWN only, so SHIFT + arrow stands in for them.
    if (newlyPressed & CC_KEY_LEFT) {
        if (shiftHeld) {
            e.inputEvent = INPUT_BROKER_UP;
            shift = shiftBeforeModifier;
        } else {
            e.inputEvent = INPUT_BROKER_LEFT;
        }
    } else if (newlyPressed & CC_KEY_RIGHT) {
        if (shiftHeld) {
            e.inputEvent = INPUT_BROKER_DOWN;
            shift = shiftBeforeModifier;
        } else {
            e.inputEvent = INPUT_BROKER_RIGHT;
        }
    } else if (newlyPressed & CC_KEY_SELECT) {
        e.inputEvent = INPUT_BROKER_SELECT;
    } else if (newlyPressed & CC_KEY_CANCEL) {
        e.inputEvent = INPUT_BROKER_CANCEL;
    }

    // TEXT INPUT
    else if (newlyPressed & CC_KEY_1) {
        keyPressed = 0;
    } else if (newlyPressed & CC_KEY_2) {
        keyPressed = 1;
    } else if (newlyPressed & CC_KEY_3) {
        keyPressed = 2;
    } else if (newlyPressed & CC_KEY_4) {
        keyPressed = 3;
    } else if (newlyPressed & CC_KEY_5) {
        keyPressed = 4;
    } else if (newlyPressed & CC_KEY_6) {
        keyPressed = 5;
    } else if (newlyPressed & CC_KEY_7) {
        keyPressed = 6;
    } else if (newlyPressed & CC_KEY_8) {
        keyPressed = 7;
    } else if (newlyPressed & CC_KEY_9) {
        keyPressed = 8;
    } else if (newlyPressed & CC_KEY_SPACE) {
        keyPressed = 9;
    }
    // BACKSPACE, or TAB (switch destination) when SHIFT is held. Plain backspace
    // now works at every case level, which it did not when TAB sat on the upper
    // case level alone.
    else if (newlyPressed & CC_KEY_BACK) {
        if (shiftHeld) {
            e.inputEvent = INPUT_BROKER_ANYKEY;
            e.kbchar = 0x09; // TAB
            shift = shiftBeforeModifier;
        } else {
            e.inputEvent = INPUT_BROKER_BACK;
            e.kbchar = 0x08;
        }
    }
    // SHIFT
    else if (newlyPressed & CC_KEY_SHIFT) {
        keyPressed = 10;
    }

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
        if (keyPressed < 10) { // it's a letter
            if (keyPressed == lastKeyPressed && millis() - lastPressTime < 500) {
                erase(); // replace the character that the previous tap produced
            }

            uint32_t codepoint = 0;
            if (language == KB_LANG_CYRILLIC && shift < 2) {
                codepoint = CyrillicKeyMap[shift][quickPress][keyPressed];
            }
            if (codepoint == 0) {
                codepoint = KeyMap[shift][quickPress][keyPressed];
            }
            emitCodepoint(e, codepoint);
        } else { // then it's shift
            shiftBeforeModifier = shift;
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

    return CUSTOM_CHATTER_POLL_MS;
}

#endif // CUSTOM_CHATTER_KEYBOARD

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

#if defined(CUSTOM_CHATTER_KEYBOARD)
        return scanCustomChatter(shiftRegister1, shiftRegister2);
#else

        keys = (shiftRegister1 << 8) + shiftRegister2;

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
#endif // CUSTOM_CHATTER_KEYBOARD
    }
    return 50;
}

#endif // INPUTBROKER_SERIAL_TYPE
