/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>

    Based on khtml code by:
    Copyright (C) 1999 Lars Knoll (knoll@kde.org)
              (C) 1999 Antti Koivisto (koivisto@kde.org)
              (C) 2001 Dirk Mueller (mueller@kde.org)
              (C) 2002-2003 Apple Computer, Inc.

    This file is part of the KDE project

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#include "config.h"
#include <assert.h>

#include <klocale.h>

#include <q3ptrstack.h>
#include <q3paintdevicemetrics.h>

#include "kdom.h"
#include <kdom/ecma/Ecma.h>
#include <kdom/Helper.h>
#include "TextImpl.h"
#include "AttrImpl.h"
#include "KDOMView.h"
#include "KDOMPart.h"
#include "Namespace.h"
#include "KDOMLoader.h"
#include "EntityImpl.h"
#include "ElementImpl.h"
#include "CommentImpl.h"
#include "NotationImpl.h"
#include "DocumentImpl.h"
#include "CDFInterface.h"
#include "DOMStringImpl.h"
#include "XMLElementImpl.h"
#include "StyleSheetImpl.h"
#include "TagNodeListImpl.h"
#include "KDOMCacheHelper.h"
#include "NamedNodeMapImpl.h"
#include "CDATASectionImpl.h"
#include "NodeIteratorImpl.h"
#include "kdom/css/CSSStyleSelector.h"
#include "DocumentTypeImpl.h"
#include "CSSStyleSheetImpl.h"
#include "StyleSheetListImpl.h"
#include "EntityReferenceImpl.h"
#include "DOMConfigurationImpl.h"
#include "DocumentFragmentImpl.h"
#include "DOMImplementationImpl.h"
#include "ProcessingInstructionImpl.h"

#include "domattrs.c"
#include "domattrs.h"

using namespace KDOM;
using namespace KDOM::XPath;
using namespace KDOM::XPointer;

DocumentImpl::DocumentImpl(DOMImplementationImpl *i, KDOMView *view, int nrTags, int nrAttrs)
: NodeBaseImpl(new DocumentPtr()), DocumentCSSImpl(), DocumentViewImpl(), DocumentEventImpl(),
  DocumentRangeImpl(), DocumentTraversalImpl(), XPointerEvaluatorImpl(), XPathEvaluatorImpl(), m_implementation(i)
{
    document->doc = this;
    m_view = view;
    m_cssTarget = 0;
    m_hoverNode = 0;
    m_focusNode = 0;
    m_ecmaEngine = 0;
    m_elementSheet = 0;
    m_listenerTypes = 0;

    m_xmlStandalone = false;

    m_xmlEncoding = 0;
    m_inputEncoding = 0;

    m_xmlVersion = new DOMStringImpl("1.0"); // we support XML feature, so use this default
    m_xmlVersion->ref();

    m_strictErrorChecking = true;

    m_paintDevice = 0;
    m_paintDeviceMetrics = 0;

    m_docLoader = new DocumentLoader((m_view ? m_view->part() : 0), this);
    
    m_attrMap = new IdNameMapping(nrAttrs > 0 ? nrAttrs : ATTR_LAST_DOMATTR + 1);
    m_elementMap = new IdNameMapping(nrTags > 0 ? nrTags : 1);
    m_namespaceMap = new IdNameMapping(1);

    m_styleSelector = 0;

    m_styleSheets = new StyleSheetListImpl();
    m_styleSheets->ref();

    m_pendingStylesheets = 0;
    m_ignorePendingStylesheets = false;

    m_parsingMode = false;
    m_usesDescendantRules = false;

    m_configuration = 0;

    m_kdomDocType = KDOM_DOC_GENERIC;
}

DocumentImpl::~DocumentImpl()
{
    delete m_ecmaEngine;
    delete m_docLoader;

    delete m_attrMap;
    delete m_elementMap;
    delete m_namespaceMap;

    if(m_focusNode)
        m_focusNode->deref();
    if(m_hoverNode)
        m_hoverNode->deref();
    if(m_elementSheet)
        m_elementSheet->deref();
    if(m_xmlVersion)
        m_xmlVersion->deref();
    if(m_xmlEncoding)
        m_xmlEncoding->deref();
    if(m_inputEncoding)
        m_inputEncoding->deref();
    if(m_configuration)
        m_configuration->deref();

    delete m_styleSelector;
    delete m_paintDeviceMetrics;
}

DOMImplementationImpl *DocumentImpl::implementation() const
{
    return m_implementation;
}

