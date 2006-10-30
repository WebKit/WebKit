/*
 * This file is part of the HTML rendering engine for KDE.
 *
 * Copyright (C) 2006 Apple Computer, Inc.
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

#include "config.h"
#include "HitTestResult.h"

#include "Document.h"
#include "Frame.h"
#include "FrameTree.h"
#include "HTMLElement.h"
#include "PlatformScrollBar.h"
#include "RenderObject.h"

namespace WebCore {

HitTestResult::HitTestResult(const IntPoint& point, bool readonly, bool active, bool mouseMove)
    : m_point(point)
    , m_readonly(readonly)
    , m_active(active)
    , m_mouseMove(mouseMove)
{
}

HitTestResult::HitTestResult(const HitTestResult& other)
    : m_innerNode(other.innerNode())
    , m_innerNonSharedNode(other.innerNonSharedNode())
    , m_point(other.point())
    , m_innerURLElement(other.URLElement())
    , m_scrollbar(other.scrollbar())
    , m_readonly(other.readonly())
    , m_active(other.active())
    , m_mouseMove(other.mouseMove())
{
}

HitTestResult::~HitTestResult()
{
}

HitTestResult& HitTestResult::operator=(const HitTestResult& other)
{
    m_innerNode = other.innerNode();
    m_innerNonSharedNode = other.innerNonSharedNode();
    m_point = other.point();
    m_innerURLElement = other.URLElement();
    m_scrollbar = other.scrollbar();
    m_readonly = other.readonly();
    m_active = other.active();
    m_mouseMove = other.mouseMove();
    return *this;
}

void HitTestResult::setInnerNode(Node* n)
{
    m_innerNode = n;
}
    
void HitTestResult::setInnerNonSharedNode(Node* n)
{
    m_innerNonSharedNode = n;
}

void HitTestResult::setURLElement(Element* n) 
{ 
    m_innerURLElement = n; 
}

void HitTestResult::setScrollbar(PlatformScrollbar* s)
{
    m_scrollbar = s;
}

Frame* HitTestResult::targetFrame() const
{
    if (!m_innerURLElement)
        return 0;

    Frame* frame = m_innerURLElement->document()->frame();
    if (!frame)
        return 0;

    return frame->tree()->find(m_innerURLElement->target());
}

IntRect HitTestResult::boundingBox() const
{
    if (m_innerNonSharedNode) {
        RenderObject* renderer = m_innerNonSharedNode->renderer();
        if (renderer)
            return renderer->absoluteBoundingBoxRect();
    }
    
    return IntRect();
}

bool HitTestResult::isSelected() const
{
    if (!m_innerNonSharedNode)
        return false;

    Frame* frame = m_innerNonSharedNode->document()->frame();
    if (!frame)
        return false;

    return frame->isPointInsideSelection(m_point);
}

String HitTestResult::title() const
{
    // Find the title in the nearest enclosing DOM node.
    // For <area> tags in image maps, walk the tree for the <area>, not the <img> using it.
    for (Node* titleNode = m_innerNode.get(); titleNode; titleNode = titleNode->parentNode()) {
        if (titleNode->isHTMLElement()) {
            HTMLElement* titleHTMLNode = static_cast<HTMLElement*>(titleNode);
            String title = titleHTMLNode->title();
            if (!title.isEmpty())
                return title;
        }
    }
    return String();
} 

} // namespace WebCore
