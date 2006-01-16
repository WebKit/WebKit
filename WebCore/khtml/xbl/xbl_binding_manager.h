namespace khtml {
    class CachedXBLDocument;
    class BindingURI;
}

namespace DOM {
    class DocumentImpl;
    class NodeImpl;
    class ElementImpl;
}

namespace XBL {

class XBLBindingChain;
class XBLChildNodeList;
    
// This class handles all access to XBL bindings and XBL binding information.  It also handles the loading/installation of XBL
// bindings.  In the comments below, the term "decorated DOM" is used to describe the final DOM that can be walked after all
// bindings have been installed.
class XBLBindingManager
{
public:
    XBLBindingManager(DOM::DocumentImpl* doc);
    ~XBLBindingManager();
    
    bool loadBindings(DOM::NodeImpl* node, khtml::BindingURI* bindingURIs, bool isStyleBinding, bool* resolveStyle);
    void checkLoadState(DOM::ElementImpl* elt);
    
    XBLBindingChain* getBindingChain(DOM::NodeImpl* node);
    void setBindingChain(DOM::NodeImpl* node, XBLBindingChain* binding);

private:
    // The document whose bindings we manage.
    DOM::DocumentImpl* m_document;

    // A mapping from a DOM::NodeImpl* to the XBL::XBLBindingChain* that is
    // installed on that element.
    QPtrDict<XBLBindingChain>* m_bindingChainTable;
    
    // A mapping from DOM::NodeImpl* to an XBLChildNodeListImpl*.  
    // This list contains an accurate reflection of our *explicit* children (once intermingled with
    // insertion points) in the decorated DOM.
    QPtrDict<XBLChildNodeList>* m_explicitChildNodesTable;
    
    // A mapping from DOM::NodeImpl* to an XBLChildNodeListImpl*.  This list contains an accurate
    // reflection of our *anonymous* children (if and only if they are
    // intermingled with insertion points) in the decorated DOM.  This
    // table is not used if no insertion points were defined directly
    // underneath a <content> tag in a binding.  The NodeList from the
    // binding's cloned <content> element is used instead as a performance optimization.
    QPtrDict<XBLChildNodeList>* m_anonymousChildNodesTable;
    
    // A mapping from a DOM::NodeImpl* to a DOM::NodeImpl*.  Used to obtain the node's parent
    // element in the decorated DOM.
    QPtrDict<DOM::NodeImpl>* m_parentElementTable;
    
    // A mapping from 
};

}
