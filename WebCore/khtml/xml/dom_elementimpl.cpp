/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
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
 *
 * $Id$
 */
//#define EVENT_DEBUG
#include "dom_elementimpl.h"
#include "dom_exception.h"
#include "dom_node.h"
#include "dom_textimpl.h"
#include "dom_docimpl.h"
#include "dom2_eventsimpl.h"
#include "dtd.h"

#include "css/cssstyleselector.h"
#include "rendering/render_object.h"
#include "misc/htmlhashes.h"
#include <kdebug.h>
#include "css_valueimpl.h"
#include "css_stylesheetimpl.h"
#include "html/htmlparser.h"

using namespace DOM;
using namespace khtml;

/*
 * NOTE:
 * According to the DOM docs, an Attr stores the value directly in it's parsed
 * form, but for values containing entities, creates a subtree with nodes
 * containing the unexpanded form (for XML). On read, the value is always
 * returned entity-free, so we decided for HTML we could only store a
 * parsed DOMString and have no child-nodes.
 */

AttrImpl::AttrImpl()
    : NodeImpl(0),
      _name( 0 ),
      _value( 0 ),
      _element(0),
      attrId(0)
{
}


AttrImpl::AttrImpl(DocumentPtr *doc, const DOMString &name)
    : NodeImpl(doc),
      _name( 0 ),
      _value( 0 ),
      _element(0),
      attrId(0)
{
    setName(name);
}

AttrImpl::AttrImpl(DocumentPtr *doc, int id)
    : NodeImpl(doc),
      _name( 0 ),
      _value( 0 ),
      _element(0),
      attrId(id)
{
}

AttrImpl::AttrImpl(const AttrImpl &other) : NodeImpl(other.docPtr())
{
    m_specified = other.specified();
    _element = other._element;
    _name = other._name;
    if (_name) _name->ref();
    _value = other._value;
    if (_value) _value->ref();
    attrId = other.attrId;
}

AttrImpl &AttrImpl::operator = (const AttrImpl &other)
{
    NodeImpl::operator =(other);
    m_specified = other.specified();
    _element = other._element;

    if (_name) _name->deref();
    _name = other._name;
    if (_name) _name->ref();
    if (_value) _value->deref();
    _value = other._value;
    if (_value) _value->ref();
    attrId = other.attrId;

    return *this;
}

AttrImpl::~AttrImpl()
{
    if(_name) _name->deref();
    if(_value) _value->deref();
}

const DOMString AttrImpl::nodeName() const
{
    return name();
}

unsigned short AttrImpl::nodeType() const
{
    return Node::ATTRIBUTE_NODE;
}


DOMString AttrImpl::name() const
{
    if(attrId)
        return getAttrName(attrId);
    else if (_name)
        return _name;
    else
        return DOMString();
}

void AttrImpl::setName(const DOMString &n)
{
    if(_name) _name->deref();
    _name = n.implementation();
    if(!_name) return;

    attrId = khtml::getAttrID(QConstString(_name->s, _name->l).string().lower().ascii(), _name->l);
    if (attrId)
        _name = 0;
    else
        _name->ref();
}

DOMString AttrImpl::value() const {
    return _value;
}

void AttrImpl::setValue( const DOMString &v )
{
    // according to the DOM docs, we should create an unparsed Text child
    // node here; we decided this was not necessary for HTML

    // ### TODO: parse value string, interprete entities (not sure if we are supposed to do this)

    if (_element)
        _element->checkReadOnly();

    DOMStringImpl *prevValue = _value;
    _value = v.implementation();
    if (_value) _value->ref();
    m_specified = true;

    if (_element) {
        _element->parseAttribute(this);
        _element->setChanged(true);
	if (getDocument()->hasListenerType(DocumentImpl::DOMATTRMODIFIED_LISTENER)) {
	    int exceptioncode;
	    _element->dispatchEvent(new MutationEventImpl(EventImpl::DOMATTRMODIFIED_EVENT,true,false,this,prevValue,
				    _value,_name,MutationEvent::MODIFICATION),exceptioncode);
	}
    }
    if (prevValue) prevValue->deref();
}

void AttrImpl::setNodeValue( const DOMString &v, int &exceptioncode )
{
    exceptioncode = 0;
    setValue(v);
}

