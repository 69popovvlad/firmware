#include "SerialKeyboardImpl.h"
#include "InputBroker.h"
#include "configuration.h"

#ifdef INPUTBROKER_SERIAL_TYPE

SerialKeyboardImpl *aSerialKeyboardImpl;

SerialKeyboardImpl::SerialKeyboardImpl() : SerialKeyboard("serialKB") {}

void SerialKeyboardImpl::init()
{
    if (!INPUTBROKER_SERIAL_TYPE) {
        disable();
        return;
    }

    // The menu entries that compose free text straight to a chosen node ("New
    // Freetext Msg", "With Freetext") are gated on kb_found, which only the I2C
    // keyboard scan and the touch keyboard set. This keyboard hangs off shift
    // registers and cannot be scanned for, so it has to say so itself - without
    // this the only way to address a node is the destination picker.
    kb_found = true;

    inputBroker->registerSource(this);
}

#endif // INPUTBROKER_SERIAL_TYPE