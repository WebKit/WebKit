/**
 * This file is part of the DOM implementation for KDE.
 *
 * (C) 1999 Lars Knoll (knoll@kde.org)
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
 */


#include "dom/dom_xml.h"
#include "dom/dom_exception.h"
#include "xml/dom_textimpl.h"
#include "xml/dom_xmlimpl.h"

using namespace DOM;

CDATASection::CDATASection()
{
}

CDATASection::CDATASection(const CDATASection &) : Text()
{
}

CDATASection &CDATASection::operator = (const Node &other)
{
    NodeImpl* ohandle = other.handle();
    if ( impl != ohandle ) {
    if (!ohandle || ohandle->nodeType() != CDATA_SECTION_NODE) {
	    if ( impl ) impl->deref();
	impl = 0;
	} else {
    Node::operator =(other);
	}
    }
    return *this;
}

CDATASection &CDATASection::operator = (const CDATASection &other)
{
    Node::operator =(other);
    return *this;
}

CDATASection::~CDATASection()
{
}

CDATASection::CDATASection(CDATASectionImpl *i) : Text(i)
{
}

// ----------------------------------------------------------------------------
Entity::Entity()
{
}

Entity::Entity(const Entity &) : Node()
{
}

Entity &Entity::operator = (const Node &other)
{
    NodeImpl* ohandle = other.handle();
    if ( impl != ohandle ) {
    if (!ohandle || ohandle->nodeType() != ENTITY_NODE) {
	    if ( impl ) impl->deref();
	impl = 0;
	} else {
    Node::operator =(other);
	}
    }
    return *this;
}

Entity &Entity::operator = (const Entity &other)
{
    Node::operator =(other);
    return *this;
}

Entity::~Entity()
{
}

DOMString Entity::publicId() const
{
    if (!impl)
	return DOMString(); // ### enable throw DOMException(DOMException::NOT_FOUND_ERR);

    return ((EntityImpl*)impl)->publicId();
}

DOMString Entity::systemId() const
{
    if (!impl)
	return DOMString(); // ### enable throw DOMException(DOMException::NOT_FOUND_ERR);

    return ((EntityImpl*)impl)->systemId();
}

DOMString Entity::notationName() const
{
    if (!impl)
	return DOMString(); // ### enable throw DOMException(DOMException::NOT_FOUND_ERR);

    return ((EntityImpl*)impl)->notationName();
}

Entity::Entity(EntityImpl *i) : Node(i)
{
}

// ----------------------------------------------------------------------------

EntityReference::EntityReference()
{
}

EntityReference::EntityReference(const EntityReference &) : Node()
{
}

EntityReference &EntityReference::operator = (const Node &other)
{
    NodeImpl* ohandle = other.handle();
    if ( impl != ohandle ) {
    if (!ohandle || ohandle->nodeType() != ENTITY_REFERENCE_NODE) {
	    if ( impl ) impl->deref();
	impl = 0;
	} else {
    Node::operator =(other);
	}
    }
    return *this;
}

EntityReference &EntityReference::operator = (const EntityReference &other)
{
    Node::operator =(other);
    return *this;
}

EntityReference::~EntityReference()
{
}

EntityReference::EntityReference(EntityReferenceImpl *i) : Node(i)
{
}

// ----------------------------------------------------------------------------

Notation::Notation()
{
}

Notation::Notation(const Notation &) : Node()
{
}

Notation &Notation::operator = (const Node &other)
{
    NodeImpl* ohandle = other.handle();
    if ( impl != ohandle ) {
    if (!ohandle || ohandle->nodeType() != NOTATION_NODE) {
	    if ( impl ) impl->deref();
	impl = 0;
	} else {
    Node::operator =(other);
	}
    }
    return *this;
}

Notation &Notation::operator = (const Notation &other)
{
    Node::operator =(other);
    return *this;
}

Notation::~Notation()
{
}

DOMString Notation::publicId() const
{
    if (!impl)
	return DOMString(); // ### enable throw DOMException(DOMException::NOT_FOUND_ERR);

    return ((NotationImpl*)impl)->publicId();
}

DOMString Notation::systemId() const
{
    if (!impl)
	return DOMString(); // ### enable throw DOMException(DOMException::NOT_FOUND_ERR);

    return ((NotationImpl*)impl)->systemId();
}

Notation::Notation(NotationImpl *i) : Node(i)
{
}


// ----------------------------------------------------------------------------

ProcessingInstruction::ProcessingInstruction()
{
}

ProcessingInstruction::ProcessingInstruction(const ProcessingInstruction &)
    : Node()
{
}

ProcessingInstruction &ProcessingInstruction::operator = (const Node &other)
{
    NodeImpl* ohandle = other.handle();
    if ( impl != ohandle ) {
    if (!ohandle || ohandle->nodeType() != PROCESSING_INSTRUCTION_NODE) {
	    if ( impl ) impl->deref();
	impl = 0;
	} else {
    Node::operator =(other);
	}
    }
    return *this;
}

ProcessingInstruction &ProcessingInstruction::operator = (const ProcessingInstruction &other)
{
    Node::operator =(other);
    return *this;
}

ProcessingInstruction::~ProcessingInstruction()
{
}

DOMString ProcessingInstruction::target() const
{
    if (!impl)
	return DOMString(); // ### enable throw DOMException(DOMException::NOT_FOUND_ERR);

    return ((ProcessingInstructionImpl*)impl)->target();
}

DOMString ProcessingInstruction::data() const
{
    if (!impl)
	return DOMString(); // ### enable throw DOMException(DOMException::NOT_FOUND_ERR);

    return ((ProcessingInstructionImpl*)impl)->data();
}

void ProcessingInstruction::setData( const DOMString &_data )
{
    if (!impl)
	return; // ### enable throw DOMException(DOMException::NOT_FOUND_ERR);

    int exceptioncode = 0;
    ((ProcessingInstructionImpl*)impl)->setData(_data, exceptioncode);
    if (exceptioncode)
	throw DOMException(exceptioncode);
}

ProcessingInstruction::ProcessingInstruction(ProcessingInstructionImpl *i) : Node(i)
{
}

StyleSheet ProcessingInstruction::sheet() const
{
    if (impl) return ((ProcessingInstructionImpl*)impl)->sheet();
    return 0;
}


