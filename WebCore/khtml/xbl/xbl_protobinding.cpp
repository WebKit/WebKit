#ifndef KHTML_NO_XBL

#include "config.h"
#include "xbl_protobinding.h"
#include "xbl_docimpl.h"

using DOM::DOMString;
using DOM::ElementImpl;

namespace XBL
{

XBLPrototypeBinding::XBLPrototypeBinding(const DOMString& id, ElementImpl* elt)
:m_id(id), m_element(elt), m_handler(0)
{
    // Add ourselves to the document's prototype table.
    document()->setPrototypeBinding(id, this);
}

void XBLPrototypeBinding::initialize()
{
}

XBLDocumentImpl* XBLPrototypeBinding::document() const
{
    return static_cast<XBLDocumentImpl*>(m_element->getDocument());
}

void XBLPrototypeBinding::addResource(const DOMString& type, const DOMString& src)
{
    // FIXME: Implement!
}

}

#endif
