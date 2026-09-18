#pragma once

#include "InputBroker.h"
#include "concurrency/OSThread.h"

class SerialKeyboard : public Observable<const InputEvent *>, public concurrency::OSThread
{
  public:
    explicit SerialKeyboard(const char *name);

    // 0 = lower case, 1 = upper case, 2 = numbers
    uint8_t getShift() const { return shift; }

#if defined(CUSTOM_CHATTER_KEYBOARD)
    enum KeyboardLanguage : uint8_t { KB_LANG_LATIN = 0, KB_LANG_CYRILLIC = 1, KB_LANG_COUNT };

    uint8_t getLanguage() const { return language; }
#endif

  protected:
    virtual int32_t runOnce() override;
    void erase();

  private:
    const char *_originName;
    bool firstTime = 1;
    int prevKeys = 0;
    int keys = 0;
    int shift = 0;
    int keyPressed = 13;
    int lastKeyPressed = 13;
    int quickPress = 0;
    unsigned long lastPressTime = 0;

#if defined(CUSTOM_CHATTER_KEYBOARD)
    int32_t scanCustomChatter(uint8_t shiftRegister1, uint8_t shiftRegister2);
    void emitCodepoint(InputEvent &e, uint32_t codepoint);

    uint16_t debounceSample = 0xFFFF; // last raw sample, used to require two identical reads in a row
    uint16_t stableKeys = 0xFFFF;     // last debounced sample (1 = released, matches the 74HC165 idle level)
    bool keyboardSettled = false;     // set once a stable "nothing pressed" sample has been seen after boot
    uint8_t language = 0;             // KeyboardLanguage
    int shiftBeforeModifier = 0;      // shift value before the SHIFT key bumped it, so chords can undo the bump
#endif
};

extern SerialKeyboard *globalSerialKeyboard;
