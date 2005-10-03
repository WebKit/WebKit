#ifndef KHTML_NO_XBL

#include "config.h"
#include "qptrdict.h"
#include "xml/dom_docimpl.h"
#include "xml/dom_elementimpl.h"
#include "rendering/render_style.h"
#include "xbl_binding_manager.h"
#include "xbl_binding.h"

using DOM::DocumentImpl;
using DOM::NodeImpl;
using DOM::ElementImpl;
using khtml::BindingURI;

namespace XBL {

XBLBindingManager::XBLBindingManager(DocumentImpl* doc)
:m_document(doc), m_bindingChainTable(0)
{
}

XBLBindingManager::~XBLBindingManager()
{
    if (m_bindingChainTable) {
        m_bindingChainTable->setAutoDelete(true);
        delete m_bindingChainTable;
    }
}

XBLBindingChain* XBLBindingManager::getBindingChain(NodeImpl* node)
{
    if (!m_bindingChainTable)
        return 0;
    
    return m_bindingChainTable->find(node);
}

void XBLBindingManager::setBindingChain(NodeImpl* node, XBLBindingChain* bindingChain)
{
    if (!m_bindingChainTable)
        m_bindingChainTable = new QPtrDict<XBLBindingChain>;
    
    if (bindingChain)
        m_bindingChainTable->replace(node, bindingChain);
    else
        m_bindingChainTable->remove(node);
}

bool XBLBindingManager::loadBindings(NodeImpl* node, BindingURI* bindingURIs, 
                                     bool isStyleBinding, bool* resolveStyle)
{
    if (resolveStyle) *resolveStyle = false;
    if (!node->isElementNode()) return true;
    bool dirty = false;
    XBLBindingChain* bindingChain = getBindingChain(node);
    if (bindingChain && isStyleBinding) {
        // Check to see if the binding is already installed.  If so, then we don't need
        // to do anything.
        XBLBindingChain* styleBindingChain = bindingChain->firstStyleBindingChain();
        if (styleBindingChain) {
            if (styleBindingChain->markedForDeath())
                return false; // The old bindings haven't destroyed themselves/fired their destructors yet.
            
            // See if the URIs match.
            BindingURI* currURI = bindingURIs;
            XBLBindingChain* currBindingChain = styleBindingChain;
            for ( ; currURI && currBindingChain;
                  currURI = currURI->next(), currBindingChain = currBindingChain->nextChain()) {
                if (currBindingChain->uri() != currURI->uri()) {
                    styleBindingChain->markForDeath();
                    return false;
                }
            }
            if ((currURI && !currBindingChain) || (currBindingChain && !currURI)) {
                styleBindingChain->markForDeath();
                return false;
            }
            
            return bindingChain->loaded();
        }
    }

    bindingChain = getBindingChain(node);
    ElementImpl* elt = static_cast<ElementImpl*>(node);
    for (BindingURI* currURI = bindingURIs; currURI; currURI = currURI->next()) {
        XBLBindingChain* newBindingChain = new XBLBindingChain(elt, currURI->uri(), isStyleBinding);
        if (newBindingChain) {
            if (bindingChain && isStyleBinding)
                // Style bindings go on the end.  Regular bindings go on the front.
                bindingChain->lastBindingChain()->insertBindingChain(newBindingChain);
            else {
                // Place on the front.
                newBindingChain->insertBindingChain(bindingChain);
                setBindingChain(node, newBindingChain); 
            }
            
            dirty = true;
        }
        
        bindingChain = newBindingChain;
    }
    
    if (!bindingChain) return true; // We did nothing.
    
    // If the binding is completely loaded, then return true.  Otherwise return false.
    bindingChain = getBindingChain(node);
    bool loaded = bindingChain->loaded();
    if (loaded && resolveStyle)
        *resolveStyle = bindingChain->hasStylesheets();
    return loaded;
}

void XBLBindingManager::checkLoadState(ElementImpl* elt)
{
    XBLBindingChain* chain = getBindingChain(elt);
    if (chain && chain->loaded())
        elt->setChanged();
}

}

#endif // KHTML_NO_XBL
