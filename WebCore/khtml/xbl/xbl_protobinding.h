#include "PlatformString.h"

namespace WebCore
{
    class Element;
    class String;
}

namespace XBL
{
    class XBLDocument;
    class XBLPrototypeHandler;
    
class XBLPrototypeBinding
{
public:
    XBLPrototypeBinding(const WebCore::String& bindingID, WebCore::Element* bindingElt);
    void initialize(); // Called once the binding and all its child elements have been parsed.
    
    XBLDocument* document() const;
    WebCore::Element* element() const { return m_element; }
    
    void setHandler(XBLPrototypeHandler* handler) { m_handler = handler; }
    XBLPrototypeHandler* handler() const { return m_handler; }
    
    void addResource(const WebCore::String& type, const WebCore::String& src);

private:
    WebCore::String m_id;
    WebCore::Element* m_element;
    
    XBLPrototypeHandler* m_handler; // Our event handlers (prototypes).
};

}