AttrImpl::AttrImpl(const DOMString &name, const DOMString &value, DocumentPtr *doc)
    : NodeImpl(doc)
{
    attrId = 0;
    _name = 0;
    setName(name);
    _value = value.implementation();
    if (_value) _value->ref();
    _element = 0;
    m_specified = 1;
}

AttrImpl::AttrImpl(int _id, const DOMString &value, DocumentPtr *doc) : NodeImpl(doc)
{
  attrId = _id;
  _name = 0;
  _value = value.implementation();
  if (_value) _value->ref();
  _element = 0;
  m_specified = false;
}

NodeImpl *AttrImpl::parentNode() const
{
    return 0;
}

NodeImpl *AttrImpl::previousSibling() const
{
    return 0;
}

NodeImpl *AttrImpl::nextSibling() const
{
    return 0;
}

NodeImpl *AttrImpl::cloneNode ( bool /*deep*/, int &/*exceptioncode*/ )
{
    AttrImpl *newImpl = new AttrImpl(*this);
    newImpl->_element = 0; // can't have two attributes with the same name/id attached to an element
    return newImpl;
}

bool AttrImpl::deleteMe()
{
    if(!_element && _ref <= 0) return true;
    return false;
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

ElementImpl::ElementImpl(DocumentPtr *doc) : NodeBaseImpl(doc)
{
    namedAttrMap = 0;

    has_tabindex=false;
    tabindex=0;
    m_styleDecls = 0;
}

ElementImpl::~ElementImpl()
{
    if (m_render)
        detach();

    if(namedAttrMap) {
        namedAttrMap->detachFromElement();
        namedAttrMap->deref();
    }

    if (m_styleDecls) {
        m_styleDecls->setNode(0);
        m_styleDecls->setParent(0);
        m_styleDecls->deref();
    }
}

bool ElementImpl::isInline() const
{
    if(!m_style) return false;
    return (m_style->display() == khtml::INLINE);
}

unsigned short ElementImpl::nodeType() const
{
    return Node::ELEMENT_NODE;
}

DOMString ElementImpl::tagName() const
{
    return nodeName();
}

DOMString ElementImpl::getAttribute( const DOMString &name ) const
{
  // search in already set attributes first
    int exceptioncode; // ### propogate
    if(!namedAttrMap) return 0;
    AttrImpl *attr = static_cast<AttrImpl*>(namedAttrMap->getNamedItem(name,exceptioncode));
    if (attr) return attr->value();

    // then search in default attr in case it is not yet set
    NamedAttrMapImpl* dm = defaultMap();
    if(!dm) return 0;
    AttrImpl* defattr = static_cast<AttrImpl*>(dm->getNamedItem(name, exceptioncode));
    if(!defattr || exceptioncode) return 0;

    return defattr->value();
}

DOMString ElementImpl::getAttribute( int id )
{
    // search in already set attributes first
    if(!namedAttrMap) return DOMString();
    AttrImpl *attr = static_cast<AttrImpl*>(namedAttrMap->getIdItem(id));
    if (attr) return attr->value();

    // then search in default attr in case it is not yet set
    NamedAttrMapImpl* dm = defaultMap();
    if(!dm) return 0;

    AttrImpl* defattr = static_cast<AttrImpl*>(dm->getIdItem(id));
    if(!defattr) return DOMString();

    return defattr->value();
}

AttrImpl *ElementImpl::getAttributeNode ( int index ) const
{
    return namedAttrMap ? namedAttrMap->getIdItem(index) : 0;
}

int ElementImpl::getAttributeCount() const
{
    int exceptioncode; // ### propogate
    return namedAttrMap ? namedAttrMap->length(exceptioncode) : 0;
}

void ElementImpl::setAttribute( const DOMString &name, const DOMString &value)
{
    // ### check for invalid characters in value -> throw exception
    int exceptioncode; // ### propogate
    if(!namedAttrMap) {
        namedAttrMap = new NamedAttrMapImpl(this);
        namedAttrMap->ref();
    }
    if (value.isNull())
        namedAttrMap->removeNamedItem(name,exceptioncode);
    else {
        AttrImpl *a = static_cast<AttrImpl*>(namedAttrMap->getNamedItem(name));
        if (a)
            a->setValue(value);
        else
            namedAttrMap->setNamedItem(new AttrImpl(name,value,docPtr()), exceptioncode);
    }
}

void ElementImpl::setAttribute( int id, const DOMString &value )
{
    if(!namedAttrMap) {
        namedAttrMap = new NamedAttrMapImpl(this);
        namedAttrMap->ref();
    }
    if (value.isNull())
        namedAttrMap->removeIdItem(id);
    else {
        int exceptioncode;
        AttrImpl* a = static_cast<AttrImpl*>(namedAttrMap->getIdItem(id));
        if(a)
            a->setValue(value);
        else
            namedAttrMap->setIdItem(new AttrImpl(id,value,docPtr() ), exceptioncode );
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

        for(unsigned int i = 0; i < len; i++) {
            AttrImpl* a = namedAttrMap->attrs[i];
            if(a && !a->_element) {
                a->_element = this;

                parseAttribute(a);
            }
        }
    }
}

void ElementImpl::removeAttribute( const DOMString &name )
{
    int exceptioncode; // ### propogate
    if(!namedAttrMap) return;
    namedAttrMap->removeNamedItem(name,exceptioncode);
}

NodeImpl *ElementImpl::cloneNode ( bool deep, int &exceptioncode )
{
    ElementImpl *newImpl = ownerDocument()->createElement(tagName());
    if (!newImpl)
      return 0;

    // clone attributes
    if(namedAttrMap)
        *(static_cast<NamedAttrMapImpl*>(newImpl->attributes())) = *namedAttrMap;

    if (deep)
        cloneChildNodes(newImpl,exceptioncode);
    return newImpl;
}

NamedNodeMapImpl *ElementImpl::attributes()
{
    if(!namedAttrMap) {
        namedAttrMap = new NamedAttrMapImpl(this);
        namedAttrMap->ref();
    }
    return namedAttrMap;
}

AttrImpl *ElementImpl::getAttributeNode( const DOMString &name )
{
    int exceptioncode; // ### propogate
    // ### do we return attribute node if it is in the default map but not specified?
    if(!namedAttrMap) return 0;
    return static_cast<AttrImpl*>(namedAttrMap->getNamedItem(name,exceptioncode));

}

Attr ElementImpl::setAttributeNode( AttrImpl *newAttr, int &exceptioncode )
{
    exceptioncode = 0;
    if (!newAttr) {
        exceptioncode = DOMException::NOT_FOUND_ERR;
        return 0;
    }
    if(!namedAttrMap) {
        namedAttrMap = new NamedAttrMapImpl(this);
        namedAttrMap->ref();
    }
    if (newAttr->attrId)
        return namedAttrMap->setIdItem(newAttr, exceptioncode);
    else
        return namedAttrMap->setNamedItem(newAttr, exceptioncode);
}

Attr ElementImpl::removeAttributeNode( AttrImpl *oldAttr, int &exceptioncode )
{
    // ### should we replace with default in map? currently default attrs don't exist in map
    exceptioncode = 0;
    if(!namedAttrMap) return 0;
    return namedAttrMap->removeAttr(oldAttr, exceptioncode);
}

NodeListImpl *ElementImpl::getElementsByTagName( const DOMString &name )
{
    return new TagNodeListImpl( this, name );
}

short ElementImpl::tabIndex() const
{
  if (has_tabindex)
    return tabindex;
  else
    return -1;
}

void ElementImpl::setTabIndex( short _tabindex )
{
  has_tabindex=true;
  tabindex=_tabindex;
}

void ElementImpl::normalize( int &exceptioncode )
{
    // In DOM level 2, this gets moved to Node
    // ### normalize attributes? (when we store attributes using child nodes)
    exceptioncode = 0;
    NodeImpl *child = _first;
    while (child) {
        NodeImpl *nextChild = child->nextSibling();
        if (child->nodeType() == Node::ELEMENT_NODE) {
            static_cast<ElementImpl*>(child)->normalize(exceptioncode);
            if (exceptioncode)
                return;
            child = nextChild;
        }
        else if (nextChild && child->nodeType() == Node::TEXT_NODE && nextChild->nodeType() == Node::TEXT_NODE) {
            static_cast<TextImpl*>(child)->appendData(static_cast<TextImpl*>(nextChild)->data());
            removeChild(nextChild,exceptioncode);
            if (exceptioncode)
                return;
        }
        else
            child = nextChild;
    }
}

NamedAttrMapImpl* ElementImpl::defaultMap() const
{
    return 0;
}

void ElementImpl::attach()
{
    if (!m_render)
    {
#if SPEED_DEBUG < 2
        setStyle(ownerDocument()->styleSelector()->styleForElement(this));
#if SPEED_DEBUG < 1
        if(_parent && _parent->renderer())
        {
            m_render = khtml::RenderObject::createObject(this);
            if(m_render)
            {
                _parent->renderer()->addChild(m_render, nextRenderer());
            }
        }
#endif
#endif

    }
    NodeBaseImpl::attach();
}

void ElementImpl::detach()
{
    NodeBaseImpl::detach();

    if ( m_render )
        m_render->detach();

    m_render = 0;
}

void ElementImpl::recalcStyle()
{
    if(!m_style) return;
    EDisplay oldDisplay = m_style->display();

    int dynamicState = StyleSelector::None;
    if ( m_mouseInside )
        dynamicState |= StyleSelector::Hover;
    if ( m_focused )
        dynamicState |= StyleSelector::Focus;
    if ( m_active )
        dynamicState |= StyleSelector::Active;

    setStyle( ownerDocument()->styleSelector()->styleForElement(this, dynamicState) );

    if (oldDisplay != m_style->display()) {
	detach();
	attach();
    }
    if( m_render && m_style )
        m_render->setStyle(m_style);
    NodeImpl *n;
    for (n = _first; n; n = n->nextSibling())
        n->recalcStyle();
}

bool ElementImpl::prepareMouseEvent( int _x, int _y,
                                     int _tx, int _ty,
                                     MouseEvent *ev)
{
#ifdef EVENT_DEBUG
    kdDebug( 6030 ) << nodeName().string() << "::prepareMouseEvent" << endl;
#endif
    bool inside = false;

    if(!m_render) return false;

    int origTx = _tx;
    int origTy = _ty;

    RenderObject *p = m_render->parent();
    while( p && p->isAnonymousBox() ) {
//      kdDebug( 6030 ) << "parent is anonymous!" << endl;
        // we need to add the offset of the anonymous box
        _tx += p->xPos();
        _ty += p->yPos();
        p = p->parent();
    }

    bool positioned = m_render->isPositioned();
    int oldZIndex = ev->currentZIndex;

    // Positioned element -> store current zIndex, for children to use
    if ( positioned ) {
        ev->currentZIndex = m_render->style()->zIndex();
        //kdDebug() << "ElementImpl::prepareMouseEvent storing currentZIndex=" << ev->currentZIndex << endl;
    }

    if(!m_render->isInline() || !m_render->firstChild() || m_render->isFloating() ) {
        bool known = m_render->absolutePosition(_tx, _ty);
	if (known && m_render->containsPoint(_x,_y,_tx,_ty)) {
            if  ( m_render->style() && !m_render->style()->visiblity() == HIDDEN ) {
                //if ( positioned )
                //    kdDebug(6030) << " currentZIndex=" << ev->currentZIndex << " ev->zIndex=" << ev->zIndex << endl;
                if ( ev->currentZIndex >= ev->zIndex ) {
                    //kdDebug(6030) << nodeName().string() << " SETTING innerNode " << endl;
                    ev->innerNode = Node(this);
                    ev->nodeAbsX = origTx;
                    ev->nodeAbsY = origTy;
                    ev->zIndex = ev->currentZIndex;
                    inside = true;
                }
            }
        }
    }

    NodeImpl *child = firstChild();
    while(child != 0) {
        if(child->prepareMouseEvent(_x, _y, _tx, _ty, ev))
            inside = true;
        child = child->nextSibling();
    }

#ifdef EVENT_DEBUG
    if(inside) kdDebug( 6030 ) << nodeName().string() << "    --> inside" << endl;
#endif

#if 0
    // #############
    if(inside || mouseInside())
        if  ( ! (m_render->style() && m_render->style()->visiblity() == HIDDEN) )
        {
            // dynamic HTML...
            // ### mouseEventHandler(ev, inside);
        }
#endif

    bool oldinside=mouseInside();

    setMouseInside(inside);

    bool oldactive = active();
    if ( inside ) {
	if ( ev->type == MousePress )
	    m_active = true;
	else if ( ev->type == MouseRelease )
	    m_active = false;
    } else if ( m_active ) {
	m_active = false;
    }


    if ( (oldinside != inside && m_style->hasHover()) ||
	 ( oldactive != m_active && m_style->hasActive() ) )
        applyChanges(true, false);

    // reset previous z index
    if ( positioned )
        ev->currentZIndex = oldZIndex;

    return inside;
}

void ElementImpl::setFocus(bool received)
{
    NodeBaseImpl::setFocus(received);
    applyChanges(true, false);
}

void ElementImpl::setActive(bool down)
{
    NodeBaseImpl::setActive(down);
    applyChanges(true, false);
}

khtml::FindSelectionResult ElementImpl::findSelectionNode( int _x, int _y, int _tx, int _ty, DOM::Node & node, int & offset )
{
    //kdDebug(6030) << "ElementImpl::findSelectionNode " << this << " _x=" << _x << " _y=" << _y
    //           << " _tx=" << _tx << " _ty=" << _ty << endl;

    // ######### Duplicated code from mouseEvent
    // TODO put the code above (getting _tx,_ty) in a common place and call it from here

    if (!m_render) return SelectionPointAfter;

    RenderObject *p = m_render->parent();
    while( p && p->isAnonymousBox() ) {
//      kdDebug( 6030 ) << "parent is anonymous!" << endl;
        // we need to add the offset of the anonymous box
        _tx += p->xPos();
        _ty += p->yPos();
        p = p->parent();
    }

    if(!m_render->isInline() || !m_render->firstChild() || m_render->isFloating() )
    {
        m_render->absolutePosition(_tx, _ty);
    }

    int off=0, lastOffset=0;
    DOM::Node nod;
    DOM::Node lastNode;
    NodeImpl *child = firstChild();
    while(child != 0)
    {
        khtml::FindSelectionResult pos = child->findSelectionNode(_x, _y, _tx, _ty, nod, off);
        //kdDebug(6030) << this << " child->findSelectionNode returned " << pos << endl;
        if ( pos == SelectionPointInside ) // perfect match
        {
            node = nod;
            offset = off;
            //kdDebug(6030) << "ElementImpl::findSelectionNode " << this << " match offset=" << offset << endl;
            return SelectionPointInside;
        } else if ( pos == SelectionPointBefore )
        {
            //x,y is before this element -> stop here
            if ( !lastNode.isNull() ) {
                node = lastNode;
                offset = lastOffset;
                //kdDebug(6030) << "ElementImpl::findSelectionNode " << this << " before this child -> returning offset=" << offset << endl;
                return SelectionPointInside;
            } else {
                //kdDebug(6030) << "ElementImpl::findSelectionNode " << this << " before us -> returning -2" << endl;
                return SelectionPointBefore;
            }
        }
        // SelectionPointAfter -> keep going
        if ( !nod.isNull() )
        {
            lastNode = nod;
            lastOffset = off;
        }
        child = child->nextSibling();
    }
    node = lastNode;
    offset = lastOffset;
    return SelectionPointAfter;
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
    if (id() && newChild->id()) // if one if these is 0 then it is an xml element and we allow it anyway
        return checkChild(id(), newChild->id());
    else
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

void ElementImpl::createDecl( )
{
    m_styleDecls = new CSSStyleDeclarationImpl(0);
    m_styleDecls->ref();
    m_styleDecls->setParent(ownerDocument()->elementSheet());
    m_styleDecls->setNode(this);
    m_styleDecls->setStrictParsing( ownerDocument()->parseMode() == DocumentImpl::Strict );
}

void ElementImpl::dispatchAttrRemovalEvent(NodeImpl *attr)
{
    if (!getDocument()->hasListenerType(DocumentImpl::DOMATTRMODIFIED_LISTENER))
	return;
    int exceptioncode;
    AttrImpl *att = static_cast<AttrImpl*>(attr);
    dispatchEvent(new MutationEventImpl(EventImpl::DOMATTRMODIFIED_EVENT,true,false,attr,att->value(),
		  att->value(),att->name(),MutationEvent::REMOVAL),exceptioncode);
}

void ElementImpl::dispatchAttrAdditionEvent(NodeImpl *attr)
{
    if (!getDocument()->hasListenerType(DocumentImpl::DOMATTRMODIFIED_LISTENER))
	return;
    int exceptioncode;
    AttrImpl *att = static_cast<AttrImpl*>(attr);
    dispatchEvent(new MutationEventImpl(EventImpl::DOMATTRMODIFIED_EVENT,true,false,attr,att->value(),
		  att->value(),att->name(),MutationEvent::ADDITION),exceptioncode);
}

// -------------------------------------------------------------------------

XMLElementImpl::XMLElementImpl(DocumentPtr *doc, DOMStringImpl *_name) : ElementImpl(doc)
{
    m_name = _name;
    if (m_name)
        m_name->ref();
    m_namespaceURI = 0;
    m_id = ownerDocument()->elementId(_name);
}

XMLElementImpl::XMLElementImpl(DocumentPtr *doc, DOMStringImpl *_name, DOMStringImpl *_namespaceURI) : ElementImpl(doc)
{
    m_name = _name;
    if (m_name)
        m_name->ref();
    m_namespaceURI = _namespaceURI;
    if (m_namespaceURI)
        m_namespaceURI->ref();
    m_id = ownerDocument()->elementId(_name);
}

XMLElementImpl::~XMLElementImpl()
{
    if (m_name)
        m_name->deref();
    if (m_namespaceURI)
        m_namespaceURI->deref();
}

const DOMString XMLElementImpl::nodeName() const
{
    return m_name;
}

DOMString XMLElementImpl::namespaceURI() const
{
    return m_namespaceURI;
}


bool XMLElementImpl::isXMLElementNode() const
{
    return true;
}

// -------------------------------------------------------------------------

NamedAttrMapImpl::NamedAttrMapImpl(ElementImpl *e) : element(e)
{
    attrs = 0;
    len = 0;
}

NamedAttrMapImpl::~NamedAttrMapImpl()
{
    clearAttrs();
}

NamedAttrMapImpl &NamedAttrMapImpl::operator =(const NamedAttrMapImpl &other)
{
    // clone all attributes in the other map, but attach to our element
    if (!element)
        return *this;

    clearAttrs();
    len = other.len;
    attrs = new AttrImpl* [len];
    uint i;

    // first initialize attrs vector, then call parseAttribute on it
    // this allows parseAttribute to use getAttribute
    for (i = 0; i < len; i++) {
        int exceptioncode; // ### propogate
        attrs[i] = static_cast<AttrImpl*>(other.attrs[i]->cloneNode(true,exceptioncode));
        attrs[i]->_element = element;
        attrs[i]->ref();
    }

    for(i = 0; i < len; i++)
        element->parseAttribute(attrs[i]);

    element->setChanged(true);
    return *this;
}

unsigned long NamedAttrMapImpl::length(int &/*exceptioncode*/) const
{
    return length();
}

unsigned long NamedAttrMapImpl::length() const
{
    return len;
}

NodeImpl *NamedAttrMapImpl::getNamedItem ( const DOMString &name, int &/*exceptioncode*/ ) const
{
    return getNamedItem(name);
}

NodeImpl *NamedAttrMapImpl::getNamedItem ( const DOMString &name ) const
{
    uint i;
    for (i = 0; i < len; i++) {
        if (!strcasecmp(attrs[i]->name(),name))
            return attrs[i];
    }

    return 0;
}

AttrImpl *NamedAttrMapImpl::getIdItem ( int id ) const
{
    uint i;
    for (i = 0; i < len; i++) {
        if (attrs[i]->attrId == id)
            return attrs[i];
    }

    return 0;
}


Node NamedAttrMapImpl::setNamedItem ( const Node &arg, int &exceptioncode )
{
    // ### check for invalid chars in name ?
    // ### check same document
    exceptioncode = 0;
    if (!element) {
        exceptioncode = DOMException::NOT_FOUND_ERR;
        return 0;
    }
    element->checkReadOnly();

    if (arg.nodeType() != Node::ATTRIBUTE_NODE) {
        exceptioncode = DOMException::HIERARCHY_REQUEST_ERR;
        return 0;
    }

    AttrImpl *attr = static_cast<AttrImpl*>(arg.handle());

    if (attr->_element) {
        exceptioncode = DOMException::INUSE_ATTRIBUTE_ERR;
        return 0;
    }

    uint i;
    for (i = 0; i < len; i++) {
        // ### for XML attributes are case sensitive (?) - check this elsewhere also
        if (!strcasecmp(attrs[i]->name(),attr->name())) {
            // attribute with this id already in list
            Attr oldAttr = attrs[i];
            attrs[i]->_element = 0;
            attrs[i]->deref();
            attrs[i] = attr;
            attrs[i]->ref();
            attr->_element = element;
            element->parseAttribute(attr);
            element->setChanged(true);
            element->dispatchAttrRemovalEvent(oldAttr.handle());
            element->dispatchAttrAdditionEvent(attrs[i]);
	    element->dispatchSubtreeModifiedEvent();
            return oldAttr;
        }
    }

    // attribute with this name not yet in list
    AttrImpl **newAttrs = new AttrImpl* [len+1];
    if (attrs) {
      for (i = 0; i < len; i++)
        newAttrs[i] = attrs[i];
      delete [] attrs;
    }
    attrs = newAttrs;
    attrs[len] = attr;
    attr->ref();
    len++;
    attr->_element = element;
    element->parseAttribute(attr);
    element->setChanged(true);
    element->dispatchAttrAdditionEvent(attr);
    element->dispatchSubtreeModifiedEvent();
    return 0;
}

Attr NamedAttrMapImpl::setIdItem ( AttrImpl *attr, int &exceptioncode )
{
    exceptioncode = 0;
    if (!element) {
        exceptioncode = DOMException::NOT_FOUND_ERR;
        return 0;
    }
    element->checkReadOnly();

    if (attr->_element) {
        exceptioncode = DOMException::INUSE_ATTRIBUTE_ERR;
        return 0;
    }

    uint i;
    for (i = 0; i < len; i++) {
        if (attrs[i]->attrId == attr->attrId) {
            // attribute with this id already in list
            Attr oldAttr = attrs[i];
            attrs[i]->_element = 0;
            attrs[i]->deref();
            attrs[i] = attr;
            attrs[i]->ref();
            attr->_element = element;
            element->parseAttribute(attr);
            element->setChanged(true);
            element->dispatchAttrRemovalEvent(oldAttr.handle());
            element->dispatchAttrAdditionEvent(attrs[i]);
            element->dispatchSubtreeModifiedEvent();
            return oldAttr; // ### check this gets deleted if ref = 0 and it's not assigned to anything
        }
    }

    // attribute with this id not yet in list
    AttrImpl **newAttrs = new AttrImpl* [len+1];
    if (attrs) {
      for (i = 0; i < len; i++)
        newAttrs[i] = attrs[i];
      delete [] attrs;
    }
    attrs = newAttrs;
    attrs[len] = attr;
    attr->ref();
    len++;
    attr->_element = element;
    element->parseAttribute(attr);
    element->setChanged(true);
    element->dispatchAttrAdditionEvent(attr);
    element->dispatchSubtreeModifiedEvent();
    return 0;
}

Node NamedAttrMapImpl::removeNamedItem ( const DOMString &name, int &exceptioncode )
{
    if (element)
        element->checkReadOnly();

    if (!attrs) {
        exceptioncode = DOMException::NOT_FOUND_ERR;
        return 0;
    }

    uint i;
    int found = -1;
    for (i = 0; i < len && found < 0; i++) {
        if (!strcasecmp(attrs[i]->name(),name))
            found = i;
    }
    if (found < 0) {
        exceptioncode = DOMException::NOT_FOUND_ERR;
        return 0;
    }

    Attr ret = attrs[found];
    attrs[found]->_element = 0;
    attrs[found]->deref();
    if (len == 1) {
        delete [] attrs;
        attrs = 0;
        len = 0;
    } else {
        AttrImpl **newAttrs = new AttrImpl* [len-1];
        for (i = 0; i < uint(found); i++)
            newAttrs[i] = attrs[i];
        len--;
        for (; i < len; i++)
            newAttrs[i] = attrs[i+1];
        delete [] attrs;
        attrs = newAttrs;
    }
    DOMString nullStr;
    AttrImpl a(name,nullStr,element->docPtr());
    element->parseAttribute(&a);
    element->setChanged(true);
    element->dispatchAttrRemovalEvent(ret.handle());
    element->dispatchSubtreeModifiedEvent();
    return ret;
}

Attr NamedAttrMapImpl::removeIdItem ( int id )
{
    if (!attrs)
        return 0;

    uint i;
    int found = -1;
    for (i = 0; i < len && found < 0; i++) {
        if (attrs[i]->attrId == id)
            found = i;
    }
    if (found < 0)
        return 0;

    Attr ret = attrs[found];
    attrs[found]->_element = 0;
    attrs[found]->deref();
    if (len == 1) {
        delete [] attrs;
        attrs = 0;
        len = 0;
    } else {
        AttrImpl **newAttrs = new AttrImpl* [len-1];
        for (i = 0; i < uint(found); i++)
            newAttrs[i] = attrs[i];
        len--;
        for (; i < len; i++)
            newAttrs[i] = attrs[i+1];
        delete [] attrs;
        attrs = newAttrs;
    }
    DOMString nullStr;
    AttrImpl a(id,nullStr,element->docPtr());
    element->parseAttribute(&a);
    element->setChanged(true);
    element->dispatchAttrRemovalEvent(ret.handle());
    element->dispatchSubtreeModifiedEvent();
    return ret;
}

NodeImpl *NamedAttrMapImpl::item ( unsigned long index, int &/*exceptioncode*/ ) const
{
    return item(index);
}

NodeImpl *NamedAttrMapImpl::item ( unsigned long index ) const
{
    if (index >= len)
        return 0;
    else
        return attrs[index];
}

void NamedAttrMapImpl::clearAttrs()
{
    if (attrs) {
        uint i;
        for (i = 0; i < len; i++) {
            attrs[i]->_element = 0;
            attrs[i]->deref();
        }
        delete [] attrs;
        attrs = 0;
    }
    len = 0;
}

void NamedAttrMapImpl::insertAttr( AttrImpl *a )
{
    // only add if not already there
    if( !a->attrId || !getIdItem(a->attrId)) {
        AttrImpl** nList = new AttrImpl* [ len+1 ];
        if(attrs) {
            for(uint i = 0; i < len; i++)
                nList[i] = attrs[i];
            delete [] attrs;
        }
        attrs = nList;
        attrs[len++] = a;
        a->ref();
    }
}

Attr NamedAttrMapImpl::removeAttr( AttrImpl *oldAttr, int &exceptioncode )
{
    exceptioncode = 0;
    uint i;
    for (i = 0; i < len; i++) {
        if (attrs[i] == oldAttr) {
            Attr ret = attrs[i];
            attrs[i]->_element = 0;
            attrs[i]->deref();

            if (len == 1) {
                delete [] attrs;
                attrs = 0;
                len = 0;
            }
            else {
                AttrImpl **newAttrs = new AttrImpl* [len-1];
                uint ni;
                for (ni = 0; ni < i; ni++)
                    newAttrs[ni] = attrs[ni];
                len--;
                for (; ni < len; ni++)
                    newAttrs[ni] = attrs[ni+1];
                delete attrs;
                attrs = newAttrs;
            }

            AttrImpl a = oldAttr->attrId ? AttrImpl(oldAttr->attrId,"",element->docPtr()) :
                                       AttrImpl(oldAttr->name(),"",element->docPtr());
            element->parseAttribute(&a);
            element->setChanged(true);
            element->dispatchAttrRemovalEvent(ret.handle());
            element->dispatchSubtreeModifiedEvent();
            return ret;
        }
    }
    exceptioncode = DOMException::NOT_FOUND_ERR;
    return 0;

}

void NamedAttrMapImpl::detachFromElement()
{
    // we allow a NamedAttrMapImpl w/o an element in case someone still has a reference
    // to if after the element gets deleted - but the map is now invalid
    element = 0;
    clearAttrs();
}