DocumentTypeImpl *DocumentImpl::doctype() const
{
    for(NodeImpl *n = firstChild(); n != 0; n = n->nextSibling())
    {
        if(n->nodeType() == DOCUMENT_TYPE_NODE)
            return static_cast<DocumentTypeImpl *>(n);
    }

    return 0;
}

void DocumentImpl::setDocType(DocumentTypeImpl *_doctype)
{
    DocumentTypeImpl *existing = doctype();
    if(existing)
        replaceChild(_doctype, existing);
    else
    {
        appendChild(_doctype);
        _doctype->setOwnerDocument(this);
    }
}

DOMStringImpl *DocumentImpl::nodeName() const
{
    return new DOMStringImpl("#document");
}

DOMStringImpl *DocumentImpl::textContent() const
{
    return 0;
}

unsigned short DocumentImpl::nodeType() const
{
    return DOCUMENT_NODE;
}

bool DocumentImpl::childTypeAllowed(unsigned short type) const
{
    switch(type)
    {
        case ELEMENT_NODE:
        case PROCESSING_INSTRUCTION_NODE:
        case COMMENT_NODE:
        case DOCUMENT_TYPE_NODE:
            return true;
            break;
        default:
            return false;
    }
}

ElementImpl *DocumentImpl::documentElement() const
{
    for(NodeImpl *n = firstChild(); n != 0; n = n->nextSibling())
    {
        if(n->nodeType() == ELEMENT_NODE)
            return static_cast<ElementImpl *>(n);
    }

    return 0;
}

ElementImpl *DocumentImpl::createElement(DOMStringImpl *tagName)
{
    // INVALID_CHARACTER_ERR: Raised if the specified name contains an illegal character.
    if(!Helper::ValidateQualifiedName(tagName))
        throw new DOMExceptionImpl(INVALID_CHARACTER_ERR);

    NodeImpl::Id id = getId(NodeImpl::ElementId, tagName, false);
    return new XMLElementImpl(document, id);
}

ElementImpl *DocumentImpl::createElementNS(DOMStringImpl *namespaceURI, DOMStringImpl *qualifiedName)
{
    if(namespaceURI)
        namespaceURI->ref();
    if(qualifiedName)
        qualifiedName->ref();

    int dummy;
    Helper::CheckQualifiedName(qualifiedName, namespaceURI, dummy, false, false);

    DOMStringImpl *prefix = 0, *localName = 0;
    Helper::SplitPrefixLocalName(qualifiedName, prefix, localName);

    if(prefix)
        prefix->ref();
    if(localName)
        localName->ref();

    NodeImpl::Id id = getId(NodeImpl::ElementId, namespaceURI, prefix, localName, false);
    ElementImpl *ret = new XMLElementImpl(document, id, prefix, !namespaceURI);

    if(prefix)
        prefix->deref();
    if(localName)
        localName->deref();
    if(namespaceURI)
        namespaceURI->deref();
    if(qualifiedName)
        qualifiedName->deref();

    return ret;
}

AttrImpl *DocumentImpl::createAttribute(DOMStringImpl *name)
{
    // INVALID_CHARACTER_ERR: Raised if the specified name contains an illegal character.
    if(!Helper::ValidateAttributeName(name))
        throw new DOMExceptionImpl(INVALID_CHARACTER_ERR);

    NodeImpl::Id id = getId(NodeImpl::AttributeId, name, false);
    return new AttrImpl(document, id);
}

AttrImpl *DocumentImpl::createAttributeNS(DOMStringImpl *namespaceURI, DOMStringImpl *qualifiedName)
{
    if(namespaceURI)
        namespaceURI->ref();
    if(qualifiedName)
        qualifiedName->ref();

    int dummy;
    Helper::CheckQualifiedName(qualifiedName, namespaceURI, dummy, false, false);

    DOMStringImpl *prefix = 0, *localName = 0;
    Helper::SplitPrefixLocalName(qualifiedName, prefix, localName);

    if(prefix)
        prefix->ref();
    if(localName)
        localName->ref();

    NodeImpl::Id id = getId(NodeImpl::AttributeId, namespaceURI, prefix, localName, false);
    AttrImpl *ret = new AttrImpl(document, id, 0, prefix, !namespaceURI);

    if(prefix)
        prefix->deref();
    if(localName)
        localName->deref();
    if(namespaceURI)
        namespaceURI->deref();
    if(qualifiedName)
        qualifiedName->deref();

    return ret;
}

DocumentFragmentImpl *DocumentImpl::createDocumentFragment()
{
    return new DocumentFragmentImpl(docPtr());
}

CommentImpl *DocumentImpl::createComment(DOMStringImpl *data)
{
    return new CommentImpl(docPtr(), data);
}

