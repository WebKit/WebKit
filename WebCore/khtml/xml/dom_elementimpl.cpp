/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Peter Kelly (pmk@post.com)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004 Apple Computer, Inc.
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

#include "khtml_part.h"

#include "html/dtd.h"
#include "html/htmlparser.h"

#include "rendering/render_canvas.h"
#include "misc/htmlhashes.h"
#include "css/css_valueimpl.h"
#include "css/css_stylesheetimpl.h"
#include "css/cssstyleselector.h"
#include "xml/dom_xmlimpl.h"

#include <qtextstream.h>
#include <kdebug.h>

using namespace DOM;
using namespace khtml;

AttributeImpl* AttributeImpl::clone(bool) const
{
    AttributeImpl* result = new AttributeImpl(m_id, _value);
    result->setPrefix(_prefix);
    return result;
}

void AttributeImpl::allocateImpl(ElementImpl* e) {
    _impl = new AttrImpl(e, e->docPtr(), this);
}

AttrImpl::AttrImpl(ElementImpl* element, DocumentPtr* docPtr, AttributeImpl* a)
    : ContainerNodeImpl(docPtr),
      m_element(element),
      m_attribute(a)
{
    assert(!m_attribute->_impl);
    m_attribute->_impl = this;
    m_attribute->ref();
    m_specified = true;
}

AttrImpl::~AttrImpl()
{
    assert(m_attribute->_impl == this);
    m_attribute->_impl = 0;
    m_attribute->deref();
}

