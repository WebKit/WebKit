/*
   Copyright (C) 2006 Apple Computer, Inc.

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
#include "MouseEventWithHitTestResults.h"

// Would TargetedMouseEvent be a better name?

namespace WebCore {

static inline Element* targetElement(Node* node)
{
    if (!node)
        return 0;
    Node* parent = node->parent();
    if (!parent || !parent->isElementNode())
        return 0;
    return static_cast<Element*>(parent);
}

MouseEventWithHitTestResults::MouseEventWithHitTestResults(const PlatformMouseEvent& event,
        PassRefPtr<Node> node, bool isOverLink)
    : m_event(event)
    , m_targetNode(node)
    , m_targetElement(targetElement(m_targetNode.get()))
    , m_isOverLink(isOverLink)
{
}
        
Node* MouseEventWithHitTestResults::targetNode() const
{
    if (m_targetElement && !m_targetNode->inDocument() && m_targetElement->inDocument())
        return m_targetElement.get();
    return m_targetNode.get();
}

}
