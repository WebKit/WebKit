/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Peter Kelly (pmk@post.com)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

//#define EVENT_DEBUG
#include "dom/dom_exception.h"
#include "dom/dom_node.h"
#include "xml/dom_textimpl.h"
#include "xml/dom_docimpl.h"
#include "xml/dom2_eventsimpl.h"
#include "xml/dom_elementimpl.h"

#include "html/dtd.h"
#include "html/htmlparser.h"

#include "rendering/render_root.h"
#include "misc/htmlhashes.h"
#include "css/css_valueimpl.h"
#include "css/css_stylesheetimpl.h"
#include "css/cssstyleselector.h"
#include "xml/dom_xmlimpl.h"

#include <qtextstream.h>
#include <kdebug.h>

using namespace DOM;
using namespace khtml;

void AttributeImpl::allocateImpl(ElementImpl* e) {
    _impl = new AttrImpl(e, e->docPtr(), this);
    _impl->ref();
}

AttrImpl::AttrImpl(ElementImpl* element, DocumentPtr* docPtr, AttributeImpl* a)
    : NodeBaseImpl(docPtr),
      m_element(element),
      m_attribute(a)
{
    assert(!m_attribute->_impl);
    m_attribute->_impl = this;
    m_attribute->ref();
}

AttrImpl::~AttrImpl()
{
    assert(m_attribute->_impl == this);
    m_attribute->_impl = 0;
    m_attribute->deref();
}

DOMString AttrImpl::nodeName() const
{
    return getDocument()->attrName(m_attribute->id());
}

unsigned short AttrImpl::nodeType() const
{
    return Node::ATTRIBUTE_NODE;
}

DOMString AttrImpl::prefix() const
{
    return m_attribute->prefix();
}

void AttrImpl::setPrefix(const DOMString &_prefix, int &exceptioncode )
{
    checkSetPrefix(_prefix, exceptioncode);
    if (exceptioncode)
        return;

    m_attribute->setPrefix(_prefix.implementation());
}

DOMString AttrImpl::nodeValue() const {
    return m_attribute->val();
}

void AttrImpl::setValue( const DOMString &v, int &exceptioncode )
{
    exceptioncode = 0;

    // ### according to the DOM docs, we should create an unparsed Text child
    // node here
    // do not interprete entities in the string, its literal!

    // NO_MODIFICATION_ALLOWED_ERR: Raised when the node is readonly
    if (isReadOnly()) {
        exceptioncode = DOMException::NO_MODIFICATION_ALLOWED_ERR;
        return;
    }

    // ### what to do on 0 ?
    if (v.isNull()) {
        exceptioncode = DOMException::DOMSTRING_SIZE_ERR;
        return;
    }

    m_attribute->setValue(v.implementation());
    if (m_element)
        m_element->parseAttribute(m_attribute);
}

void AttrImpl::setNodeValue( const DOMString &v, int &exceptioncode )
{
    exceptioncode = 0;
    // NO_MODIFICATION_ALLOWED_ERR: taken care of by setValue()
    setValue(v, exceptioncode);
}

NodeImpl *AttrImpl::cloneNode ( bool /*deep*/)
{
	return new AttrImpl(0, docPtr(), new AttributeImpl(m_attribute->id(), m_attribute->val()));
}

// DOM Section 1.1.1
bool AttrImpl::childAllowed( NodeImpl *newChild )
{
    if(!newChild)
        return false;

    return childTypeAllowed(newChild->nodeType());
}

bool AttrImpl::childTypeAllowed( unsigned short type )
{
    switch (type) {
        case Node::TEXT_NODE:
        case Node::ENTITY_REFERENCE_NODE:
            return true;
            break;
        default:
            return false;
    }
}

// -------------------------------------------------------------------------

ElementImpl::ElementImpl(DocumentPtr *doc)
    : NodeBaseImpl(doc)
{
    namedAttrMap = 0;
    m_styleDecls = 0;
    m_prefix = 0;
}

ElementImpl::~ElementImpl()
{
    if(namedAttrMap) {
        namedAttrMap->detachFromElement();
        namedAttrMap->deref();
    }

    if (m_styleDecls) {
        m_styleDecls->setNode(0);
        m_styleDecls->setParent(0);
        m_styleDecls->deref();
    }

    if (m_prefix)
        m_prefix->deref();
}

void ElementImpl::removeAttribute( NodeImpl::Id id, int &exceptioncode )
{
    if (namedAttrMap)
        namedAttrMap->removeNamedItem(id, exceptioncode);
}

