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

#include "dom_exception.h"
#include "dom_string.h"

#include "dom_text.h"
#include "dom_textimpl.h"
using namespace DOM;


CharacterData::CharacterData() : Node()
{
}

CharacterData::CharacterData(const CharacterData &other) : Node(other)
{
}

CharacterData &CharacterData::operator = (const Node &other)
{
    if( other.nodeType() != CDATA_SECTION_NODE &&
	other.nodeType() != TEXT_NODE &&
	other.nodeType() != COMMENT_NODE )
    {
	impl = 0;
	return *this;
    }
    Node::operator =(other);
    return *this;
}

CharacterData &CharacterData::operator = (const CharacterData &other)
{
    Node::operator =(other);
    return *this;
}

CharacterData::~CharacterData()
{
}

DOMString CharacterData::data() const
{
    if(!impl) return DOMString();
    return ((CharacterDataImpl *)impl)->data();
}

void CharacterData::setData( const DOMString &str )
{
    if ( impl )
	((CharacterDataImpl *)impl)->setData(str);
}

unsigned long CharacterData::length() const
{
    if ( impl )
	return ((CharacterDataImpl *)impl)->length();
    return 0;
}

DOMString CharacterData::substringData( const unsigned long offset, const unsigned long count )
{
    int exceptioncode = DOMException::HIERARCHY_REQUEST_ERR;
    DOMString str;
    if ( impl )
	str = ((CharacterDataImpl *)impl)->substringData(offset, count, exceptioncode);
    if ( exceptioncode )
	throw DOMException( exceptioncode );
    return str;
}

void CharacterData::appendData( const DOMString &arg )
{
    if ( impl )
	((CharacterDataImpl *)impl)->appendData(arg);
}

void CharacterData::insertData( const unsigned long offset, const DOMString &arg )
{
    int exceptioncode = DOMException::HIERARCHY_REQUEST_ERR;
    if ( impl )
	((CharacterDataImpl *)impl)->insertData(offset, arg, exceptioncode);
    if ( exceptioncode )
	throw DOMException( exceptioncode );
}

void CharacterData::deleteData( const unsigned long offset, const unsigned long count )
{
    int exceptioncode = DOMException::HIERARCHY_REQUEST_ERR;
    if ( impl )
	((CharacterDataImpl *)impl)->deleteData(offset, count, exceptioncode);
    if ( exceptioncode )
	throw DOMException( exceptioncode );
}

void CharacterData::replaceData( const unsigned long offset, const unsigned long count, const DOMString &arg )
{
    int exceptioncode = DOMException::HIERARCHY_REQUEST_ERR;
    if ( impl )
	((CharacterDataImpl *)impl)->replaceData(offset, count, arg, exceptioncode);
    if ( exceptioncode )
	throw DOMException( exceptioncode );
}

CharacterData::CharacterData(CharacterDataImpl *i) : Node(i)
{
}

// ---------------------------------------------------------------------------

Comment::Comment() : CharacterData()
{
}

Comment::Comment(const Comment &other) : CharacterData(other)
{
}

Comment &Comment::operator = (const Node &other)
{
    if(other.nodeType() != COMMENT_NODE)
    {
	impl = 0;
	return *this;
    }
    Node::operator =(other);
    return *this;
}

Comment &Comment::operator = (const Comment &other)
{
    CharacterData::operator =(other);
    return *this;
}

Comment::~Comment()
{
}

Comment::Comment(CommentImpl *i) : CharacterData(i)
{
}

// ----------------------------------------------------------------------------

Text::Text()
{
}

Text::Text(const Text &other) : CharacterData(other)
{
}

Text &Text::operator = (const Node &other)
{
    if(other.nodeType() != TEXT_NODE  &&
       other.nodeType() != CDATA_SECTION_NODE)
    {
	impl = 0;
	return *this;
    }
    Node::operator =(other);
    return *this;
}

Text &Text::operator = (const Text &other)
{
    Node::operator =(other);
    return *this;
}

Text::~Text()
{
}

Text Text::splitText( const unsigned long offset )
{
    int exceptioncode = DOMException::NO_MODIFICATION_ALLOWED_ERR;
    TextImpl *newText = 0;
    if ( impl )
	newText = static_cast<TextImpl *>(impl)->splitText(offset, exceptioncode );
    if ( exceptioncode )
	throw DOMException( exceptioncode );
    return newText;
}

Text::Text(TextImpl *i) : CharacterData(i)
{
}