CDATASectionImpl *DocumentImpl::createCDATASection(DOMStringImpl *data)
{
    return new CDATASectionImpl(docPtr(), data);
}

EntityReferenceImpl *DocumentImpl::createEntityReference(DOMStringImpl *data)
{
    // INVALID_CHARACTER_ERR: Raised if the specified name contains an illegal character.
    if(!Helper::ValidateAttributeName(data))
        throw new DOMExceptionImpl(INVALID_CHARACTER_ERR);

    return new EntityReferenceImpl(docPtr(), data);
}

ProcessingInstructionImpl *DocumentImpl::createProcessingInstruction(DOMStringImpl *target, DOMStringImpl *data)
{
    // INVALID_CHARACTER_ERR: Raised if the specified name contains an illegal character.
    if(!Helper::ValidateAttributeName(target))
        throw new DOMExceptionImpl(INVALID_CHARACTER_ERR);

    return new ProcessingInstructionImpl(docPtr(), target, data);
}

TextImpl *DocumentImpl::createTextNode(DOMStringImpl *data)
{
    return new TextImpl(docPtr(), data);
}

NodeImpl *DocumentImpl::cloneNode(bool deep, DocumentPtr *) const
{
    DOMStringImpl *namespaceURI = documentElement() ? documentElement()->namespaceURI() : 0;
    DOMStringImpl *qualifiedName = documentElement() ? documentElement()->nodeName() : 0;

    // TODO: Is passing view=0 correct? Investigate!
    DocumentImpl *clone = implementation()->createDocument(namespaceURI, qualifiedName, 0, false, 0);
    clone->ref();
    clone->setXmlVersion(xmlVersion());
    clone->setXmlEncoding(xmlEncoding());
    clone->setInputEncoding(inputEncoding());
    clone->setXmlStandalone(xmlStandalone());

    if(deep)
        cloneChildNodes(clone, clone->docPtr());

    return clone;
}

int DocumentImpl::addListenerType(DOMStringImpl *type)
{
    int eventId = implementation()->typeToId(type);
    addListenerType(eventId);
    return eventId;
}

int DocumentImpl::removeListenerType(DOMStringImpl *type)
{
    int eventId = implementation()->typeToId(type);
    removeListenerType(eventId);
    return eventId;
}

bool DocumentImpl::hasListenerType(DOMStringImpl *type) const
{
    int eventId = implementation()->typeToId(type);
    return hasListenerType(eventId);
}

void DocumentImpl::addListenerType(int eventId)
{
    m_listenerTypes |= (1 << eventId);
}

void DocumentImpl::removeListenerType(int eventId)
{
    m_listenerTypes &= ~(1 << eventId);
}

bool DocumentImpl::hasListenerType(int eventId) const
{
    return m_listenerTypes & (1 << eventId);
}

Ecma *DocumentImpl::ecmaEngine() const
{
    if(m_ecmaEngine) // No need to initialize anymore...
        return m_ecmaEngine;

    m_ecmaEngine = new Ecma(const_cast<DocumentImpl *>(this));
    m_ecmaEngine->setup(implementation()->cdfInterface());

    return m_ecmaEngine;
}

CSSStyleSheetImpl *DocumentImpl::elementSheet() const
{
    if(!m_elementSheet)
    {
        m_elementSheet = new CSSStyleSheetImpl(const_cast<DocumentImpl *>(this),
                                               new DOMStringImpl(documentKURI().url()));
        m_elementSheet->ref();
    }

    return m_elementSheet;
}

NodeListImpl *DocumentImpl::getElementsByTagName(DOMStringImpl *tagName)
{
    return new TagNodeListImpl(this, tagName);
}

NodeListImpl *DocumentImpl::getElementsByTagNameNS(DOMStringImpl *namespaceURI, DOMStringImpl *localName)
{
    return new TagNodeListImpl(this, localName, namespaceURI);
}

