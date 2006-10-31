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
#include "SelectionController.h"

namespace WebCore {

HitTestResult::HitTestResult(const IntPoint& point)
    : m_point(point)
{
}

HitTestResult::HitTestResult(const HitTestResult& other)
    : m_innerNode(other.innerNode())
    , m_innerNonSharedNode(other.innerNonSharedNode())
    , m_point(other.point())
    , m_innerURLElement(other.URLElement())
    , m_scrollbar(other.scrollbar())
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

    return frame->selectionController()->contains(m_point);
}

String HitTestResult::spellingToolTip() const
{
    // Return the tool tip string associated with this point
    // FIXME: At the moment this returns the same hardwired string for all bad grammar rects. This needs to
    // instead return an instance-specific string that was stashed away when the bad grammar was discovered.
    // Checking it in now just to modularize the work a little.
    Vector<IntRect> rects = m_innerNonSharedNode->document()->renderedRectsForMarkers(DocumentMarker::Grammar);
    unsigned count = rects.size();
    for (unsigned index = 0; index < count; ++index)
        if (rects[index].contains(m_point))
            return "Questionable grammar!";

    return String();
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
