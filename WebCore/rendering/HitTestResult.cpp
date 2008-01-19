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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
*/

#include "config.h"
#include "HitTestResult.h"

#include "CSSHelper.h"
#include "Document.h"
#include "Frame.h"
#include "FrameTree.h"
#include "HTMLAnchorElement.h"
#include "HTMLElement.h"
#include "HTMLImageElement.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "KURL.h"
#include "PlatformScrollBar.h"
#include "RenderImage.h"
#include "RenderObject.h"
#include "SelectionController.h"

#if ENABLE(SVG)
#include "SVGNames.h"
#include "XLinkNames.h"
#endif

namespace WebCore {

using namespace HTMLNames;

HitTestResult::HitTestResult(const IntPoint& point)
    : m_point(point)
{
}

HitTestResult::HitTestResult(const HitTestResult& other)
    : m_innerNode(other.innerNode())
    , m_innerNonSharedNode(other.innerNonSharedNode())
    , m_point(other.point())
    , m_localPoint(other.localPoint())
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
    m_localPoint = other.localPoint();
    m_innerURLElement = other.URLElement();
    m_scrollbar = other.scrollbar();
    return *this;
}

void HitTestResult::setToNonShadowAncestor()
{
    Node* node = innerNode();
    if (node)
        node = node->shadowAncestorNode();
    setInnerNode(node);
    node = innerNonSharedNode();
    if (node)
        node = node->shadowAncestorNode();
    setInnerNonSharedNode(node);
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
    // Return the tool tip string associated with this point, if any. Only markers associated with bad grammar
    // currently supply strings, but maybe someday markers associated with misspelled words will also.
    if (!m_innerNonSharedNode)
        return String();
    
     DocumentMarker* marker = m_innerNonSharedNode->document()->markerContainingPoint(m_point, DocumentMarker::Grammar);
    if (!marker)
        return String();

    return marker->description;
}

String HitTestResult::title() const
{
    // Find the title in the nearest enclosing DOM node.
    // For <area> tags in image maps, walk the tree for the <area>, not the <img> using it.
    for (Node* titleNode = m_innerNode.get(); titleNode; titleNode = titleNode->parentNode()) {
        if (titleNode->isElementNode()) {
            String title = static_cast<Element*>(titleNode)->title();
            if (!title.isEmpty())
                return title;
        }
    }
    return String();
}

String displayString(const String& string, const Node* node)
{
    if (!node)
        return string;
    String copy(string);
    copy.replace('\\', node->document()->backslashAsCurrencySymbol());
    return copy;
}

String HitTestResult::altDisplayString() const
{
    if (!m_innerNonSharedNode)
        return String();
    
    if (m_innerNonSharedNode->hasTagName(imgTag)) {
        HTMLImageElement* image = static_cast<HTMLImageElement*>(m_innerNonSharedNode.get());
        return displayString(image->alt(), m_innerNonSharedNode.get());
    }
    
    if (m_innerNonSharedNode->hasTagName(inputTag)) {
        HTMLInputElement* input = static_cast<HTMLInputElement*>(m_innerNonSharedNode.get());
        return displayString(input->alt(), m_innerNonSharedNode.get());
    }
    
    return String();
}

Image* HitTestResult::image() const
{
    if (!m_innerNonSharedNode)
        return 0;
    
    RenderObject* renderer = m_innerNonSharedNode->renderer();
    if (renderer && renderer->isImage()) {
        RenderImage* image = static_cast<WebCore::RenderImage*>(renderer);
        if (image->cachedImage() && !image->cachedImage()->errorOccurred())
            return image->cachedImage()->image();
    }

    return 0;
}

IntRect HitTestResult::imageRect() const
{
    if (!image())
        return IntRect();
    return m_innerNonSharedNode->renderer()->absoluteContentBox();
}

KURL HitTestResult::absoluteImageURL() const
{
    if (!(m_innerNonSharedNode && m_innerNonSharedNode->document()))
        return KURL();

    if (!(m_innerNonSharedNode->renderer() && m_innerNonSharedNode->renderer()->isImage()))
        return KURL();

    AtomicString urlString;
    if (m_innerNonSharedNode->hasTagName(imgTag) || m_innerNonSharedNode->hasTagName(inputTag))
        urlString = static_cast<Element*>(m_innerNonSharedNode.get())->getAttribute(srcAttr);
#if ENABLE(SVG)
    else if (m_innerNonSharedNode->hasTagName(SVGNames::imageTag))
        urlString = static_cast<Element*>(m_innerNonSharedNode.get())->getAttribute(XLinkNames::hrefAttr);
#endif
    else if (m_innerNonSharedNode->hasTagName(objectTag))
        urlString = static_cast<Element*>(m_innerNonSharedNode.get())->getAttribute(dataAttr);
    else
        return KURL();
    
    return KURL(m_innerNonSharedNode->document()->completeURL(parseURL(urlString).deprecatedString()));
}

KURL HitTestResult::absoluteLinkURL() const
{
    if (!(m_innerURLElement && m_innerURLElement->document()))
        return KURL();

    AtomicString urlString;
    if (m_innerURLElement->hasTagName(aTag) || m_innerURLElement->hasTagName(areaTag) || m_innerURLElement->hasTagName(linkTag))
        urlString = m_innerURLElement->getAttribute(hrefAttr);
#if ENABLE(SVG)
    else if (m_innerURLElement->hasTagName(SVGNames::aTag))
        urlString = m_innerURLElement->getAttribute(XLinkNames::hrefAttr);
#endif
    else
        return KURL();

    return KURL(m_innerURLElement->document()->completeURL(parseURL(urlString).deprecatedString()));
}

bool HitTestResult::isLiveLink() const
{
    if (!(m_innerURLElement && m_innerURLElement->document()))
        return false;

    if (m_innerURLElement->hasTagName(aTag))
        return static_cast<HTMLAnchorElement*>(m_innerURLElement.get())->isLiveLink();
#if ENABLE(SVG)
    if (m_innerURLElement->hasTagName(SVGNames::aTag))
        return m_innerURLElement->isLink();
#endif
    
    return false;
}

String HitTestResult::titleDisplayString() const
{
    if (!m_innerURLElement)
        return String();
    
    return displayString(m_innerURLElement->title(), m_innerURLElement.get());
}

String HitTestResult::textContent() const
{
    if (!m_innerURLElement)
        return String();
    return m_innerURLElement->textContent();
}

// FIXME: This function needs a better name and may belong in a different class. It's not
// really isContentEditable(); it's more like needsEditingContextMenu(). In many ways, this
// function would make more sense in the ContextMenu class, except that WebElementDictionary 
// hooks into it. Anyway, we should architect this better. 
bool HitTestResult::isContentEditable() const
{
    if (!m_innerNonSharedNode)
        return false;

    if (m_innerNonSharedNode->hasTagName(textareaTag) || m_innerNonSharedNode->hasTagName(isindexTag))
        return true;

    if (m_innerNonSharedNode->hasTagName(inputTag))
        return static_cast<HTMLInputElement*>(m_innerNonSharedNode.get())->isTextField();

    return m_innerNonSharedNode->isContentEditable();
}

} // namespace WebCore