NodeImpl *DocumentImpl::importNode(NodeImpl *importedNode, bool deep)
{
    NodeImpl *result = 0;

    // Not mentioned in spec: throw NOT_FOUND_ERR if importedNode is null
    if(!importedNode)
        throw new DOMExceptionImpl(NOT_FOUND_ERR);

    const bool oldMode = parsing();
    setParsing(true);

    if(importedNode->nodeType() == ATTRIBUTE_NODE)
    {
        result = importedNode->cloneNode(true, docPtr());
        deep = false;
    }
    else if(importedNode->nodeType() == ELEMENT_NODE)
        result = importedNode->cloneNode(false, docPtr());
    else if(importedNode->nodeType() == DOCUMENT_FRAGMENT_NODE)
        result = createDocumentFragment();
    else if(importedNode->nodeType() == ENTITY_NODE)
    {
        EntityImpl *importedEntity = static_cast<EntityImpl *>(importedNode);
        result = importedEntity->cloneNode(false, docPtr());
    }
    else if(importedNode->nodeType() == ENTITY_REFERENCE_NODE)
    {
        setParsing(false);
        result = createEntityReference(importedNode->nodeName());
        deep = false;
    }
    else if(importedNode->nodeType() == NOTATION_NODE)
    {
        NotationImpl *importedNotation = static_cast<NotationImpl *>(importedNode);
        NotationImpl *temp = new NotationImpl(docPtr(), importedNotation->publicId(), importedNotation->systemId());

        result = temp;
        deep = false;
    }
    else if(importedNode->nodeType() == PROCESSING_INSTRUCTION_NODE)
    {
        result = createProcessingInstruction(importedNode->nodeName(), importedNode->nodeValue());
        deep = false;
    }
    else if(importedNode->nodeType() == COMMENT_NODE)
    {
        CommentImpl *importedComment = static_cast<CommentImpl *>(importedNode);
        
        result = createComment(importedComment->nodeValue());
        deep = false;
    }
    else if(importedNode->nodeType() == TEXT_NODE)
    {
        TextImpl *importedText = static_cast<TextImpl *>(importedNode);

        result = createTextNode(importedText->nodeValue());
        deep = false;
    }
    else if(importedNode->nodeType() == CDATA_SECTION_NODE)
    {
        CDATASectionImpl *importedCD = static_cast<CDATASectionImpl *>(importedNode);

        result = createCDATASection(importedCD->nodeValue());
        deep = false;
    }

    if(deep)
    {
        for(NodeImpl *n = importedNode->firstChild(); n != 0; n = n->nextSibling())
            result->appendChild(importNode(n, true));
    }

    setParsing(oldMode);
    return result;
}

void DocumentImpl::normalizeDocument()
{
    for(NodeImpl *child = firstChild();child != 0; child = next)
    {
        next = child->nextSibling();
        child = normalizeNode(child);
        if(child != 0)
            next = child;
    }
}

NodeImpl *DocumentImpl::renameNode(NodeImpl *n, DOMStringImpl *namespaceURI, DOMStringImpl *qualifiedName)
{
    // NO_MODIFICATION_ALLOWED_ERR: Raised if this node is readonly.
    if(isReadOnly())
        throw new DOMExceptionImpl(NO_MODIFICATION_ALLOWED_ERR);

    // WRONG_DOCUMENT_ERR: Raised when the specified node was created from
    // a different document than this document.
    if(n->ownerDocument() && n->ownerDocument() != this)
        throw new DOMExceptionImpl(WRONG_DOCUMENT_ERR);

    if(n->nodeType() == ATTRIBUTE_NODE)
    {
        AttrImpl *attr = static_cast<AttrImpl *>(n);
        ElementImpl *owner = attr->ownerElement();
        if(owner)
            owner->removeAttributeNode(attr);

        // a new node is created
        AttrImpl *newAttr = n->ownerDocument()->createAttributeNS(namespaceURI, qualifiedName);
        
        // move children to new node
        NodeImpl *child = attr->firstChild();
        while(child)
        {
            n->removeChild(child);
            newAttr->appendChild(child);
            child = attr->firstChild();
        }

        if(owner)
            owner->setAttributeNodeNS(newAttr);
            
        return newAttr;
    }
    else if(n->nodeType() == ELEMENT_NODE)
    {
        // a new node is created
        ElementImpl *newElem = n->ownerDocument()->createElementNS(namespaceURI, qualifiedName);
        
        // TODO : any user data attached to the old node is removed from that node
        // the old node is removed from its parent if it has one
        NodeImpl *parent = n->parentNode();
        NodeImpl *nextSib = n->nextSibling();
        if(parent)
            parent->removeChild(n);
            
        // move children to new node
        NodeImpl *child = n->firstChild();
        while(child)
        {
            n->removeChild(child);
            newElem->appendChild(child);
            child = n->firstChild();
        }
        
        // insert new node where old one was
        if(parent)
            parent->insertBefore(newElem, nextSib);
            
        return newElem;
    }
    else
    {
        // NOT_SUPPORTED_ERR: Raised when the type of the specified node is neither
        // ELEMENT_NODE nor ATTRIBUTE_NODE, or if the implementation does not support
        // the renaming of the document element.
        throw new DOMExceptionImpl(NOT_SUPPORTED_ERR);
    }

    return n;
}

