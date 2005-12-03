#include "dom/dom_string.h"

namespace DOM
{
    class ElementImpl;
    class DOMString;
}

namespace XBL
{
    class XBLPrototypeBinding;

class XBLPrototypeHandler
{
public:
    XBLPrototypeHandler(const DOM::DOMString& event,
                        const DOM::DOMString& phase,
                        const DOM::DOMString& action,
                        const DOM::DOMString& keycode,
                        const DOM::DOMString& charcode,
                        const DOM::DOMString& modifiers,
                        const DOM::DOMString& button,
                        const DOM::DOMString& clickcount,
                        const DOM::DOMString& preventdefault,
                        XBLPrototypeBinding* binding);
    ~XBLPrototypeHandler();
    
    void setNext(XBLPrototypeHandler* handler) { m_next = handler; }
    XBLPrototypeHandler* next() const { return m_next; }
    
    void appendData(const QString& ch);
    
    static const int shiftKey;
    static const int altKey;
    static const int ctrlKey;
    static const int metaKey;
    static const int allKeys;
    
    static const int bubblingPhase;
    static const int capturingPhase;
    static const int targetPhase;
    
private:
    DOM::DOMString m_handlerText; // The text for the event handler.
    XBLPrototypeBinding* m_binding; // The binding that owns us.
    XBLPrototypeHandler* m_next; // Our next event handler.

    DOM::DOMString m_eventName; // The name of the event.

    // The following values make up 32 bits.
    int m_phase : 2;            // The phase (capturing, bubbling, target)
    bool m_preventDefault : 1;  // Whether or not to preventDefault after executing the event handler.
    int m_keyMask : 4;          // Which modifier keys this event handler expects to have down
                                // in order to be matched.
    int m_misc : 8;             // Miscellaneous extra information.  For key events,
                                // stores whether or not we're a key code or char code.
                                // For mouse events, stores the clickCount.
    // The primary filter information for mouse/key events.
    short m_button;             // For mouse events, stores the button info.
    int m_unused: 1;
    DOM::DOMString m_key;            // The keycode/charcode we want to match for key events
};

}
