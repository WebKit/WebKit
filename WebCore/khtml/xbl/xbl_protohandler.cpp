#ifndef KHTML_NO_XBL

#include "config.h"
#include "QStringList.h"
#include "QString.h"
#include "xbl_protohandler.h"

using DOM::DOMString;

namespace XBL {

const int XBLPrototypeHandler::shiftKey = (1<<1);
const int XBLPrototypeHandler::altKey = (1<<2);
const int XBLPrototypeHandler::ctrlKey = (1<<3);
const int XBLPrototypeHandler::metaKey = (1<<4);
const int XBLPrototypeHandler::allKeys = shiftKey | altKey | ctrlKey | metaKey;

const int XBLPrototypeHandler::bubblingPhase = 0;
const int XBLPrototypeHandler::capturingPhase = 1;
const int XBLPrototypeHandler::targetPhase = 2;

XBLPrototypeHandler::XBLPrototypeHandler(const DOMString& event,
                                         const DOMString& phase,
                                         const DOMString& action,
                                         const DOMString& keycode,
                                         const DOMString& charcode,
                                         const DOMString& modifiers,
                                         const DOMString& button,
                                         const DOMString& clickcount,
                                         const DOMString& preventdefault,
                                         XBLPrototypeBinding* binding)
:m_binding(binding), m_next(0), m_eventName(event), 
 m_phase(bubblingPhase), m_preventDefault(false), m_keyMask(0), m_misc(0), m_button(-1), m_unused(0)
{
    if (event.isEmpty())
        return;

    if (phase == "capturing")
        m_phase = capturingPhase;
    else if (phase == "target")
        m_phase = targetPhase;

    if (!action.isEmpty())
        m_handlerText = action;

    if (!modifiers.isEmpty()) {
        QStringList result = QStringList::split(",", modifiers.qstring());
        for (QStringList::Iterator it = result.begin(); it != result.end(); ++it) {
            QString modifier = (*it).stripWhiteSpace();
            if (modifier == "shift")
                m_keyMask |= shiftKey;
            else if (modifier == "alt")
                m_keyMask |= altKey;
            else if (modifier == "control")
                m_keyMask |= ctrlKey;
            else if (modifier == "meta" || modifier == "accel" || modifier == "access")
                m_keyMask |= metaKey; // FIXME: For Apple, accel is like meta, and access really has no equivalent.
                                      // If/when KDE adopts this code, use the appropriate defaults for the
                                      // platform (probably ctrl for accel and alt for access if they match Win32).
        }
    }

    if (!charcode.isEmpty()) {
        // Normalize the character based off case (and whether the shift key has to be down).
        m_key = charcode;
        if ((m_keyMask & shiftKey) != 0)
            m_key = m_key.upper();
        else
            m_key = m_key.lower();
         
        // We have a charcode.
        m_misc = 1;
    }
    else if (!keycode.isEmpty())
        m_key = keycode;
     
    if (!clickcount.isEmpty())
        m_misc = clickcount.toInt();
    if (!button.isEmpty())
        m_button = button.toInt();
}

XBLPrototypeHandler::~XBLPrototypeHandler()
{
    delete m_next;
}

void XBLPrototypeHandler::appendData(const QString& ch)
{
    m_handlerText += ch;
}

}

#endif
