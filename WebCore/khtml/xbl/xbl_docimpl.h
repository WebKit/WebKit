namespace DOM {
    class DOMString;
}

namespace XBL {
    class XBLPrototypeBinding;
    
// This class represents an XBL document.  It subclasses XML documents and holds references to prototype bindings.
// The DOM of an XBL document is optimized to avoid creating full-blown DOM elements for most sections of the various
// bindings.  Custom prototype binding data structures are created instead.
class XBLDocumentImpl: public DOM::DocumentImpl
{
public:
    XBLDocumentImpl();
    ~XBLDocumentImpl();
    
    virtual khtml::XMLHandler* createTokenHandler();

    void setPrototypeBinding(const DOM::DOMString& id, XBLPrototypeBinding* binding);
    XBLPrototypeBinding* prototypeBinding(const DOM::DOMString& id);
    
private:
    // A mapping from URIs to bindings.  Used to look up specific prototype binding objects.
    QDict<XBLPrototypeBinding> m_prototypeBindingTable;
};

}