NodeImpl *DocumentImpl::normalizeNode(NodeImpl *node)
{
    switch(node->nodeType())
    {
        case ELEMENT_NODE:
        {
            NodeImpl *next = 0;
            for(NodeImpl *child = node->firstChild();child != 0; child = next)
            {
                next = child->nextSibling();
                child = normalizeNode(child);
                if(child != 0)
                    next = child;
            }
            break;
        }
        case COMMENT_NODE:
        {
            if(!domConfig()->getParameter(FEATURE_COMMENTS))
            {
                NodeImpl *prevSibling = node->previousSibling();
                NodeImpl *parent = node->parentNode();
                
                // remove the comment node
                parent->removeChild(node);
                if(prevSibling != 0 && prevSibling->nodeType() == TEXT_NODE)
                {
                    NodeImpl *nextSibling = prevSibling->nextSibling();
                    if(nextSibling != 0 && nextSibling->nodeType() == TEXT_NODE)
                    {
                        static_cast<TextImpl *>(nextSibling)->insertData(0, prevSibling->nodeValue());
                        parent->removeChild(prevSibling);
                        return nextSibling;
                    }
                }
            }
            break;
        }
        case CDATA_SECTION_NODE:
        {
            if(!domConfig()->getParameter(FEATURE_CDATA_SECTIONS))
            {
                // convert CDATA to TEXT nodes
                TextImpl *text = createTextNode(node->nodeValue());
                NodeImpl *parent = node->parentNode();
                NodeImpl *prevSibling = node->previousSibling();

                node = parent->replaceChild(text, node);
                if(prevSibling != 0 && prevSibling->nodeType() == TEXT_NODE)
                {
                    text->insertData(0, prevSibling->nodeValue());
                    parent->removeChild(prevSibling);
                }
                
                return text; // Don't advance!
            }
            break;
        }
        case TEXT_NODE:
        {
            NodeImpl *next = node->nextSibling();
            if(next != 0 && next->nodeType() == TEXT_NODE)
            {
                static_cast<TextImpl *>(node)->appendData(next->nodeValue());
                node->parentNode()->removeChild(next);
                node->normalize();
                return node;
            }
            else if((!node->nodeValue() || node->nodeValue()->isEmpty()))
                node->parentNode()->removeChild(node);
            else
                node->normalize();

            break;
        }
        case DOCUMENT_TYPE_NODE:
        {
            if(domConfig()->getParameter(FEATURE_CANONICAL_FORM)) // remove the document type
                node->parentNode()->removeChild(node);
        }
    }

    return 0;
}

ElementImpl *DocumentImpl::getElementById(DOMStringImpl *elementId)
{
    if(elementId)
        elementId->ref();

    Q3PtrStack<NodeImpl> nodeStack;
    NodeImpl *current = firstChild();

    while(1)
    {
        if(!current)
        {
            if(nodeStack.isEmpty())
                break;

            current = nodeStack.pop();
            current = current->nextSibling();
        }
        else
        {
            if(current->nodeType() == ELEMENT_NODE)
            {
                ElementImpl *e = static_cast<ElementImpl *>(current);
                if(e->getIdAttribute(elementId))
                {
                    if(elementId)
                        elementId->deref();

                    return e;
                }
            }

            NodeImpl *child = current->firstChild();
            if(child)
            {
                nodeStack.push(current);
                current = child;
            }
            else
                current = current->nextSibling();
        }
    }

    if(elementId)
        elementId->deref();

    return 0;
}

void DocumentImpl::setDocumentKURI(const KURL &url)
{
    m_url = url;
}

KURL DocumentImpl::documentKURI() const
{
    return m_url;
}

bool DocumentImpl::xmlStandalone() const
{
    return m_xmlStandalone;
}

void DocumentImpl::setXmlStandalone(bool xmlStandalone)
{
    m_xmlStandalone = xmlStandalone;
}

DOMStringImpl *DocumentImpl::inputEncoding() const
{
    return m_inputEncoding;
}

void DocumentImpl::setInputEncoding(DOMStringImpl *inputEncoding)
{
    KDOM_SAFE_SET(m_inputEncoding, inputEncoding);
}

DOMStringImpl *DocumentImpl::xmlEncoding() const
{
    return m_xmlEncoding;
}

void DocumentImpl::setXmlEncoding(DOMStringImpl *encoding)
{
    KDOM_SAFE_SET(m_xmlEncoding, encoding);
}

