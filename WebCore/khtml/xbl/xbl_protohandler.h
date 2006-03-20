#include "PlatformString.h"

namespace WebCore
{
    class Element;
    class String;
}

namespace XBL
{
    class XBLPrototypeBinding;

class XBLPrototypeHandler
{
public:
    XBLPrototypeHandler(const WebCore::String& event,
                        const WebCore::String& phase,
                        const WebCore::String& action,
                        const WebCore::String& keycode,
                        const WebCore::String& charcode,
                        const WebCore::String& modifiers,
                        const WebCore::String& button,
                        const WebCore::String& clickcount,
                        const WebCore::String& preventdefault,
                        XBLPrototypeBinding* binding);
    ~XBLPrototypeHandler();
    
    void setNext(XBLPrototypeHandler* handler) { m_next = handler; }
    XBLPrototypeHandler* next() const { return m_next; }
    
    void appendData(const DeprecatedString& ch);
    
    static const int shiftKey;
    static const int altKey;
    static const int ctrlKey;
    static const int metaKey;
    static const int allKeys;
    
    static const int bubblingPhase;
    static const int capturingPhase;
    static const int targetPhase;
    
private:
    WebCore::String m_handlerText; // The text for the event handler.
    XBLPrototypeBinding* m_binding; // The binding that owns us.
    XBLPrototypeHandler* m_next; // Our next event handler.

    WebCore::String m_eventName; // The name of the event.

    // The following values make up 32 bits.
    unsigned m_phase : 2;       // The phase (capturing, bubbling, target)
    bool m_preventDefault : 1;  // Whether or not to preventDefault after executing the event handler.
    unsigned m_keyMask : 4;     // Which modifier keys this event handler expects to have down
                                // in order to be matched.
    unsigned m_misc : 8;        // Miscellaneous extra information.  For key events,
                                // stores whether or not we're a key code or char code.
                                // For mouse events, stores the clickCount.
    // 1 bit unused
    // The primary filter information for mouse/key events.
    short m_button;             // For mouse events, stores the button info.
    WebCore::String m_key;            // The keycode/charcode we want to match for key events
};

}