void ElementImpl::setAttribute(NodeImpl::Id id, const DOMString &value)
{
    int exceptioncode = 0;
    setAttribute(id,value.implementation(),exceptioncode);
}

unsigned short ElementImpl::nodeType() const
{
    return Node::ELEMENT_NODE;
}

DOMString ElementImpl::getAttribute(NodeImpl::Id id) const
{
    if (!namedAttrMap) return DOMString();
    AttributeImpl* a = namedAttrMap->getAttributeItem(id);
    if (a) return a->val();

    // then search in default attr in case it is not yet set
    NamedAttrMapImpl* dm = defaultMap();
    if(!dm) return DOMString();
    AttributeImpl* defattr = dm->getAttributeItem(id);
    if (!defattr) return DOMString();
    return defattr->val();
}

void ElementImpl::setAttribute(NodeImpl::Id id, DOMStringImpl* value, int &exceptioncode )
{
    // allocate attributemap if necessary
    AttributeImpl* old = attributes(false)->getAttributeItem(id);

    // NO_MODIFICATION_ALLOWED_ERR: Raised when the node is readonly
    if (namedAttrMap->isReadOnly()) {
        exceptioncode = DOMException::NO_MODIFICATION_ALLOWED_ERR;
        return;
    }

    if (old && !value)
        namedAttrMap->removeAttribute(id);
    else if (!old && value)
        namedAttrMap->addAttribute(new AttributeImpl(id, value));
    else if (old && value) {
        old->setValue(value);
        parseAttribute(old);
    }
}

void ElementImpl::setAttributeMap( NamedAttrMapImpl* list )
{
    if(namedAttrMap)
        namedAttrMap->deref();

    namedAttrMap = list;

    if(namedAttrMap) {
        namedAttrMap->ref();
        namedAttrMap->element = this;
        unsigned int len = namedAttrMap->length();
        for(unsigned int i = 0; i < len; i++)
            parseAttribute(namedAttrMap->attrs[i]);
    }
}

NodeImpl *ElementImpl::cloneNode(bool deep)
{
    // ### we loose the namespace here ... FIXME
    ElementImpl *clone = getDocument()->createElement(tagName());
    if (!clone) return 0;

    // clone attributes
    if(namedAttrMap)
        *(static_cast<NamedAttrMapImpl*>(clone->attributes())) = *namedAttrMap;

    // clone individual style rules
    if (m_styleDecls)
        *(clone->styleRules()) = *m_styleDecls;

    if (deep)
        cloneChildNodes(clone);
    return clone;
}

DOMString ElementImpl::nodeName() const
{
    return tagName();
}

DOMString ElementImpl::tagName() const
{
    DOMString tn = getDocument()->tagName(id());

    if (m_prefix)
        return DOMString(m_prefix) + ":" + tn;

    return tn;
}

void ElementImpl::setPrefix( const DOMString &_prefix, int &exceptioncode )
{
    checkSetPrefix(_prefix, exceptioncode);
    if (exceptioncode)
        return;

    if (m_prefix)
        m_prefix->deref();
    m_prefix = _prefix.implementation();
    if (m_prefix)
        m_prefix->ref();
}

void ElementImpl::createAttributeMap() const
{
    namedAttrMap = new NamedAttrMapImpl(const_cast<ElementImpl*>(this));
    namedAttrMap->ref();
}

NamedAttrMapImpl* ElementImpl::defaultMap() const
{
    return 0;
}

void ElementImpl::attach()
{
    assert(!attached());
    assert(!m_render);
    assert(parentNode());

#if SPEED_DEBUG < 1
    if (parentNode()->renderer()) {
        RenderStyle* _style = getDocument()->styleSelector()->styleForElement(this);
        _style->ref();
        m_render = RenderObject::createObject(this, _style);
        if(m_render)
            parentNode()->renderer()->addChild(m_render, nextRenderer());
        _style->deref();
    }
#endif

    NodeBaseImpl::attach();
}

