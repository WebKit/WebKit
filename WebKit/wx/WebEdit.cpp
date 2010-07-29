

#include "config.h"
#include "WebEdit.h"

#include "AtomicString.h"
#include "CompositeEditCommand.h"
#include "Document.h"
#include "Frame.h"
#include "HTMLNames.h"
#include "QualifiedName.h"
#include "StringImpl.h"

#include "WebFrame.h"
#include "WebDOMElement.h"

namespace WebCore {

class WebCoreEditCommand: public CompositeEditCommand
{
public:
    WebCoreEditCommand(WebCore::Document* document)
        : CompositeEditCommand(document)
        { }
        
    void setElementAttribute(PassRefPtr<Element> element, const QualifiedName& attribute, const AtomicString& value) 
    { 
        setNodeAttribute(element, attribute, value); 
    }
    // composite commands are applied as they are added, so we don't
    // need doApply to do anything.
    virtual void doApply() {}
};

}

wxWebEditCommand::wxWebEditCommand(wxWebFrame* webframe)
{
    if (webframe) {
        WebCore::Frame* frame = webframe->GetFrame();
        if (frame && frame->document())
            m_impl = new WebCore::WebCoreEditCommand(frame->document());
            m_impl->ref();
    }
}

wxWebEditCommand::~wxWebEditCommand()
{
    // the impl. is ref-counted, so don't delete it as it may be in an undo/redo stack
    if (m_impl)
        m_impl->deref();
    m_impl = 0;
}

void wxWebEditCommand::SetNodeAttribute(WebDOMElement* element, const wxString& name, const wxString& value)
{
    if (m_impl)
        m_impl->setElementAttribute(element->impl(), WebCore::QualifiedName(WebCore::nullAtom, WebCore::String(name), WebCore::nullAtom), WebCore::String(value));
}

void wxWebEditCommand::Apply()
{
    if (m_impl)
        m_impl->apply();
}