#ifndef KHTML_NO_XBL

#include "config.h"
#include "xbl_docimpl.h"
#include "xbl_tokenizer.h"
#include "xbl_protobinding.h"

using DOM::DocumentImpl;
using khtml::XMLHandler;

namespace XBL {

XBLDocumentImpl::XBLDocumentImpl()
:DocumentImpl(0,0)
{
    m_prototypeBindingTable.setAutoDelete(true); // The prototype bindings will be deleted when the XBL document dies.
}

XBLDocumentImpl::~XBLDocumentImpl()
{
}

XMLHandler* XBLDocumentImpl::createTokenHandler()
{
    return new XBLTokenHandler(docPtr());
}

void XBLDocumentImpl::setPrototypeBinding(const DOM::DOMString& id, XBLPrototypeBinding* binding)
{
    m_prototypeBindingTable.replace(id.qstring(), binding);
}

XBLPrototypeBinding* XBLDocumentImpl::prototypeBinding(const DOM::DOMString& id)
{
    if (id.length() == 0)
        return 0;
    
    return m_prototypeBindingTable.find(id.qstring());
}

}

#endif