void ElementImpl::recalcStyle( StyleChange change )
{
    // ### should go away and be done in renderobject
    RenderStyle* _style = m_render ? m_render->style() : 0;
#if 0
    const char* debug;
    switch(change) {
    case NoChange: debug = "NoChange";
        break;
    case NoInherit: debug= "NoInherit";
        break;
    case Inherit: debug = "Inherit";
        break;
    case Force: debug = "Force";
        break;
    }
    qDebug("recalcStyle(%d: %s)[%p: %s]", change, debug, this, tagName().string().latin1());
#endif
    if ( change >= Inherit || changed() ) {
        EDisplay oldDisplay = _style ? _style->display() : NONE;

        int dynamicState = StyleSelector::None;
        if ( m_render && m_render->mouseInside() )
            dynamicState |= StyleSelector::Hover;
        if ( m_focused )
            dynamicState |= StyleSelector::Focus;
        if ( m_active )
            dynamicState |= StyleSelector::Active;

        RenderStyle *newStyle = getDocument()->styleSelector()->styleForElement(this, dynamicState);
        newStyle->ref();
        StyleChange ch = diff( _style, newStyle );
        if ( ch != NoChange ) {
            if (oldDisplay != newStyle->display()) {
                if (attached()) detach();
                // ### uuhm, suboptimal. style gets calculated again
                attach();
		// attach recalulates the style for all children. No need to do it twice.
		setChanged( false );
		setHasChangedChild( false );
		newStyle->deref();
		return;
            }
            if( m_render && newStyle ) {
                //qDebug("--> setting style on render element bgcolor=%s", newStyle->backgroundColor().name().latin1());
                m_render->setStyle(newStyle);
            }
        }
        newStyle->deref();

        if ( change != Force )
            change = ch;
    }

    NodeImpl *n;
    for (n = _first; n; n = n->nextSibling()) {
	//qDebug("    (%p) calling recalcStyle on child %s/%p, change=%d", this, n, n->isElementNode() ? ((ElementImpl *)n)->tagName().string().latin1() : n->isTextNode() ? "text" : "unknown", change );
        if ( change >= Inherit || n->isTextNode() ||
             n->hasChangedChild() || n->changed() )
            n->recalcStyle( change );
    }

    setChanged( false );
    setHasChangedChild( false );
}

bool ElementImpl::isSelectable() const
{
    return false;
}

// DOM Section 1.1.1
bool ElementImpl::childAllowed( NodeImpl *newChild )
{
    if (!childTypeAllowed(newChild->nodeType()))
        return false;

    // ### check xml element allowedness according to DTD

    // If either this node or the other node is an XML element node, allow regardless (we don't do DTD checks for XML
    // yet)
    if (isXMLElementNode() || newChild->isXMLElementNode())
	return true;
    else
	return checkChild(id(), newChild->id());
}

bool ElementImpl::childTypeAllowed( unsigned short type )
{
    switch (type) {
        case Node::ELEMENT_NODE:
        case Node::TEXT_NODE:
        case Node::COMMENT_NODE:
        case Node::PROCESSING_INSTRUCTION_NODE:
        case Node::CDATA_SECTION_NODE:
        case Node::ENTITY_REFERENCE_NODE:
            return true;
            break;
        default:
            return false;
    }
}

void ElementImpl::createDecl( )
{
    m_styleDecls = new CSSStyleDeclarationImpl(0);
    m_styleDecls->ref();
    m_styleDecls->setParent(getDocument()->elementSheet());
    m_styleDecls->setNode(this);
    m_styleDecls->setStrictParsing( getDocument()->parseMode() == DocumentImpl::Strict );
}

void ElementImpl::dispatchAttrRemovalEvent(AttributeImpl *attr)
{
    if (!getDocument()->hasListenerType(DocumentImpl::DOMATTRMODIFIED_LISTENER))
	return;
    //int exceptioncode = 0;
//     dispatchEvent(new MutationEventImpl(EventImpl::DOMATTRMODIFIED_EVENT,true,false,attr,attr->value(),
// 		  attr->value(), getDocument()->attrName(attr->id()),MutationEvent::REMOVAL),exceptioncode);
}

void ElementImpl::dispatchAttrAdditionEvent(AttributeImpl *attr)
{
    if (!getDocument()->hasListenerType(DocumentImpl::DOMATTRMODIFIED_LISTENER))
	return;
   // int exceptioncode = 0;
//     dispatchEvent(new MutationEventImpl(EventImpl::DOMATTRMODIFIED_EVENT,true,false,attr,attr->value(),
//                                         attr->value(),getDocument()->attrName(attr->id()),MutationEvent::ADDITION),exceptioncode);
}

#ifndef NDEBUG
void ElementImpl::dump(QTextStream *stream, QString ind) const
{
    if (namedAttrMap) {
        for (uint i = 0; i < namedAttrMap->length(); i++) {
            AttributeImpl *attr = namedAttrMap->attributeItem(i);
            *stream << " " << DOMString(getDocument()->attrName(attr->id())).string().ascii()
                    << "=\"" << DOMString(attr->value()).string().ascii() << "\"";
        }
    }

    NodeBaseImpl::dump(stream,ind);
}
#endif

// -------------------------------------------------------------------------

