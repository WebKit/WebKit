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
 * $Id$
 */


#include "dom_xml.h"
#include "dom_string.h"
#include "dom_textimpl.h"
#include "dom_xmlimpl.h"
using namespace DOM;


CDATASection::CDATASection()
{
}

CDATASection::CDATASection(const CDATASection &) : Text()
{
}

CDATASection &CDATASection::operator = (const Node &other)
{
    if(other.nodeType() != CDATA_SECTION_NODE)
    {
	impl = 0;
	return *this;
    }
    Node::operator =(other);
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
    if(other.nodeType() != ENTITY_NODE)
    {
	impl = 0;
	return *this;
    }
    Node::operator =(other);
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
    if (impl) return ((EntityImpl*)impl)->publicId();
    return DOMString();
}

DOMString Entity::systemId() const
{
    if (impl) return ((EntityImpl*)impl)->systemId();
    return DOMString();
}

DOMString Entity::notationName() const
{
    if (impl) return ((EntityImpl*)impl)->notationName();
    return DOMString();
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
    if(other.nodeType() != ENTITY_REFERENCE_NODE)
    {
	impl = 0;
	return *this;
    }
    Node::operator =(other);
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
    if(other.nodeType() != NOTATION_NODE)
    {
	impl = 0;
	return *this;
    }
    Node::operator =(other);
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
    if (impl) return ((EntityImpl*)impl)->publicId();
    return DOMString();
}

DOMString Notation::systemId() const
{
    if (impl) return ((EntityImpl*)impl)->systemId();
    return DOMString();
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
    if(other.nodeType() != PROCESSING_INSTRUCTION_NODE)
    {
	impl = 0;
	return *this;
    }
    Node::operator =(other);
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
    if (impl) return ((ProcessingInstructionImpl*)impl)->target();
    return DOMString();
}

DOMString ProcessingInstruction::data() const
{
    if (impl) return ((ProcessingInstructionImpl*)impl)->data();
    return DOMString();
}

void ProcessingInstruction::setData( const DOMString &_data )
{
    if (impl) ((ProcessingInstructionImpl*)impl)->setData(_data);
}

ProcessingInstruction::ProcessingInstruction(ProcessingInstructionImpl *i) : Node(i)
{
}

StyleSheet ProcessingInstruction::sheet() const
{
    if (impl) return ((ProcessingInstructionImpl*)impl)->sheet();
    return 0;
}


