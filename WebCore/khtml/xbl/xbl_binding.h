#include "CachedObjectClient.h"
#include "dom/dom_string.h"

namespace khtml
{
    class CachedXBLDocument;
}

namespace DOM
{
    class ElementImpl;
}

namespace XBL
{
class XBLPrototypeBinding;
class XBLBindingChain;

class XBLBinding: public khtml::CachedObjectClient
{
public:
    XBLBinding(XBLBindingChain* chain, const DOM::DOMString& uri, XBLBinding* derivedBinding = 0);
    virtual ~XBLBinding();

    bool loaded() const;
    bool hasStylesheets() const { return false; } // FIXME: Implement scoped sheets.
    
    XBLBindingChain* chain() const { return m_chain; }

    void setNextBinding(XBLBinding* n) { m_nextBinding = n; }
    void setPreviousBinding(XBLBinding* p) { m_previousBinding = p; }
    
    // CachedObjectClient
    void setXBLDocument(const DOM::DOMString& url, XBL::XBLDocumentImpl* doc);
    
private:
    XBLBindingChain* m_chain; // The owning binding chain.
    khtml::CachedXBLDocument* m_xblDocument; // The prototype XBL document.
    XBLPrototypeBinding* m_prototype; // The prototype binding that corresponds to our binding.
    DOM::DOMString m_id; // The id of the binding.

    XBLBinding* m_previousBinding; // The previous explicit connection.
    XBLBinding* m_nextBinding; // The next explicit connection (e.g., a base binding specified via the extends attribute).
};

class XBLBindingChain
{
public:
    XBLBindingChain(DOM::ElementImpl* elt, const DOM::DOMString& uri, bool isStyleChain);
    ~XBLBindingChain();
    
    const DOM::DOMString& uri() const { return m_uri; }
    
    XBLBindingChain* firstStyleBindingChain();
    XBLBindingChain* lastBindingChain() ;
    void insertBindingChain(XBLBindingChain* bindingChain);
    
    XBLBindingChain* nextChain() const { return m_nextChain; }
    XBLBindingChain* previousChain() const { return m_previousChain; }
    
    void setNextBindingChain(XBLBindingChain* bindingChain) { m_nextChain = bindingChain; }
    void setPreviousBindingChain(XBLBindingChain* bindingChain) { m_previousChain = bindingChain; }
    
    bool markedForDeath() const { return m_markedForDeath; }
    void markForDeath();

    bool loaded() const;
    bool hasStylesheets() const;
    
    DOM::ElementImpl* element() const { return m_element; }
    
    void failed();
    
private:
    DOM::DOMString m_uri; // The URI of the binding that is the leaf of this chain.
    DOM::ElementImpl* m_element; // The bound element. All the bindings in the chain are attached to this element.
    XBLBinding* m_binding; // The bindings contained within this chain.
    XBLBindingChain* m_previousChain; // The previous implicit connection.
    XBLBindingChain* m_nextChain; // The next implicit connection (e.g., from addBinding or multiple urls in a CSS binding declaration).
    
    bool m_isStyleChain : 1;
    bool m_markedForDeath : 1;
};

}