XMLElementImpl::XMLElementImpl(DocumentPtr *doc, DOMStringImpl *_tagName)
    : ElementImpl(doc)
{
    m_id = doc->document()->tagId(0 /* no namespace */, _tagName,  false /* allocate */);
}

XMLElementImpl::XMLElementImpl(DocumentPtr *doc, DOMStringImpl *_qualifiedName, DOMStringImpl *_namespaceURI)
    : ElementImpl(doc)
{
    int colonpos = -1;
    for (uint i = 0; i < _qualifiedName->l; ++i)
        if (_qualifiedName->s[i] == ':') {
            colonpos = i;
            break;
        }

    if (colonpos >= 0) {
        // we have a prefix
        DOMStringImpl* localName = _qualifiedName->copy();
        localName->ref();
        localName->remove(0,colonpos+1);
        m_id = doc->document()->tagId(_namespaceURI, localName, false /* allocate */);
        localName->deref();
        m_prefix = _qualifiedName->copy();
        m_prefix->ref();
        m_prefix->truncate(colonpos);
    }
    else {
        // no prefix
        m_id = doc->document()->tagId(_namespaceURI, _qualifiedName, false /* allocate */);
        m_prefix = 0;
    }
}

XMLElementImpl::~XMLElementImpl()
{
}

DOMString XMLElementImpl::localName() const
{
    return getDocument()->tagName(m_id);
}


NodeImpl *XMLElementImpl::cloneNode ( bool deep )
{
    // ### we loose namespace here FIXME
    // should pass id around
    XMLElementImpl *clone = new XMLElementImpl(docPtr(), getDocument()->tagName(m_id).implementation());
    clone->m_id = m_id;

    // clone attributes
    if(namedAttrMap)
        *(static_cast<NamedAttrMapImpl*>(clone->attributes())) = *namedAttrMap;

    // clone individual style rules
    if (m_styleDecls)
        *(clone->styleRules()) = *m_styleDecls;

    if (deep)
        cloneChildNodes(clone);

    return clone;
}

// -------------------------------------------------------------------------

NamedAttrMapImpl::NamedAttrMapImpl(ElementImpl *e)
    : element(e)
{
    attrs = 0;
    len = 0;
}

NamedAttrMapImpl::~NamedAttrMapImpl()
{
    clearAttributes();
}


AttrImpl *NamedAttrMapImpl::getNamedItem ( NodeImpl::Id id ) const
{
    AttributeImpl* a = getAttributeItem(id);
    if (!a) return 0;

    if (!a->attrImpl())
        a->allocateImpl(element);

    return a->attrImpl();
}

Node NamedAttrMapImpl::setNamedItem ( NodeImpl* arg, int &exceptioncode )
{
    if (!element) {
        exceptioncode = DOMException::NOT_FOUND_ERR;
        return 0;
    }

    // NO_MODIFICATION_ALLOWED_ERR: Raised if this map is readonly.
    if (isReadOnly()) {
        exceptioncode = DOMException::NO_MODIFICATION_ALLOWED_ERR;
        return 0;
    }

    // WRONG_DOCUMENT_ERR: Raised if arg was created from a different document than the one that created this map.
    if (arg->getDocument() != element->getDocument()) {
        exceptioncode = DOMException::WRONG_DOCUMENT_ERR;
        return 0;
    }

    // Not mentioned in spec: throw a HIERARCHY_REQUEST_ERROR if the user passes in a non-attribute node
    if (!arg->isAttributeNode()) {
        exceptioncode = DOMException::HIERARCHY_REQUEST_ERR;
        return 0;
    }
    AttrImpl *attr = static_cast<AttrImpl*>(arg);

    // INUSE_ATTRIBUTE_ERR: Raised if arg is an Attr that is already an attribute of another Element object.
    // The DOM user must explicitly clone Attr nodes to re-use them in other elements.
    if (attr->ownerElement()) {
        exceptioncode = DOMException::INUSE_ATTRIBUTE_ERR;
        return 0;
    }

    AttributeImpl* a = attr->attrImpl();
    AttributeImpl* old = getAttributeItem(a->id());
    if (old == a) return arg; // we know about it already

    // ### slightly inefficient - resizes attribute array twice.
    Node r;
    if (old) {
        if (!old->attrImpl())
            old->allocateImpl(element);
        r = old->_impl;
        removeAttribute(a->id());
    }
    addAttribute(a);
    return r;
}

