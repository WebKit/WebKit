#ifndef KHTML_NO_XBL

#include "config.h"
#include "dom/dom_node.h"
#include "xml/dom_elementimpl.h"

#include "xbl_tokenizer.h"
#include "xbl_docimpl.h"
#include "xbl_protobinding.h"
#include "xbl_protohandler.h"
#include "xbl_protoimplementation.h"

using DOM::ElementImpl;
using DOM::Node;

namespace XBL {

const char xblNS[] = "http://www.mozilla.org/xbl";
    
XBLTokenHandler::XBLTokenHandler(DocumentImpl* doc)
:   XMLHandler(doc, 0),
    m_state(eXBL_InDocument),
    m_secondaryState(eXBL_None),
    m_binding(0),
    m_handler(0),
    m_implementation(0),
    m_member(0),
    m_property(0),
    m_method(0),
    m_field(0)
{
}

XBLTokenHandler::~XBLTokenHandler()
{
}

XBLDocumentImpl* XBLTokenHandler::xblDocument() const
{
    return static_cast<XBLDocumentImpl*>(m_doc->document());
}

bool XBLTokenHandler::startElement(const QString& namespaceURI, const QString& localName, const QString& qName, 
                                   const QXmlAttributes& attrs)
{
    if (namespaceURI == xblNS) {
        if (localName == "binding")
            m_state = eXBL_InBinding;
        else if (localName == "content")
            m_state = eXBL_InContent;
        else if (localName == "handlers") {
            m_state = eXBL_InHandlers;
            return true;
        }
        else if (localName == "handler") {
            m_secondaryState = eXBL_InHandler;
            createHandler(attrs);
            return true;
        }
        else if (localName == "resources") {
            m_state = eXBL_InResources;
            return true;
        }
        else if (m_state == eXBL_InResources) {
            if (localName == "stylesheet" || localName == "image")
                createResource(localName, attrs);
            return true;
        }
        else if (localName == "implementation") {
            m_state = eXBL_InImplementation;
            createImplementation(attrs);
            return true;
        }
        else if (m_state == eXBL_InImplementation) {
            if (localName == "constructor") {
                m_secondaryState = eXBL_InConstructor;
                createConstructor();
            }
            else if (localName == "destructor") {
                m_secondaryState = eXBL_InDestructor;
                createDestructor();
            }
            else if (localName == "field") {
                m_secondaryState = eXBL_InField;
                createField(attrs);
            }
            else if (localName == "property") {
                m_secondaryState = eXBL_InProperty;
                createProperty(attrs);
            }
            else if (localName == "getter")
                m_secondaryState = eXBL_InGetter;
            else if (localName == "setter")
                m_secondaryState = eXBL_InSetter;
            else if (localName == "method") {
                m_secondaryState = eXBL_InMethod;
                createMethod(attrs);
            }
            else if (localName == "parameter")
                createParameter(attrs);
            else if (localName == "body")
                m_secondaryState = eXBL_InBody;
            
            return true; // Ignore everything we encounter inside an <implementation> block.
        }        
    }
    
    if (!XMLHandler::startElement(namespaceURI, localName, qName, attrs))
        return false;
    
    // Create our binding if it doesn't exist already.
    if (m_state == eXBL_InBinding && !m_binding)
        createBinding();
    
    return true;
}

bool XBLTokenHandler::endElement(const QString& namespaceURI, const QString& localName, const QString& qName)
{
    if (m_state != eXBL_InDocument) {
        if (namespaceURI == xblNS) {
            if (m_state == eXBL_InContent && localName == "content")
                m_state = eXBL_InBinding;
            else if (m_state == eXBL_InHandlers) {
                if (localName == "handlers") {
                    m_state = eXBL_InBinding;
                    m_handler = 0;
                }
                else if (localName == "handler")
                    m_secondaryState = eXBL_None;
                return true;
            }
            else if (m_state == eXBL_InResources) {
                if (localName == "resources")
                    m_state = eXBL_InBinding;
                return true;
            }
            else if (m_state == eXBL_InImplementation) {
                if (localName == "implementation")
                    m_state = eXBL_InBinding;
                else if (localName == "property") {
                    m_secondaryState = eXBL_None;
                    m_property = 0;
                }
                else if (localName == "method") {
                    m_secondaryState = eXBL_None;
                    m_method = 0;
                }
                else if (localName == "field") {
                    m_secondaryState = eXBL_None;
                    m_field = 0;
                }
                else if (localName == "constructor" ||
                         localName == "destructor")
                    m_secondaryState = eXBL_None;
                else if (localName == "getter" ||
                         localName == "setter")
                    m_secondaryState = eXBL_InProperty;
                else if (localName == "parameter" ||
                         localName == "body")
                    m_secondaryState = eXBL_InMethod;
                return true;
            }
            
            if (!XMLHandler::endElement(namespaceURI, localName, qName))
                return false;
            
            if (m_state == eXBL_InImplementation && localName == "implementation")
                m_state = eXBL_InBinding;
            else if (m_state == eXBL_InBinding && localName == "binding") {
                m_state = eXBL_InDocument;
                m_binding->initialize();
                m_binding = 0;
            }
            return true;
        }
    }
    
    return XMLHandler::endElement(namespaceURI, localName, qName);
}

bool XBLTokenHandler::characters(const QString& text)
{
    if (text.length() == 0)
        return true;
    
    if (m_state == eXBL_InHandlers) {
        // Get the text and add it to the event handler.
        if (m_secondaryState == eXBL_InHandler)
            m_handler->appendData(text);
        return true;
    }
    else if (m_state == eXBL_InImplementation) {
        if (m_secondaryState == eXBL_InConstructor || m_secondaryState == eXBL_InDestructor || m_secondaryState == eXBL_InBody) {
            if (m_method)
                m_method->appendData(text);
        }
        else if (m_secondaryState == eXBL_InGetter)
            m_property->appendGetterText(text);
        else if (m_secondaryState == eXBL_InSetter)
            m_property->appendSetterText(text);
        else if (m_secondaryState == eXBL_InField)
            m_field->appendData(text);
        return true;
    }

    // XBL files munch all whitespace, except when inside an anonymous content template (<content>).
    if (m_state != eXBL_InContent) {
        uint l = text.length();
        uint i;
        for (i = 0; i < l; i++) {
	    if (!isspace(text[i].unicode()))
	        break;
	}
        if (i == l)
            return true;
    }
    
    return XMLHandler::characters(text);
}

void XBLTokenHandler::createBinding()
{
    if (!m_currentNode || m_currentNode->nodeType() != Node::ELEMENT_NODE)
        return;
    
    ElementImpl* elt = static_cast<ElementImpl*>(m_currentNode);
    DOMString id = elt->getAttribute(ATTR_ID);
    if (!id.isEmpty()) {
        m_binding = new XBLPrototypeBinding(id, elt);
        int exCode;
        elt->removeAttribute(ATTR_ID, exCode);
    }
}

void XBLTokenHandler::createHandler(const QXmlAttributes& attrs)
{
    DOMString event;
    DOMString modifiers;
    DOMString button;
    DOMString clickcount;
    DOMString keycode;
    DOMString charcode;
    DOMString phase;
    DOMString action;
    DOMString preventdefault;
    
    int i;
    for (i = 0; i < attrs.length(); i++) {
        if (attrs.uri(i).length() > 0)
            continue; // Only care about attributes with no namespace.
        
        if (attrs.qName(i) == "event")
            event = attrs.value(i);
        else if (attrs.qName(i) == "modifiers")
            modifiers = attrs.value(i);
        else if (attrs.qName(i) == "button")
            button = attrs.value(i);
        else if (attrs.qName(i) == "clickcount")
            clickcount = attrs.value(i);
        else if (attrs.qName(i) == "keycode")
            keycode = attrs.value(i);
        else if (attrs.qName(i) == "key" || attrs.qName(i) == "charcode")
            charcode = attrs.value(i);
        else if (attrs.qName(i) == "phase")
            phase = attrs.value(i);
        else if (attrs.qName(i) == "action")
            action = attrs.value(i);
        else if (attrs.qName(i) == "preventdefault")
            preventdefault = attrs.value(i);
    }
    
    XBLPrototypeHandler* newHandler = new XBLPrototypeHandler(event, phase, action, keycode, 
                                                              charcode, modifiers, button,
                                                              clickcount, preventdefault, m_binding);
    if (newHandler) {
        // Add this handler to our chain of handlers.
        if (m_handler)
            m_handler->setNext(newHandler); // Already have a chain. Just append to the end.
        else
            m_binding->setHandler(newHandler); // We're the first handler in the chain.
        m_handler = newHandler; // Adjust our m_handler pointer to point to the new last handler in the chain.
    }
}

void XBLTokenHandler::createResource(const QString& localName, const QXmlAttributes& attrs)
{
    if (!m_binding)
        return;
    
    for (int i = 0; i < attrs.length(); i++) {
        if (attrs.uri(i).length() > 0)
            continue;
        
        if (attrs.qName(i) == "src") {
            m_binding->addResource(localName, attrs.value(i));
            break;
        }
    }
}

void XBLTokenHandler::createImplementation(const QXmlAttributes& attrs)
{
    m_implementation = 0;
    m_member = 0;
    
    if (!m_binding)
        return;
    
    DOMString name;    
    for (int i = 0; i < attrs.length(); i++) {
        if (attrs.uri(i).length() > 0)
            continue;
        
        if (attrs.qName(i) == "name")
            name = attrs.value(i);
        else if (attrs.qName(i) == "implements") {
            // FIXME: If we implement any sort of interface language binding, then we can parse the list of
            // implemented interfaces here.
            // m_binding->constructInterfaceTable(attrs.value(i));
        }
    }
    
    m_implementation = new XBLPrototypeImplementation(name, m_binding);
}

void XBLTokenHandler::addMember(XBLPrototypeMember* member)
{
    if (m_member)
        m_member->setNext(member);
    else
        m_implementation->setMember(member);
    m_member = member;
}

void XBLTokenHandler::createConstructor()
{
    m_method = new XBLPrototypeConstructor();
    addMember(m_method);
}

void XBLTokenHandler::createDestructor()
{
    m_method = new XBLPrototypeDestructor();
    addMember(m_method);
}

void XBLTokenHandler::createField(const QXmlAttributes& attrs)
{
    DOMString name;
    bool readonly = false;
    for (int i = 0; i < attrs.length(); i++) {
        if (attrs.uri(i).length() > 0)
            continue;
        if (attrs.qName(i) == "name")
            name = attrs.value(i);
        else if (attrs.qName(i) == "readonly")
            readonly = !(strcasecmp(attrs.value(i), "true"));
    }
    
    m_field = new XBLPrototypeField(name, readonly);
    addMember(m_field);
}

void XBLTokenHandler::createProperty(const QXmlAttributes& attrs)
{
    DOMString name, onget, onset;
    bool readonly = false;
    for (int i = 0; i < attrs.length(); i++) {
        if (attrs.uri(i).length() > 0)
            continue;
        if (attrs.qName(i) == "name")
            name = attrs.value(i);
        else if (attrs.qName(i) == "readonly")
            readonly = !(strcasecmp(attrs.value(i), "true"));
        else if (attrs.qName(i) == "onget")
            onget = attrs.value(i);
        else if (attrs.qName(i) == "onset")
            onset = attrs.value(i);
    }
    
    m_property = new XBLPrototypeProperty(name, readonly, onget, onset);
    addMember(m_property);
}

void XBLTokenHandler::createMethod(const QXmlAttributes& attrs)
{
    DOMString name;
    for (int i = 0; i < attrs.length(); i++) {
        if (attrs.uri(i).length() > 0)
            continue;
        if (attrs.qName(i) == "name")
            name = attrs.value(i);
    }
    
    m_method = new XBLPrototypeMethod(name);
    addMember(m_method);
}

void XBLTokenHandler::createParameter(const QXmlAttributes& attrs)
{
    if (!m_method) return;

    for (int i = 0; i < attrs.length(); i++) {
        if (attrs.uri(i).length() > 0)
            continue;
        
        if (attrs.qName(i) == "name") {
            m_method->addParameter(attrs.value(i));
            break;
        }
    }
}

}

#endif
