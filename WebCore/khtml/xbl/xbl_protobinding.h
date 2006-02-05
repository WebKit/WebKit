#include "PlatformString.h"

namespace DOM
{
    class ElementImpl;
    class DOMString;
}

namespace XBL
{
    class XBLDocumentImpl;
    class XBLPrototypeHandler;
    
class XBLPrototypeBinding
{
public:
    XBLPrototypeBinding(const DOM::DOMString& bindingID, DOM::ElementImpl* bindingElt);
    void initialize(); // Called once the binding and all its child elements have been parsed.
    
    XBLDocumentImpl* document() const;
    DOM::ElementImpl* element() const { return m_element; }
    
    void setHandler(XBLPrototypeHandler* handler) { m_handler = handler; }
    XBLPrototypeHandler* handler() const { return m_handler; }
    
    void addResource(const DOM::DOMString& type, const DOM::DOMString& src);

private:
    DOM::DOMString m_id;
    DOM::ElementImpl* m_element;
    
    XBLPrototypeHandler* m_handler; // Our event handlers (prototypes).
};

}