// The DOM2 spec doesn't say that removeAttribute[NS] throws NOT_FOUND_ERR
// if the attribute is not found - David
Node NamedAttrMapImpl::removeNamedItem ( NodeImpl::Id id, int &exceptioncode )
{
    // ### should this really be raised when the attribute to remove isn't there at all?
    // NO_MODIFICATION_ALLOWED_ERR: Raised when the node is readonly
    if (isReadOnly()) {
        exceptioncode = DOMException::NO_MODIFICATION_ALLOWED_ERR;
        return Node();
    }

    AttributeImpl* a = getAttributeItem(id);
    if (!a) return Node();

    if (!a->attrImpl())  a->allocateImpl(element);
    Node r(a->attrImpl());
    removeAttribute(id);
    return r;
}

AttrImpl *NamedAttrMapImpl::item ( unsigned long index ) const
{
    if (index >= len)
        return 0;

    if (!attrs[index]->attrImpl())
        attrs[index]->allocateImpl(element);

    return attrs[index]->attrImpl();
}

unsigned long NamedAttrMapImpl::length(  ) const
{
    return len;
}

AttributeImpl* NamedAttrMapImpl::getAttributeItem(NodeImpl::Id id) const
{
    for (unsigned long i = 0; i < len; ++i)
        if (attrs[i]->id() == id)
            return attrs[i];

    return 0;
}

NodeImpl::Id NamedAttrMapImpl::mapId(const DOMString& namespaceURI,
                                     const DOMString& localName, bool readonly)
{
    assert(element);
    if (!element) return 0;
    return element->getDocument()->attrId(namespaceURI.implementation(),
                                            localName.implementation(), readonly);
}

void NamedAttrMapImpl::clearAttributes()
{
    if (attrs) {
        uint i;
        for (i = 0; i < len; i++) {
            if (attrs[i]->_impl)
                attrs[i]->_impl->m_element = 0;
            attrs[i]->deref();
        }
        delete [] attrs;
        attrs = 0;
    }
    len = 0;
}

void NamedAttrMapImpl::detachFromElement()
{
    // we allow a NamedAttrMapImpl w/o an element in case someone still has a reference
    // to if after the element gets deleted - but the map is now invalid
    element = 0;
    clearAttributes();
}

NamedAttrMapImpl& NamedAttrMapImpl::operator=(const NamedAttrMapImpl& other)
{
    // clone all attributes in the other map, but attach to our element
    if (!element) return *this;

    clearAttributes();
    len = other.len;
    attrs = new AttributeImpl* [len];

    // first initialize attrs vector, then call parseAttribute on it
    // this allows parseAttribute to use getAttribute
    for (uint i = 0; i < len; i++) {
        attrs[i] = new AttributeImpl(other.attrs[i]->id(), other.attrs[i]->val());
        attrs[i]->ref();
    }

    for(uint i = 0; i < len; i++)
        element->parseAttribute(attrs[i]);

    return *this;
}

void NamedAttrMapImpl::addAttribute(AttributeImpl *attr)
{
    // Add the attribute tot he list
    AttributeImpl **newAttrs = new AttributeImpl* [len+1];
    if (attrs) {
      for (uint i = 0; i < len; i++)
        newAttrs[i] = attrs[i];
      delete [] attrs;
    }
    attrs = newAttrs;
    attrs[len++] = attr;
    attr->ref();

    // Notify the element that the attribute has been added, and dispatch appropriate mutation events
    // Note that element may be null here if we are called from insertAttr() during parsing
    if (element) {
        element->parseAttribute(attr);
        element->dispatchAttrAdditionEvent(attr);
        element->dispatchSubtreeModifiedEvent();
    }
}

void NamedAttrMapImpl::removeAttribute(NodeImpl::Id id)
{
    unsigned long index = len+1;
    for (unsigned long i = 0; i < len; ++i)
        if (attrs[i]->id() == id) {
            index = i;
            break;
        }

    if (index >= len) return;

    // Remove the attribute from the list
    AttributeImpl* attr = attrs[index];
    if (attrs[index]->_impl)
        attrs[index]->_impl->m_element = 0;
    if (len == 1) {
        delete [] attrs;
        attrs = 0;
        len = 0;
    }
    else {
        AttributeImpl **newAttrs = new AttributeImpl* [len-1];
        uint i;
        for (i = 0; i < uint(index); i++)
            newAttrs[i] = attrs[i];
        len--;
        for (; i < len; i++)
            newAttrs[i] = attrs[i+1];
        delete [] attrs;
        attrs = newAttrs;
    }

    // Notify the element that the attribute has been removed
    // dispatch appropriate mutation events
    if (attr->_value) {
        attr->_value->deref();
        attr->_value = 0;
        if (element)
            element->parseAttribute(attr);
    }
    if (element) {
        element->dispatchAttrRemovalEvent(attr);
        element->dispatchSubtreeModifiedEvent();
    }
    attr->deref();
}