NodeImpl *DocumentImpl::adoptNode(NodeImpl *source) const
{
    unsigned short nType = source->nodeType();
    
    // NOT_SUPPORTED_ERR: Raised if the source node is of type DOCUMENT, DOCUMENT_TYPE.
    if(nType == DOCUMENT_NODE || nType == DOCUMENT_TYPE_NODE)
        throw new DOMExceptionImpl(NOT_SUPPORTED_ERR);

    // NO_MODIFICATION_ALLOWED_ERR: Raised if this node is readonly.
    if(source->isReadOnly())
        throw new DOMExceptionImpl(NO_MODIFICATION_ALLOWED_ERR);

    if(nType == ENTITY_NODE || nType == NOTATION_NODE)
        return 0;

    if(source->parentNode())
        source->parentNode()->removeChild(source);
    else if(nType == ATTRIBUTE_NODE)
    {
        AttrImpl *attr = static_cast<AttrImpl *>(source);
        if(attr->ownerElement())
            attr->ownerElement()->removeAttributeNode(attr);
            
        attr->setSpecified(true);
    }

    // TODO: do setOwnerDocument recursively
    return source;
}

DOMStringImpl *DocumentImpl::xmlVersion() const
{
    return m_xmlVersion;
}

void DocumentImpl::setXmlVersion(DOMStringImpl *versionImpl)
{
    DOMString version(versionImpl);

    // NOT_SUPPORTED_ERR: Raised if the version is set to a value that is not
    // supported by this Document or if this document does not support the
    // "XML" feature.
    if(version != "1.0" && version != "1.1")
        throw new DOMExceptionImpl(NOT_SUPPORTED_ERR);

    KDOM_SAFE_SET(m_xmlVersion, versionImpl);
}

bool DocumentImpl::strictErrorChecking() const
{
    return m_strictErrorChecking;
}

void DocumentImpl::setStrictErrorChecking(bool strictErrorChecking)
{
    m_strictErrorChecking = strictErrorChecking;
}

DOMConfigurationImpl *DocumentImpl::domConfig() const
{
    if(!m_configuration)
        m_configuration = new DOMConfigurationImpl();

    return m_configuration;
}

void DocumentImpl::setPaintDevice(QPaintDevice *dev)
{
    if(m_paintDevice != dev)
    {
        m_paintDevice = dev;
        
        delete m_paintDeviceMetrics;
        m_paintDeviceMetrics = new Q3PaintDeviceMetrics(dev);
    }
}

void DocumentImpl::setUserStyleSheet(const QString &sheet)
{
    if(m_userSheet != sheet)
    {
        m_userSheet = sheet;
        updateStyleSelector();
    }
}

void DocumentImpl::setPrintStyleSheet(const QString &sheet)
{
    m_printSheet = sheet;
}

