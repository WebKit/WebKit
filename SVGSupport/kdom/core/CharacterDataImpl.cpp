/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
				  2004, 2005 Rob Buis <buis@kde.org>

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

#include "kdomevents.h"
#include "DocumentImpl.h"
#include "DOMConfigurationImpl.h"
#include "CharacterData.h"
#include "CharacterDataImpl.h"
#include "MutationEventImpl.h"

using namespace KDOM;

CharacterDataImpl::CharacterDataImpl(DocumentImpl *doc) : NodeImpl(doc)
{
	str = 0;
}

CharacterDataImpl::~CharacterDataImpl()
{
	if(str)
		str->deref();
}

void CharacterDataImpl::checkCharDataOperation(CharacterDataImpl *node, const unsigned long offset)
{
	if(!str)
		return;

	// INDEX_SIZE_ERR: Raised if the specified offset is negative or
	//				   greater than the number of 16-bit units in data.
	if(offset > str->length())
		throw new DOMExceptionImpl(INDEX_SIZE_ERR);

	// NO_MODIFICATION_ALLOWED_ERR: Raised if this node is readonly
	if(node->isReadOnly())
		throw new DOMExceptionImpl(NO_MODIFICATION_ALLOWED_ERR);
}

DOMString CharacterDataImpl::textContent() const
{
	return nodeValue();
}

DOMString CharacterDataImpl::nodeValue() const
{
	return DOMString(str);
}

void CharacterDataImpl::setNodeValue(const DOMString &nodeValue)
{
	// NO_MODIFICATION_ALLOWED_ERR: Raised if this node is readonly
	if(isReadOnly())
		throw new DOMExceptionImpl(NO_MODIFICATION_ALLOWED_ERR);

	KDOM_SAFE_SET(str, nodeValue.implementation());
}

DOMStringImpl *CharacterDataImpl::data() const
{
	return str;
}

DOMStringImpl *CharacterDataImpl::substringData(unsigned long offset, unsigned long count)
{
	if(!str)
		return 0;

	if((long) count < 0)
		throw new DOMExceptionImpl(INDEX_SIZE_ERR);

	checkCharDataOperation(this, offset);
	return str->substring(offset, count);
}

void CharacterDataImpl::appendData(DOMStringImpl *arg)
{
	if(!str)
		return;

	// NO_MODIFICATION_ALLOWED_ERR: Raised if this node is readonly
	if(isReadOnly())
		throw new DOMExceptionImpl(NO_MODIFICATION_ALLOWED_ERR);

	DOMString prevValue = nodeValue();
	str->append(arg);

	dispatchModifiedEvent(prevValue);
}

void CharacterDataImpl::insertData(unsigned long offset, DOMStringImpl *arg)
{
	if(!str)
		return;
		
	checkCharDataOperation(this, offset);
	
	DOMString prevValue = nodeValue();
	str->insert(arg, offset);

	dispatchModifiedEvent(prevValue);
}

void CharacterDataImpl::deleteData(unsigned long offset, unsigned long count)
{
	if(!str)
		return;
		
	if((long) count < 0)
		throw new DOMExceptionImpl(INDEX_SIZE_ERR);

	checkCharDataOperation(this, offset);
	
	DOMString prevValue = nodeValue();
	str->remove(offset, count);

	dispatchModifiedEvent(prevValue);
}

void CharacterDataImpl::replaceData(unsigned long offset, unsigned long count, DOMStringImpl *arg)
{
	if(!str)
		return;
		
	if((long) count < 0)
		throw new DOMExceptionImpl(INDEX_SIZE_ERR);

	checkCharDataOperation(this, offset);

	DOMString prevValue = nodeValue();
	
	unsigned long realCount;
	if(offset + count > str->length())
		realCount = str->length() - offset;
	else
		realCount = count;

	str->remove(offset, realCount);
	str->insert(arg, offset);

	dispatchModifiedEvent(prevValue);
}

unsigned long CharacterDataImpl::length() const
{
	if(!str)
		return 0;
		
	return str->length();
}

void CharacterDataImpl::setData(DOMStringImpl *data)
{
	if(!str || data == str)
		return;

	// NO_MODIFICATION_ALLOWED_ERR: Raised if this node is readonly
	if(isReadOnly())
		throw new DOMExceptionImpl(NO_MODIFICATION_ALLOWED_ERR);

	DOMString prevValue = nodeValue();
	KDOM_SAFE_SET(str, data);

	dispatchModifiedEvent(prevValue);
}

void CharacterDataImpl::dispatchModifiedEvent(const DOMString &prevValue)
{
	if(parentNode())
		parentNode()->childrenChanged();

	if(!ownerDocument()->hasListenerType(DOMCHARACTERDATAMODIFIED_EVENT))
		return;

	MutationEventImpl *event = static_cast<MutationEventImpl *>(ownerDocument()->createEvent("MutationEvents"));
	event->ref();

	event->initMutationEvent("DOMCharacterDataModified", true, false, 0, prevValue, nodeValue(), DOMString(), 0);

	dispatchEvent(event);
	dispatchSubtreeModifiedEvent();

	event->deref();
}

void CharacterDataImpl::normalize()
{
	NodeImpl::normalize();
	setData(ownerDocument()->domConfig()->normalizeCharacters(data()));
}

bool CharacterDataImpl::containsOnlyWhitespace() const
{
	return str->containsOnlyWhitespace();
}

// vim:ts=4:noet
