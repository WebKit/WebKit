

#include "config.h"
#include "WebEdit.h"

#include "CompositeEditCommand.h"
#include "Document.h"
#include "Frame.h"
#include "HTMLNames.h"
#include "QualifiedName.h"
#include "StringImpl.h"

#include "WebFrame.h"
#include "WebDOMElement.h"
#include <wtf/text/AtomicString.h>

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

class WebCoreEditCommandPrivate {
public:
    WebCoreEditCommandPrivate()
        : m_ptr(0)
    { }
    
    WebCoreEditCommandPrivate(WebCore::WebCoreEditCommand* ptr)
        : m_ptr(adoptRef(ptr))
    { }
    
    ~WebCoreEditCommandPrivate() { }
    
    WebCore::WebCoreEditCommand* command() { return m_ptr.get(); }
        
    RefPtr<WebCore::WebCoreEditCommand> m_ptr;
};

wxWebEditCommand::wxWebEditCommand(wxWebFrame* webframe)
{
    if (webframe) {
        WebCore::Frame* frame = webframe->GetFrame();
        if (frame && frame->document())
            m_impl = new WebCoreEditCommandPrivate(new WebCore::WebCoreEditCommand(frame->document()));
    }
}

wxWebEditCommand::~wxWebEditCommand()
{
    // the impl. is ref-counted, so don't delete it as it may be in an undo/redo stack
    delete m_impl;
    m_impl = 0;
}

void wxWebEditCommand::SetNodeAttribute(WebDOMElement* element, const wxString& name, const wxString& value)
{
    if (m_impl && m_impl->command())
        m_impl->command()->setElementAttribute(element->impl(), WebCore::QualifiedName(WTF::nullAtom, WTF::String(name), WTF::nullAtom), WTF::String(value));
}

void wxWebEditCommand::Apply()
{
    if (m_impl && m_impl->command())
        m_impl->command()->apply();
}