NodeImpl::Id DocumentImpl::getId(NodeImpl::IdType type, DOMStringImpl *namespaceURI, DOMStringImpl *prefix, DOMStringImpl *nameImpl, bool readonly) const
{
    DOMString nameDOMString(nameImpl);
    
    if(!nameImpl || !nameImpl->length())
        return 0;

    IdNameMapping *map;
    switch(type)
    {
        case NodeImpl::ElementId:
        {
            map = m_elementMap;
            break;
        }
        case NodeImpl::AttributeId:
        {
            map = m_attrMap;
            break;
        }
        case NodeImpl::NamespaceId:
        {
            // ### Id == 0 can't be used with ((void *) int) based QDicts...
            if(!strcasecmp(nameDOMString, DOMString(defaultNS())))
                return 0;
    
            map = m_namespaceMap;
            break;
        }
        default:
            return 0;
    }

    // Actually perform the request...
    CDFInterface *interface = implementation()->cdfInterface();

    NodeImpl::Id id, nsid = 0;
    
    QConstString n(nameDOMString.unicode(), nameDOMString.length());

    if(type != NodeImpl::NamespaceId)
    {
        if(nameImpl)
            nsid = getId(NamespaceId, 0, 0, namespaceURI, false) << 16;

        if(!nsid)
        {
            switch(type)
            {
                case NodeImpl::ElementId:
                {
                    if((id = interface->getTagID(n.string().ascii(), nameDOMString.length())))
                        return id;

                    // compatibility: upper case - case insensitive
                    if((id = interface->getTagID(n.string().lower().ascii(), nameDOMString.length())))
                        return id;
                
                    break;
                }
                default:
                {
                    if((id = interface->getAttrID(n.string().ascii(), nameDOMString.length())))
                        return id;

                    // compatibility: upper case - case insensitive
                    if((id = interface->getAttrID(n.string().lower().ascii(), nameDOMString.length())))
                        return id;
                }
            }
        }
    }

    // Look in the names array for the name
    // compatibility mode has to lookup upper case
    if(!nameImpl)
    {
        id = (NodeImpl::Id) map->ids.find(n.string());
        if(!id && type != NodeImpl::NamespaceId)
            id = (NodeImpl::Id) map->ids.find(QString::fromLatin1("aliases: ") + n.string());
    }
    else 
    {
        id = (NodeImpl::Id) map->ids.find(n.string());
        if(!readonly && id && prefix && !prefix->isEmpty())
        {
            // we were called in registration mode... check if the alias exists
            QConstString px(prefix->unicode(), prefix->length());
            QString qn(QString::fromLatin1("aliases: ") + px.string() + QString::fromLatin1(":") + n.string());
            if(!map->ids.find(qn))
                map->ids.insert(qn, (NodeImpl::Id *)id);
        }
    }

    if(id)
        return nsid + id;

    // unknown
    if(readonly)
        return 0;

    if(type != NodeImpl::NamespaceId && !Helper::ValidateQualifiedName(nameImpl))
        throw new DOMExceptionImpl(INVALID_CHARACTER_ERR);
    
    // Name not found, so let's add it
    NodeImpl::Id cid = map->count++ + map->idStart;
    map->names.insert(cid, nameImpl);
    nameImpl->ref();
    map->ids.insert(n.string(), (NodeImpl::Id *)cid);

    // and register an alias if needed for DOM1 methods compatibility
    if(prefix && !prefix->isEmpty())
    {
        QConstString px(prefix->unicode(), prefix->length());
        QString qn(QString::fromLatin1("aliases: ") + px.string() + QString::fromLatin1(":") + n.string());
        if(!map->ids.find(qn))
            map->ids.insert(qn, (NodeImpl::Id *)cid);
    }

#ifndef APPLE_CHANGES
    if(map->ids.size() == map->ids.count() && map->ids.size() != KDOM_CACHE_MAX_SEED)
        map->ids.resize(cacheNextSeed(map->ids.count()));
        
    if(map->names.size() == map->names.count() && map->names.size() != KDOM_CACHE_MAX_SEED)
        map->names.resize(cacheNextSeed(map->names.count()));
#endif

    return nsid + cid;
}

NodeImpl::Id DocumentImpl::getId(IdType type, DOMStringImpl *nodeName, bool readonly) const
{
    return getId(type, 0, 0, nodeName, readonly);
}

DOMStringImpl *DocumentImpl::getName(NodeImpl::IdType type, NodeImpl::Id id) const
{
    IdNameMapping *map;
    bool hasNS = (id & NodeImpl_IdNSMask);

    switch(type)
    {
        case NodeImpl::ElementId:
        {
            map = m_elementMap;
            break;
        }
        case NodeImpl::AttributeId:
        {
            map = m_attrMap;
            break;
        }
        case NodeImpl::NamespaceId:
        {
            if(!id)
                return defaultNS();
    
            map = m_namespaceMap;
            break;
        }
        default:
            return 0;
    }
    
    id = id & NodeImpl_IdLocalMask;

    if(id >= map->idStart && map->names[id])
        return new DOMStringImpl(map->names[id]->string());

    // Actually perform the request...
    CDFInterface *interface = implementation()->cdfInterface();

    const char *val = 0;
    switch(type)
    {
        case NodeImpl::ElementId:
        {
            val = interface->getTagName(id);
            break;
        }
        case NodeImpl::AttributeId:
        {
            val = interface->getAttrName(id);
            break;
        }
        case NodeImpl::NamespaceId:
        default:
            return 0;
    }

    QString ret = QString::fromLatin1(val, strlen(val));
    return new DOMStringImpl(hasNS ? ret.lower() : ret);
}

DOMStringImpl *DocumentImpl::defaultNS() const
{
    return new DOMStringImpl(NS_XHTML.string());
}

void DocumentImpl::updateRendering()
{
    if(!hasChangedChild())
        return;

    StyleChange change = NoChange;
    recalcStyle(change);
}

void DocumentImpl::updateStyleSelector()
{
    // Don't bother updating, since we haven't loaded all our style info yet.
    if(m_pendingStylesheets > 0)
        return;

    recalcStyleSelector();
    recalcStyle(Force);
}

void DocumentImpl::recalcStyleSelector()
{
    if(!m_render || !attached())
        return;

    assert(m_pendingStylesheets == 0);
}

// This method is called whenever a top-level stylesheet has finished loading.
void DocumentImpl::styleSheetLoaded()
{
    // Make sure we knew this sheet was pending, and that our count isn't out of sync.
    assert(m_pendingStylesheets > 0);

    m_pendingStylesheets--;
    updateStyleSelector();
}