DOMString AttrImpl::nodeName() const
{
    return name();
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

DOMString AttrImpl::nodeValue() const
{
    return value();
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
        m_element->attributeChanged(m_attribute);
}

void AttrImpl::setNodeValue( const DOMString &v, int &exceptioncode )
{
    exceptioncode = 0;
    // NO_MODIFICATION_ALLOWED_ERR: taken care of by setValue()
    setValue(v, exceptioncode);
}

NodeImpl *AttrImpl::cloneNode ( bool /*deep*/)
{
    return new AttrImpl(0, docPtr(), m_attribute->clone());
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

DOMString AttrImpl::toString() const
{
    DOMString result;

    result += nodeName();

    // FIXME: substitute entities for any instances of " or ' --
    // maybe easier to just use text value and ignore existing
    // entity refs?

    if (firstChild() != NULL) {
	result += "=\"";

	for (NodeImpl *child = firstChild(); child != NULL; child = child->nextSibling()) {
	    result += child->toString();
	}
	
	result += "\"";
    }

    return result;
}

DOMString AttrImpl::name() const
{
    return getDocument()->attrName(m_attribute->id());
}

DOMString AttrImpl::value() const
{
    return m_attribute->value();
}

// -------------------------------------------------------------------------

ElementImpl::ElementImpl(DocumentPtr *doc)
    : ContainerNodeImpl(doc)
{
    namedAttrMap = 0;
    m_prefix = 0;
}

ElementImpl::~ElementImpl()
{
    if (namedAttrMap) {
        namedAttrMap->detachFromElement();
        namedAttrMap->deref();
    }

    if (m_prefix)
        m_prefix->deref();
}

void ElementImpl::removeAttribute( NodeImpl::Id id, int &exceptioncode )
{
    if (namedAttrMap) {
        namedAttrMap->removeNamedItem(id, exceptioncode);
        if (exceptioncode == DOMException::NOT_FOUND_ERR) {
            exceptioncode = 0;
        }
    }
}

void ElementImpl::setAttribute(NodeImpl::Id id, const DOMString &value)
{
    int exceptioncode = 0;
    setAttribute(id,value.implementation(),exceptioncode);
}

// Virtual function, defined in base class.
NamedAttrMapImpl *ElementImpl::attributes() const
{
    return attributes(false);
}

NamedAttrMapImpl* ElementImpl::attributes(bool readonly) const
{
    updateStyleAttributeIfNeeded();
    if (!readonly && !namedAttrMap)
        createAttributeMap();
    return namedAttrMap;
}

unsigned short ElementImpl::nodeType() const
{
    return Node::ELEMENT_NODE;
}

const AtomicStringList* ElementImpl::getClassList() const
{
    return 0;
}

const AtomicString& ElementImpl::getIDAttribute() const
{
    return namedAttrMap ? namedAttrMap->id() : nullAtom;
}

const AtomicString& ElementImpl::getAttribute(NodeImpl::Id id) const
{
    if (id == ATTR_STYLE)
        updateStyleAttributeIfNeeded();

    if (namedAttrMap) {
        AttributeImpl* a = namedAttrMap->getAttributeItem(id);
        if (a) return a->value();
    }
    return nullAtom;
}

const AtomicString& ElementImpl::getAttributeNS(const DOMString &namespaceURI,
                                                const DOMString &localName) const
{   
    NodeImpl::Id id = getDocument()->attrId(namespaceURI.implementation(),
                                            localName.implementation(), true);
    if (!id) return nullAtom;
    return getAttribute(id);
}

void ElementImpl::setAttribute(NodeImpl::Id id, DOMStringImpl* value, int &exceptioncode )
{
    if (inDocument())
        getDocument()->incDOMTreeVersion();

    // allocate attributemap if necessary
    AttributeImpl* old = attributes(false)->getAttributeItem(id);

    // NO_MODIFICATION_ALLOWED_ERR: Raised when the node is readonly
    if (namedAttrMap->isReadOnly()) {
        exceptioncode = DOMException::NO_MODIFICATION_ALLOWED_ERR;
        return;
    }

    if (id == ATTR_ID) {
	updateId(old ? old->value() : nullAtom, value);
    }
    
    if (old && !value)
        namedAttrMap->removeAttribute(id);
    else if (!old && value)
        namedAttrMap->addAttribute(createAttribute(id, value));
    else if (old && value) {
        old->setValue(value);
        attributeChanged(old);
    }
}

AttributeImpl* ElementImpl::createAttribute(NodeImpl::Id id, DOMStringImpl* value)
{
    return new AttributeImpl(id, value);
}

void ElementImpl::setAttributeMap( NamedAttrMapImpl* list )
{
    if (inDocument())
        getDocument()->incDOMTreeVersion();

    // If setting the whole map changes the id attribute, we need to
    // call updateId.

    AttributeImpl *oldId = namedAttrMap ? namedAttrMap->getAttributeItem(ATTR_ID) : 0;
    AttributeImpl *newId = list ? list->getAttributeItem(ATTR_ID) : 0;

    if (oldId || newId) {
	updateId(oldId ? oldId->value() : nullAtom, newId ? newId->value() : nullAtom);
    }

    if(namedAttrMap)
        namedAttrMap->deref();

    namedAttrMap = list;

    if(namedAttrMap) {
        namedAttrMap->ref();
        namedAttrMap->element = this;
        unsigned int len = namedAttrMap->length();
        for(unsigned int i = 0; i < len; i++)
            attributeChanged(namedAttrMap->attrs[i]);
    }
}

bool ElementImpl::hasAttributes() const
{
    updateStyleAttributeIfNeeded();

    return namedAttrMap && namedAttrMap->length() > 0;
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

bool ElementImpl::isURLAttribute(AttributeImpl *attr) const
{
    return false;
}

RenderStyle *ElementImpl::styleForRenderer(RenderObject *parentRenderer)
{
    return getDocument()->styleSelector()->styleForElement(this);
}

RenderObject *ElementImpl::createRenderer(RenderArena *arena, RenderStyle *style)
{
    if (getDocument()->documentElement() == this && style->display() == NONE) {
        // Ignore display: none on root elements.  Force a display of block in that case.
        RenderBlock* result = new (arena) RenderBlock(this);
        if (result) result->setStyle(style);
        return result;
    }
    return RenderObject::createObject(this, style);
}


void ElementImpl::insertedIntoDocument()
{
    // need to do superclass processing first so inDocument() is true
    // by the time we reach updateId
    ContainerNodeImpl::insertedIntoDocument();

    if (hasID()) {
        NamedAttrMapImpl *attrs = attributes(true);
        if (attrs) {
            AttributeImpl *idAttr = attrs->getAttributeItem(ATTR_ID);
            if (idAttr && !idAttr->isNull()) {
                updateId(nullAtom, idAttr->value());
            }
        }
    }
}

void ElementImpl::removedFromDocument()
{
    if (hasID()) {
        NamedAttrMapImpl *attrs = attributes(true);
        if (attrs) {
            AttributeImpl *idAttr = attrs->getAttributeItem(ATTR_ID);
            if (idAttr && !idAttr->isNull()) {
                updateId(idAttr->value(), nullAtom);
            }
        }
    }

    ContainerNodeImpl::removedFromDocument();
}

void ElementImpl::attach()
{
#if SPEED_DEBUG < 1
    createRendererIfNeeded();
#endif
    ContainerNodeImpl::attach();
}

void ElementImpl::recalcStyle( StyleChange change )
{
    // ### should go away and be done in renderobject
    RenderStyle* _style = m_render ? m_render->style() : 0;
    bool hasParentRenderer = parent() ? parent()->renderer() : false;
    
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
    if ( hasParentRenderer && (change >= Inherit || changed()) ) {
        RenderStyle *newStyle = getDocument()->styleSelector()->styleForElement(this);
        newStyle->ref();
        StyleChange ch = diff( _style, newStyle );
        if (ch == Detach) {
            if (attached()) detach();
            // ### Suboptimal. Style gets calculated again.
            attach();
            // attach recalulates the style for all children. No need to do it twice.
            setChanged( false );
            setHasChangedChild( false );
            newStyle->deref(getDocument()->renderArena());
            return;
        }
        else if (ch != NoChange) {
            if( m_render && newStyle ) {
                //qDebug("--> setting style on render element bgcolor=%s", newStyle->backgroundColor().name().latin1());
                m_render->setStyle(newStyle);
            }
        }
        else if (changed() && m_render && newStyle && (getDocument()->usesSiblingRules() || getDocument()->usesDescendantRules())) {
            // Although no change occurred, we use the new style so that the cousin style sharing code won't get
            // fooled into believing this style is the same.  This is only necessary if the document actually uses
            // sibling/descendant rules, since otherwise it isn't possible for ancestor styles to affect sharing of
            // descendants.
            m_render->setStyleInternal(newStyle);
        }

        newStyle->deref(getDocument()->renderArena());

        if ( change != Force) {
            if (getDocument()->usesDescendantRules())
                change = Force;
            else
                change = ch;
        }
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

// DOM Section 1.1.1
bool ElementImpl::childAllowed( NodeImpl *newChild )
{
    if (!childTypeAllowed(newChild->nodeType()))
        return false;

    // For XML documents, we are non-validating and do not check against a DTD, even for HTML elements.
    if (getDocument()->isHTMLDocument())
        return checkChild(id(), newChild->id(), !getDocument()->inCompatMode());
    return true;
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

DOMString ElementImpl::openTagStartToString() const
{
    DOMString result = DOMString("<") + tagName();

    NamedAttrMapImpl *attrMap = attributes(true);

    if (attrMap) {
	unsigned long numAttrs = attrMap->length();
	for (unsigned long i = 0; i < numAttrs; i++) {
	    result += " ";

	    AttributeImpl *attribute = attrMap->attributeItem(i);
	    AttrImpl *attr = attribute->attrImpl();

	    if (attr) {
		result += attr->toString();
	    } else {
		result += getDocument()->attrName(attribute->id());
		if (!attribute->value().isNull()) {
		    result += "=\"";
		    // FIXME: substitute entities for any instances of " or '
		    result += attribute->value();
		    result += "\"";
		}
	    }
	}
    }

    return result;
}

DOMString ElementImpl::toString() const
{
    DOMString result = openTagStartToString();

    if (hasChildNodes()) {
	result += ">";

	for (NodeImpl *child = firstChild(); child != NULL; child = child->nextSibling()) {
	    result += child->toString();
	}

	result += "</";
	result += tagName();
	result += ">";
    } else {
	result += " />";
    }

    return result;
}

void ElementImpl::updateId(const AtomicString& oldId, const AtomicString& newId)
{
    if (!inDocument())
	return;

    if (oldId == newId)
	return;

    DocumentImpl* doc = getDocument();
    if (!oldId.isEmpty())
	doc->removeElementById(oldId, this);
    if (!newId.isEmpty())
	doc->addElementById(newId, this);
}

#ifndef NDEBUG
void ElementImpl::dump(QTextStream *stream, QString ind) const
{
    updateStyleAttributeIfNeeded();
    if (namedAttrMap) {
        for (uint i = 0; i < namedAttrMap->length(); i++) {
            AttributeImpl *attr = namedAttrMap->attributeItem(i);
            *stream << " " << DOMString(getDocument()->attrName(attr->id())).string().ascii()
                    << "=\"" << DOMString(attr->value()).string().ascii() << "\"";
        }
    }

    ContainerNodeImpl::dump(stream,ind);
}
#endif

#ifndef NDEBUG
void ElementImpl::formatForDebugger(char *buffer, unsigned length) const
{
    DOMString result;
    DOMString s;
    
    s = nodeName();
    if (s.length() > 0) {
        result += s;
    }
          
    s = getAttribute(ATTR_ID);
    if (s.length() > 0) {
        if (result.length() > 0)
            result += "; ";
        result += "id=";
        result += s;
    }
          
    s = getAttribute(ATTR_CLASS);
    if (s.length() > 0) {
        if (result.length() > 0)
            result += "; ";
        result += "class=";
        result += s;
    }
          
    strncpy(buffer, result.string().latin1(), length - 1);
}
#endif

SharedPtr<AttrImpl> ElementImpl::setAttributeNode(AttrImpl *attr, int &exception)
{
    return static_pointer_cast<AttrImpl>(attributes(false)->setNamedItem(attr, exception));
}

SharedPtr<AttrImpl> ElementImpl::removeAttributeNode(AttrImpl *attr, int &exception)
{
    if (!attr || attr->ownerElement() != this) {
        exception = DOMException::NOT_FOUND_ERR;
        return SharedPtr<AttrImpl>();
    }
    if (getDocument() != attr->getDocument()) {
        exception = DOMException::WRONG_DOCUMENT_ERR;
        return SharedPtr<AttrImpl>();
    }

    NamedAttrMapImpl *attrs = attributes(true);
    if (!attrs)
        return SharedPtr<AttrImpl>();

    return static_pointer_cast<AttrImpl>(attrs->removeNamedItem(attr->m_attribute->id(), exception));
}

void ElementImpl::setAttributeNS(const DOMString &namespaceURI, const DOMString &qualifiedName, const DOMString &value, int &exception)
{
    int colonpos = qualifiedName.find(':');
    DOMString localName = qualifiedName;
    if (colonpos >= 0) {
        localName.remove(0, colonpos+1);
        // ### extract and set new prefix
    }

    if (!DocumentImpl::isValidName(localName)) {
        exception = DOMException::INVALID_CHARACTER_ERR;
        return;
    }

    Id id = getDocument()->attrId(namespaceURI.implementation(), localName.implementation(), false /* allocate */);
    setAttribute(id, value.implementation(), exception);
}

void ElementImpl::removeAttributeNS(const DOMString &namespaceURI, const DOMString &localName, int &exception)
{
    Id id = getDocument()->attrId(namespaceURI.implementation(), localName.implementation(), true);
    if (!id)
        return;
    removeAttribute(id, exception);
}

AttrImpl *ElementImpl::getAttributeNodeNS(const DOMString &namespaceURI, const DOMString &localName)
{
    Id id = getDocument()->attrId(namespaceURI.implementation(), localName.implementation(), true);
    if (!id)
        return 0;
    NamedAttrMapImpl *attrs = attributes(true);
    if (!attrs)
        return 0;
    return attrs->getNamedItem(id);
}

bool ElementImpl::hasAttributeNS(const DOMString &namespaceURI, const DOMString &localName) const
{
    Id id = getDocument()->attrId(namespaceURI.implementation(), localName.implementation(), true);
    if (!id)
        return false;
    NamedAttrMapImpl *attrs = attributes(true);
    if (!attrs)
        return false;
    return attrs->getAttributeItem(id);
}

CSSStyleDeclarationImpl *ElementImpl::style()
{
    return 0;
}

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

DOMString XMLElementImpl::namespaceURI() const
{
    return getDocument()->namespaceURI(m_id);
}

NodeImpl *XMLElementImpl::cloneNode ( bool deep )
{
    // ### we loose namespace here FIXME
    // should pass id around
    XMLElementImpl *clone = new XMLElementImpl(docPtr(), getDocument()->tagName(m_id).implementation());
    clone->m_id = m_id;

    // clone attributes
    if (namedAttrMap)
        *clone->attributes(false) = *namedAttrMap;

    if (deep)
        cloneChildNodes(clone);

    return clone;
}

// -------------------------------------------------------------------------

NamedAttrMapImpl::NamedAttrMapImpl(ElementImpl *e)
    : element(e)
    , attrs(0)
    , len(0)
{
}

NamedAttrMapImpl::~NamedAttrMapImpl()
{
    NamedAttrMapImpl::clearAttributes(); // virtual method, so qualify just to be explicit
}

bool NamedAttrMapImpl::isHTMLAttributeMap() const
{
    return false;
}

AttrImpl *NamedAttrMapImpl::getNamedItem ( NodeImpl::Id id ) const
{
    AttributeImpl* a = getAttributeItem(id);
    if (!a) return 0;

    if (!a->attrImpl())
        a->allocateImpl(element);

    return a->attrImpl();
}

SharedPtr<NodeImpl> NamedAttrMapImpl::setNamedItem ( NodeImpl* arg, int &exceptioncode )
{
    if (!element) {
        exceptioncode = DOMException::NOT_FOUND_ERR;
        return SharedPtr<NodeImpl>();
    }

    // NO_MODIFICATION_ALLOWED_ERR: Raised if this map is readonly.
    if (isReadOnly()) {
        exceptioncode = DOMException::NO_MODIFICATION_ALLOWED_ERR;
        return SharedPtr<NodeImpl>();
    }

    // WRONG_DOCUMENT_ERR: Raised if arg was created from a different document than the one that created this map.
    if (arg->getDocument() != element->getDocument()) {
        exceptioncode = DOMException::WRONG_DOCUMENT_ERR;
        return SharedPtr<NodeImpl>();
    }

    // Not mentioned in spec: throw a HIERARCHY_REQUEST_ERROR if the user passes in a non-attribute node
    if (!arg->isAttributeNode()) {
        exceptioncode = DOMException::HIERARCHY_REQUEST_ERR;
        return SharedPtr<NodeImpl>();
    }
    AttrImpl *attr = static_cast<AttrImpl*>(arg);

    AttributeImpl* a = attr->attrImpl();
    AttributeImpl* old = getAttributeItem(a->id());
    if (old == a)
        return SharedPtr<NodeImpl>(arg); // we know about it already

    // INUSE_ATTRIBUTE_ERR: Raised if arg is an Attr that is already an attribute of another Element object.
    // The DOM user must explicitly clone Attr nodes to re-use them in other elements.
    if (attr->ownerElement()) {
        exceptioncode = DOMException::INUSE_ATTRIBUTE_ERR;
        return SharedPtr<NodeImpl>();
    }

    if (a->id() == ATTR_ID) {
	element->updateId(old ? old->value() : nullAtom, a->value());
    }

    // ### slightly inefficient - resizes attribute array twice.
    SharedPtr<NodeImpl> r;
    if (old) {
        if (!old->attrImpl())
            old->allocateImpl(element);
        r.reset(old->_impl);
        removeAttribute(a->id());
    }

    addAttribute(a);
    return r;
}

// The DOM2 spec doesn't say that removeAttribute[NS] throws NOT_FOUND_ERR
// if the attribute is not found, but at this level we have to throw NOT_FOUND_ERR
// because of removeNamedItem, removeNamedItemNS, and removeAttributeNode.
SharedPtr<NodeImpl> NamedAttrMapImpl::removeNamedItem ( NodeImpl::Id id, int &exceptioncode )
{
    // ### should this really be raised when the attribute to remove isn't there at all?
    // NO_MODIFICATION_ALLOWED_ERR: Raised when the node is readonly
    if (isReadOnly()) {
        exceptioncode = DOMException::NO_MODIFICATION_ALLOWED_ERR;
        return SharedPtr<NodeImpl>();
    }

    AttributeImpl* a = getAttributeItem(id);
    if (!a) {
        exceptioncode = DOMException::NOT_FOUND_ERR;
        return SharedPtr<NodeImpl>();
    }

    if (!a->attrImpl())  a->allocateImpl(element);
    SharedPtr<NodeImpl> r(a->attrImpl());

    if (id == ATTR_ID) {
	element->updateId(a->value(), nullAtom);
    }

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

AttributeImpl* NamedAttrMapImpl::getAttributeItem(NodeImpl::Id id) const
{
    bool matchAnyNamespace = (namespacePart(id) == anyNamespace);
    for (unsigned long i = 0; i < len; ++i) {
        if (attrs[i]->id() == id)
            return attrs[i];
        else if (matchAnyNamespace) {
            if (localNamePart(attrs[i]->id()) == localNamePart(id))
                return attrs[i];
        }
    }
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
        main_thread_free(attrs);
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

    // If assigning the map changes the id attribute, we need to call
    // updateId.

    AttributeImpl *oldId = getAttributeItem(ATTR_ID);
    AttributeImpl *newId = other.getAttributeItem(ATTR_ID);

    if (oldId || newId) {
	element->updateId(oldId ? oldId->value() : nullAtom, newId ? newId->value() : nullAtom);
    }

    clearAttributes();
    len = other.len;
    attrs = static_cast<AttributeImpl **>(main_thread_malloc(len * sizeof(AttributeImpl *)));

    // first initialize attrs vector, then call attributeChanged on it
    // this allows attributeChanged to use getAttribute
    for (uint i = 0; i < len; i++) {
        attrs[i] = other.attrs[i]->clone();
        attrs[i]->ref();
    }

    // FIXME: This is wasteful.  The class list could be preserved on a copy, and we
    // wouldn't have to waste time reparsing the attribute.
    // The derived class, HTMLNamedAttrMapImpl, which manages a parsed class list for the CLASS attribute,
    // will update its member variable when parse attribute is called.
    for(uint i = 0; i < len; i++)
        element->attributeChanged(attrs[i], true);

    return *this;
}

void NamedAttrMapImpl::addAttribute(AttributeImpl *attr)
{
    // Add the attribute to the list
    AttributeImpl **newAttrs = static_cast<AttributeImpl **>(main_thread_malloc((len + 1) * sizeof(AttributeImpl *)));
    if (attrs) {
      for (uint i = 0; i < len; i++)
        newAttrs[i] = attrs[i];
      main_thread_free(attrs);
    }
    attrs = newAttrs;
    attrs[len++] = attr;
    attr->ref();

    AttrImpl * const attrImpl = attr->_impl;
    if (attrImpl)
        attrImpl->m_element = element;

    // Notify the element that the attribute has been added, and dispatch appropriate mutation events
    // Note that element may be null here if we are called from insertAttr() during parsing
    if (element) {
        element->attributeChanged(attr);
        element->dispatchAttrAdditionEvent(attr);
        element->dispatchSubtreeModifiedEvent(false);
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
        main_thread_free(attrs);
        attrs = 0;
        len = 0;
    }
    else {
        AttributeImpl **newAttrs = static_cast<AttributeImpl **>(main_thread_malloc((len - 1) * sizeof(AttributeImpl *)));
        uint i;
        for (i = 0; i < uint(index); i++)
            newAttrs[i] = attrs[i];
        len--;
        for (; i < len; i++)
            newAttrs[i] = attrs[i+1];
        main_thread_free(attrs);
        attrs = newAttrs;
    }

    // Notify the element that the attribute has been removed
    // dispatch appropriate mutation events
    if (element && !attr->_value.isNull()) {
        AtomicString value = attr->_value;
        attr->_value = nullAtom;
        element->attributeChanged(attr);
        attr->_value = value;
    }
    if (element) {
        element->dispatchAttrRemovalEvent(attr);
        element->dispatchSubtreeModifiedEvent(false);
    }
    attr->deref();
}

