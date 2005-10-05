/*
 * This file is part of the KDE libraries
 *
 * Copyright (C) 2005 Frans Englich     <frans.englich@telia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB. If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#include "config.h"
#include <q3valuelist.h>

#include "kdom/Shared.h"
#include "NodeImpl.h"
#include "kdomxpointer.h"
#include "DocumentImpl.h"
#include "DOMStringImpl.h"
#include "PointerPartImpl.h"
#include "XPointerResultImpl.h"
#include "XPointerExceptionImpl.h"
#include "XPointerExpressionImpl.h"

using namespace KDOM;
using namespace KDOM::XPointer;

XPointerExpressionImpl::XPointerExpressionImpl(DOMStringImpl *raw, NodeImpl *r, DocumentImpl *context)
: Shared(), m_isShortHand(false), m_pointer(raw), m_relatedNode(r), m_context(context)
{
    Q_ASSERT((m_pointer && !m_pointer->isEmpty()));
    Q_ASSERT(m_context);

    if(m_relatedNode)
        m_relatedNode->ref();

    m_pointer->ref();
    m_context->ref();
}

XPointerExpressionImpl::~XPointerExpressionImpl()
{
    List::const_iterator it;
    List::const_iterator end(m_parts.constEnd());

    for(it = m_parts.constBegin(); it != end; ++it)
    {
        PointerPartImpl *part = (*it);

        if(part)
            part->deref();
    }

    if(m_relatedNode)
        m_relatedNode->deref();

    Q_ASSERT(m_context);
    m_context->deref();

    Q_ASSERT(m_pointer);
    m_pointer->deref();
}

XPointerResultImpl *XPointerExpressionImpl::evaluate() const
{
    List::const_iterator it;
    List::const_iterator end(m_parts.constEnd());

    for(it = m_parts.constBegin(); it != end; ++it)
    {
        PointerPartImpl *part = (*it);
        if(!part)
            continue;
            
        XPointerResultImpl *result = part->evaluate(static_cast<NodeImpl *>(m_context));
        if(!result || result->resultType() == NO_MATCH && !isShortHand())
            continue;
        
        return result;
    }

    return new XPointerResultImpl(NO_MATCH);
}

DOMStringImpl *XPointerExpressionImpl::string() const
{
    return m_pointer;
}

void XPointerExpressionImpl::setIsShortHand(bool state)
{
    m_isShortHand = state;
}

bool XPointerExpressionImpl::isShortHand() const
{
    return m_isShortHand;
}

void XPointerExpressionImpl::appendPart(PointerPartImpl *part)
{
    m_parts.append(part);
}

XPointerExpressionImpl::List XPointerExpressionImpl::pointerParts() const
{
    return m_parts;
}

// vim:ts=4:noet