bool DocumentImpl::haveStylesheetsLoaded() const
{
    return m_pendingStylesheets <= 0 || m_ignorePendingStylesheets;
}

KDOMPart *DocumentImpl::part() const
{
    return m_view ? m_view->part() : 0;
}

StyleSheetImpl *DocumentImpl::checkForStyleSheet(NodeImpl *, QString &)
{
    // no stylesheet elements in xml
    return 0;
}

CSSStyleSelector *DocumentImpl::createStyleSelector(const QString &userSheet)
{
    return new CSSStyleSelector(this, userSheet, m_styleSheets, m_url, false);
}

CSSStyleSheetImpl *DocumentImpl::createCSSStyleSheet(NodeImpl *parent, DOMStringImpl *url) const
{
    CSSStyleSheetImpl *sheet = new CSSStyleSheetImpl(parent, url);
    sheet->ref();
    return sheet;
}

CSSStyleSheetImpl *DocumentImpl::createCSSStyleSheet(CSSRuleImpl *ownerRule, DOMStringImpl *url) const
{
    CSSStyleSheetImpl *sheet = new CSSStyleSheetImpl(ownerRule, url);
    sheet->ref();
    return sheet;
}

bool DocumentImpl::prepareMouseEvent(bool, int, int, MouseEventImpl *)
{
    return false;
}

DOMStringImpl *DocumentImpl::documentURI() const
{
    if(documentKURI().isEmpty())
        return 0;

    return new DOMStringImpl(documentKURI().url());
}

void DocumentImpl::setDocumentURI(DOMStringImpl *documentURI)
{
    // TODO: check if that is enough.
    m_url = KURL(DOMString(documentURI).string());
}

void DocumentImpl::setHoverNode(NodeImpl *newHoverNode)
{
    KDOM_SAFE_SET(m_hoverNode, newHoverNode);
}

void DocumentImpl::setFocusNode(NodeImpl *newFocusNode)
{
    // don't process focus changes while detaching
    if(!m_render) return;

    // Make sure newFocusNode is actually in this document
    if(newFocusNode && (newFocusNode->ownerDocument() != this))
        return;

    if(m_focusNode == newFocusNode)
        return;

    NodeImpl *oldFocusNode = m_focusNode;
    // Set focus on the new node
    m_focusNode = newFocusNode;
    // Remove focus from the existing focus node (if any)
    if(oldFocusNode)
    {
        if(oldFocusNode->active())
            oldFocusNode->setActive(false);

        oldFocusNode->setFocus(false);
#if 0
        oldFocusNode->dispatchHTMLEvent(BLUR_EVENT,false,false);
        oldFocusNode->dispatchUIEvent(DOMFOCUSOUT_EVENT);
#endif
        if((oldFocusNode == this))// && oldFocusNode->hasOneRef())
        {
            oldFocusNode->deref(); // deletes this
            return;
        }
        else
            oldFocusNode->deref();
    }

    if(m_focusNode)
    {
        m_focusNode->ref();
#if 0
        m_focusNode->dispatchHTMLEvent(FOCUS_EVENT,false,false);
#endif
        if(m_focusNode != newFocusNode) return;
#if 0
        m_focusNode->dispatchUIEvent(DOMFOCUSIN_EVENT);
#endif
        if(m_focusNode != newFocusNode) return;
        m_focusNode->setFocus();
        if(m_focusNode != newFocusNode) return;

#if 0
        // eww, I suck. set the qt focus correctly
        // ### find a better place in the code for this
        if(view())
        {
            if(!m_focusNode->renderer() || !m_focusNode->renderer()->isWidget())
                view()->setFocus();
            else if(static_cast<RenderWidget*>(m_focusNode->renderer())->widget())
            {
                if(view()->isVisible())
                    static_cast<RenderWidget*>(m_focusNode->renderer())->widget()->setFocus();
            }
        }
#endif
    }

    updateRendering();
}

void DocumentImpl::setCSSTarget(NodeImpl *n)
{
    if(m_cssTarget)
        m_cssTarget->setChanged();

    m_cssTarget = n;
    if(n)
        n->setChanged();
}

KDOMDocumentType DocumentImpl::kdomDocumentType() const
{
    return m_kdomDocType;
}

void DocumentImpl::attachNodeIterator(NodeIteratorImpl *ni)
{
    m_nodeIterators.append(ni);
}

void DocumentImpl::detachNodeIterator(NodeIteratorImpl *ni)
{
    m_nodeIterators.remove(ni);
}

// vim:ts=4:noet
