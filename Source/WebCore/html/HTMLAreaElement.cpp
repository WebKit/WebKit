/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2009, 2011 Apple Inc. All rights reserved.
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
 */

#include "config.h"
#include "HTMLAreaElement.h"

#include "AffineTransform.h"
#include "Frame.h"
#include "HTMLImageElement.h"
#include "HTMLMapElement.h"
#include "HTMLParserIdioms.h"
#include "HitTestResult.h"
#include "Path.h"
#include "RenderImage.h"
#include "RenderView.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(HTMLAreaElement);

using namespace HTMLNames;

inline HTMLAreaElement::HTMLAreaElement(const QualifiedName& tagName, Document& document)
    : HTMLAnchorElement(tagName, document)
    , m_lastSize(-1, -1)
    , m_shape(Unknown)
{
    ASSERT(hasTagName(areaTag));
}

Ref<HTMLAreaElement> HTMLAreaElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new HTMLAreaElement(tagName, document));
}

void HTMLAreaElement::parseAttribute(const QualifiedName& name, const AtomString& value)
{
    if (name == shapeAttr) {
        if (equalLettersIgnoringASCIICase(value, "default"))
            m_shape = Default;
        else if (equalLettersIgnoringASCIICase(value, "circle") || equalLettersIgnoringASCIICase(value, "circ"))
            m_shape = Circle;
        else if (equalLettersIgnoringASCIICase(value, "poly") || equalLettersIgnoringASCIICase(value, "polygon"))
            m_shape = Poly;
        else {
            // The missing value default is the rectangle state.
            m_shape = Rect;
        }
        invalidateCachedRegion();
    } else if (name == coordsAttr) {
        m_coords = parseHTMLListOfOfFloatingPointNumberValues(value.string());
        invalidateCachedRegion();
    } else if (name == altAttr) {
        // Do nothing.
    } else
        HTMLAnchorElement::parseAttribute(name, value);
}

void HTMLAreaElement::invalidateCachedRegion()
{
    m_lastSize = LayoutSize(-1, -1);
}

bool HTMLAreaElement::mapMouseEvent(LayoutPoint location, const LayoutSize& size, HitTestResult& result)
{
    if (m_lastSize != size) {
        m_region = makeUnique<Path>(getRegion(size));
        m_lastSize = size;
    }

    if (!m_region->contains(location))
        return false;
    
    result.setInnerNode(this);
    result.setURLElement(this);
    return true;
}

// FIXME: We should use RenderElement* instead of RenderObject* once we upstream iOS's DOMUIKitExtensions.{h, mm}.
Path HTMLAreaElement::computePath(RenderObject* obj) const
{
    if (!obj)
        return Path();
    
    // FIXME: This doesn't work correctly with transforms.
    FloatPoint absPos = obj->localToAbsolute();

    // Default should default to the size of the containing object.
    LayoutSize size = m_lastSize;
    if (m_shape == Default)
        size = obj->absoluteOutlineBounds().size();
    
    Path p = getRegion(size);
    float zoomFactor = obj->style().effectiveZoom();
    if (zoomFactor != 1.0f) {
        AffineTransform zoomTransform;
        zoomTransform.scale(zoomFactor);
        p.transform(zoomTransform);
    }

    p.translate(toFloatSize(absPos));
    return p;
}

Path HTMLAreaElement::computePathForFocusRing(const LayoutSize& elementSize) const
{
    return getRegion(m_shape == Default ? elementSize : m_lastSize);
}

// FIXME: Use RenderElement* instead of RenderObject* once we upstream iOS's DOMUIKitExtensions.{h, mm}.
LayoutRect HTMLAreaElement::computeRect(RenderObject* obj) const
{
    return enclosingLayoutRect(computePath(obj).fastBoundingRect());
}

Path HTMLAreaElement::getRegion(const LayoutSize& size) const
{
    if (m_coords.isEmpty() && m_shape != Default)
        return Path();

    LayoutUnit width = size.width();
    LayoutUnit height = size.height();

    // If element omits the shape attribute, select shape based on number of coordinates.
    Shape shape = m_shape;
    if (shape == Unknown) {
        if (m_coords.size() == 3)
            shape = Circle;
        else if (m_coords.size() == 4)
            shape = Rect;
        else if (m_coords.size() >= 6)
            shape = Poly;
    }

    Path path;
    switch (shape) {
        case Poly:
            if (m_coords.size() >= 6) {
                int numPoints = m_coords.size() / 2;
                path.moveTo(FloatPoint(m_coords[0], m_coords[1]));
                for (int i = 1; i < numPoints; ++i)
                    path.addLineTo(FloatPoint(m_coords[i * 2], m_coords[i * 2 + 1]));
                path.closeSubpath();
            }
            break;
        case Circle:
            if (m_coords.size() >= 3) {
                double radius = m_coords[2];
                if (radius > 0)
                    path.addEllipse(FloatRect(m_coords[0] - radius, m_coords[1] - radius, 2 * radius, 2 * radius));
            }
            break;
        case Rect:
            if (m_coords.size() >= 4) {
                double x0 = m_coords[0];
                double y0 = m_coords[1];
                double x1 = m_coords[2];
                double y1 = m_coords[3];
                path.addRect(FloatRect(x0, y0, x1 - x0, y1 - y0));
            }
            break;
        case Default:
            path.addRect(FloatRect(0, 0, width, height));
            break;
        case Unknown:
            break;
    }

    return path;
}

HTMLImageElement* HTMLAreaElement::imageElement() const
{
    RefPtr<Node> mapElement = parentNode();
    if (!is<HTMLMapElement>(mapElement))
        return nullptr;
    
    return downcast<HTMLMapElement>(*mapElement).imageElement();
}

bool HTMLAreaElement::isKeyboardFocusable(KeyboardEvent*) const
{
    return isFocusable();
}
    
bool HTMLAreaElement::isMouseFocusable() const
{
    return isFocusable();
}

bool HTMLAreaElement::isFocusable() const
{
    RefPtr<HTMLImageElement> image = imageElement();
    if (!image || !image->isVisibleWithoutResolvingFullStyle())
        return false;

    return supportsFocus() && tabIndexSetExplicitly().value_or(0) >= 0;
}
    
void HTMLAreaElement::setFocus(bool shouldBeFocused, FocusVisibility visibility)
{
    if (focused() == shouldBeFocused)
        return;

    HTMLAnchorElement::setFocus(shouldBeFocused, visibility);

    RefPtr<HTMLImageElement> imageElement = this->imageElement();
    if (!imageElement)
        return;

    auto* renderer = imageElement->renderer();
    if (!is<RenderImage>(renderer))
        return;

    downcast<RenderImage>(*renderer).areaElementFocusChanged(this);
}

RefPtr<Element> HTMLAreaElement::focusAppearanceUpdateTarget()
{
    if (!isFocusable())
        return nullptr;
    return imageElement();
}
    
bool HTMLAreaElement::supportsFocus() const
{
    // If the AREA element was a link, it should support focus.
    // The inherited method is not used because it assumes that a render object must exist 
    // for the element to support focus. AREA elements do not have render objects.
    return isLink();
}

String HTMLAreaElement::target() const
{
    return attributeWithoutSynchronization(targetAttr);
}

}
