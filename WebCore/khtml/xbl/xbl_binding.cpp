#ifndef KHTML_NO_XBL

#include "config.h"
#include <kurl.h>
#include "xbl_protobinding.h"
#include "xbl_binding.h"
#include "xbl_docimpl.h"
#include "xbl_binding_manager.h"
#include "misc/loader.h"

using DOM::DOMString;
using DOM::ElementImpl;
using DOM::DocumentImpl;
using khtml::CachedXBLDocument;

namespace XBL
{

XBLBindingChain::XBLBindingChain(ElementImpl* elt, const DOMString& uri, bool isStyleChain)
:m_uri(uri), m_element(elt), m_previousChain(0), m_nextChain(0), m_isStyleChain(isStyleChain), 
 m_markedForDeath(false)
{
    m_binding = new XBLBinding(this, uri);
}

XBLBindingChain::~XBLBindingChain()
{
    delete m_binding;
    delete m_nextChain;
}

XBLBindingChain* XBLBindingChain::firstStyleBindingChain()
{
    if (m_isStyleChain)
        return this;
    if (m_nextChain)
        return m_nextChain->firstStyleBindingChain();
    return 0;
}

XBLBindingChain* XBLBindingChain::lastBindingChain()
{
    if (m_nextChain)
        return m_nextChain->lastBindingChain();
    return this;
}

void XBLBindingChain::insertBindingChain(XBLBindingChain* bindingChain)
{
    if (m_nextChain) {
        m_nextChain->setPreviousBindingChain(bindingChain);
        bindingChain->setNextBindingChain(m_nextChain);
    }
    
    if (bindingChain)
        bindingChain->setPreviousBindingChain(this);
    setNextBindingChain(bindingChain);
}

void XBLBindingChain::markForDeath()
{
    m_markedForDeath = true;
    if (m_nextChain && m_isStyleChain)
        m_nextChain->markForDeath();

    // FIXME: Schedule a timer that will fire as soon as possible and destroy the binding.
}

bool XBLBindingChain::loaded() const
{
    if (m_binding && !m_binding->loaded())
        return false;
    if (m_nextChain)
        return m_nextChain->loaded();
    return true;
}

bool XBLBindingChain::hasStylesheets() const
{
    if (m_binding && m_binding->hasStylesheets())
        return true;
    if (m_nextChain)
        return m_nextChain->hasStylesheets();
    return false;
}

void XBLBindingChain::failed()
{
    delete m_binding;
    m_binding = 0;
    
    element()->getDocument()->bindingManager()->checkLoadState(element());
}

// ==========================================================================================

XBLBinding::XBLBinding(XBLBindingChain* ch, const DOMString& uri, XBLBinding* derivedBinding)
:m_chain(ch), m_xblDocument(0), m_prototype(0), m_previousBinding(0), m_nextBinding(0)
{
    if (derivedBinding)
        derivedBinding->setNextBinding(this);
    setPreviousBinding(derivedBinding);
    
    // Parse the URL into a document URI and a binding id.  Kick off the load of
    // the XBL document here, and cache the id that we hope to find once the document
    // has loaded in m_id.
    DOMString docURL = uri;
    int hashIndex = uri.find('#');
    if (hashIndex != -1) {
        QString url = uri.qstring();
        docURL = url.left(hashIndex);
        if (int(url.length()) > hashIndex+1)
            m_id = url.right(url.length()-hashIndex-1);
    }
    else
        m_id = "_xbl_first_binding";
    
    // Now kick off the load of the XBL document.
    DocumentImpl* doc = chain()->element()->getDocument();
    m_xblDocument = doc->docLoader()->requestXBLDocument(docURL);
    if (m_xblDocument)
        m_xblDocument->ref(this);
}

XBLBinding::~XBLBinding()
{
    delete m_nextBinding;
    if (m_xblDocument)
        m_xblDocument->deref(this);
}

bool XBLBinding::loaded() const
{
    // If our prototype document hasn't loaded, then we aren't ready yet.
    if (!m_prototype)
        return false;
    
    // FIXME: If all our resources haven't loaded, then we aren't ready.
    
    // If our base binding hasn't loaded, then we also aren't ready yet.
    if (m_nextBinding && !m_nextBinding->loaded())
        return false;

    // We're ready.
    return true;
}

void XBLBinding::setXBLDocument(const DOMString& url, XBLDocumentImpl* doc) 
{
    // Locate the prototype binding.  If it doesn't exist in the XBLDocument, then the entire binding
    // chain is considered invalid and should be destroyed.
    if (m_id == "_xbl_first_binding") {
        // FIXME: Obtain the ID of the first binding by examining the DOM.
    }
    
    // Obtain our prototype from the XBL document.
    m_prototype = doc->prototypeBinding(m_id);
    
    if (!m_prototype)
        return chain()->failed(); // This binding chain failed to load. Discard the chain.
    
    // See if we have an "extends" attribute.  If so, load that binding.
    DOMString extends = m_prototype->element()->getAttribute("extends");
    if (!extends.isEmpty()) {
        // FIXME: Add support for : extension for built-in types, e.g., "html:img".
        // Resolve to an absolute URL.
        new XBLBinding(chain(), 
                       KURL(m_prototype->document()->baseURL(), extends.qstring()).url(),
                       this);
        return;
    }
    
    chain()->element()->getDocument()->bindingManager()->checkLoadState(chain()->element());
}

}

#endif // KHTML_NO_XBL

