/*
    Copyright (C) 2004 Nikolas Zimmermann <wildfox@kde.org>
				  2004 Rob Buis <buis@kde.org>

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

#include "MutationEventImpl.h"

using namespace KDOM;

MutationEventImpl::MutationEventImpl(EventImplType identifier) : EventImpl(identifier), m_relatedNode(0)
{
	m_attrChange = 0;
}

MutationEventImpl::~MutationEventImpl()
{
}

NodeImpl *MutationEventImpl::relatedNode() const
{
	return m_relatedNode;
}

DOMString MutationEventImpl::prevValue() const
{
	return m_prevValue;
}

DOMString MutationEventImpl::newValue() const
{
	return m_newValue;
}

DOMString MutationEventImpl::attrName() const
{
	return m_attrName;
}

unsigned short MutationEventImpl::attrChange() const
{
	return m_attrChange;
}

void MutationEventImpl::initMutationEvent(const DOMString &typeArg, bool canBubbleArg, bool cancelableArg, NodeImpl *relatedNodeArg, const DOMString &prevValueArg, const DOMString &newValueArg, const DOMString &attrNameArg, unsigned short attrChangeArg)
{
	initEvent(typeArg, canBubbleArg, cancelableArg);

	m_prevValue = prevValueArg;
	m_newValue = newValueArg;
	m_attrName = attrNameArg;
	m_attrChange = attrChangeArg;
	m_relatedNode = relatedNodeArg;
}

// vim:ts=4:noet
