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

#include "config.h"
#include "NodeImpl.h"
#include "DOMStringImpl.h"
#include "MutationEventImpl.h"

using namespace KDOM;

MutationEventImpl::MutationEventImpl(EventImplType identifier) : EventImpl(identifier)
{
    m_newValue = 0;
    m_prevValue = 0;

    m_attrName = 0;
    m_attrChange = 0;

    m_relatedNode = 0;
}

MutationEventImpl::~MutationEventImpl()
{
    if(m_newValue)
        m_newValue->deref();
    if(m_prevValue)
        m_prevValue->deref();
    if(m_attrName)
        m_attrName->deref();
    if(m_relatedNode)
        m_relatedNode->deref();
}

NodeImpl *MutationEventImpl::relatedNode() const
{
    return m_relatedNode;
}

DOMStringImpl *MutationEventImpl::prevValue() const
{
    return m_prevValue;
}

DOMStringImpl *MutationEventImpl::newValue() const
{
    return m_newValue;
}

DOMStringImpl *MutationEventImpl::attrName() const
{
    return m_attrName;
}

unsigned short MutationEventImpl::attrChange() const
{
    return m_attrChange;
}

void MutationEventImpl::initMutationEvent(DOMStringImpl *typeArg, bool canBubbleArg, bool cancelableArg, NodeImpl *relatedNodeArg, DOMStringImpl *prevValueArg, DOMStringImpl *newValueArg, DOMStringImpl *attrNameArg, unsigned short attrChangeArg)
{
    initEvent(typeArg, canBubbleArg, cancelableArg);

    m_attrChange = attrChangeArg;

    KDOM_SAFE_SET(m_prevValue, prevValueArg);
    KDOM_SAFE_SET(m_newValue, newValueArg);
    KDOM_SAFE_SET(m_attrName, attrNameArg);
    KDOM_SAFE_SET(m_relatedNode, relatedNodeArg);
}

// vim:ts=4:noet
