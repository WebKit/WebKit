namespace WebCore {
    class String;
}

namespace XBL {
    class XBLPrototypeBinding;
    
// This class represents an XBL document.  It subclasses XML documents and holds references to prototype bindings.
// The DOM of an XBL document is optimized to avoid creating full-blown DOM elements for most sections of the various
// bindings.  Custom prototype binding data structures are created instead.
class XBLDocument: public WebCore::Document
{
public:
    XBLDocument();
    ~XBLDocument();
    
    virtual WebCore::XMLHandler* createTokenHandler();

    void setPrototypeBinding(const WebCore::String& id, XBLPrototypeBinding* binding);
    XBLPrototypeBinding* prototypeBinding(const WebCore::String& id);
    
private:
    // A mapping from URIs to bindings.  Used to look up specific prototype binding objects.
    QDict<XBLPrototypeBinding> m_prototypeBindingTable;
};

}
