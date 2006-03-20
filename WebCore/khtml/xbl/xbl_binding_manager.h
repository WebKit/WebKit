namespace WebCore {
    class CachedXBLDocument;
    class BindingURI;
}

namespace WebCore {
    class Document;
    class Node;
    class Element;
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
    XBLBindingManager(WebCore::Document* doc);
    ~XBLBindingManager();
    
    bool loadBindings(WebCore::Node* node, WebCore::BindingURI* bindingURIs, bool isStyleBinding, bool* resolveStyle);
    void checkLoadState(WebCore::Element* elt);
    
    XBLBindingChain* getBindingChain(WebCore::Node* node);
    void setBindingChain(WebCore::Node* node, XBLBindingChain* binding);

private:
    // The document whose bindings we manage.
    WebCore::Document* m_document;

    // A mapping from a WebCore::Node* to the XBL::XBLBindingChain* that is
    // installed on that element.
    QPtrDict<XBLBindingChain>* m_bindingChainTable;
    
    // A mapping from WebCore::Node* to an XBLChildNodeList*.  
    // This list contains an accurate reflection of our *explicit* children (once intermingled with
    // insertion points) in the decorated DOM.
    QPtrDict<XBLChildNodeList>* m_explicitChildNodesTable;
    
    // A mapping from WebCore::Node* to an XBLChildNodeList*.  This list contains an accurate
    // reflection of our *anonymous* children (if and only if they are
    // intermingled with insertion points) in the decorated DOM.  This
    // table is not used if no insertion points were defined directly
    // underneath a <content> tag in a binding.  The NodeList from the
    // binding's cloned <content> element is used instead as a performance optimization.
    QPtrDict<XBLChildNodeList>* m_anonymousChildNodesTable;
    
    // A mapping from a WebCore::Node* to a WebCore::Node*.  Used to obtain the node's parent
    // element in the decorated DOM.
    QPtrDict<WebCore::Node>* m_parentElementTable;
    
    // A mapping from 
};

}
