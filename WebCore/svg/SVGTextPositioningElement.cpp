/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Rob Buis <buis@kde.org>
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

#if ENABLE(SVG)
#include "SVGTextPositioningElement.h"

#include "Attribute.h"
#include "RenderObject.h"
#include "RenderSVGResource.h"
#include "SVGLengthList.h"
#include "SVGNames.h"
#include "SVGNumberList.h"

namespace WebCore {

SVGTextPositioningElement::SVGTextPositioningElement(const QualifiedName& tagName, Document* document)
    : SVGTextContentElement(tagName, document)
    , m_x(SVGLengthList::create(SVGNames::xAttr))
    , m_y(SVGLengthList::create(SVGNames::yAttr))
    , m_dx(SVGLengthList::create(SVGNames::dxAttr))
    , m_dy(SVGLengthList::create(SVGNames::dyAttr))
    , m_rotate(SVGNumberList::create(SVGNames::rotateAttr))
{
}

void SVGTextPositioningElement::parseMappedAttribute(Attribute* attr)
{
    if (attr->name() == SVGNames::xAttr)
        xBaseValue()->parse(attr->value(), LengthModeWidth);
    else if (attr->name() == SVGNames::yAttr)
        yBaseValue()->parse(attr->value(), LengthModeHeight);
    else if (attr->name() == SVGNames::dxAttr)
        dxBaseValue()->parse(attr->value(), LengthModeWidth);
    else if (attr->name() == SVGNames::dyAttr)
        dyBaseValue()->parse(attr->value(), LengthModeHeight);
    else if (attr->name() == SVGNames::rotateAttr)
        rotateBaseValue()->parse(attr->value());
    else
        SVGTextContentElement::parseMappedAttribute(attr);
}

void SVGTextPositioningElement::svgAttributeChanged(const QualifiedName& attrName)
{
    SVGTextContentElement::svgAttributeChanged(attrName);

    if (attrName == SVGNames::xAttr
        || attrName == SVGNames::yAttr
        || attrName == SVGNames::dxAttr
        || attrName == SVGNames::dyAttr)
        updateRelativeLengthsInformation();

    if (!renderer())
        return;

    if (isKnownAttribute(attrName))
        RenderSVGResource::markForLayoutAndParentResourceInvalidation(renderer());
}

void SVGTextPositioningElement::synchronizeProperty(const QualifiedName& attrName)
{
    SVGTextContentElement::synchronizeProperty(attrName);

    if (attrName == anyQName()) {
        synchronizeX();
        synchronizeY();
        synchronizeDx();
        synchronizeDy();
        synchronizeRotate();
        return;
    }

    if (attrName == SVGNames::xAttr)
        synchronizeX();
    else if (attrName == SVGNames::yAttr)
        synchronizeY();
    else if (attrName == SVGNames::dxAttr)
        synchronizeDx();
    else if (attrName == SVGNames::dyAttr)
        synchronizeDy();
    else if (attrName == SVGNames::rotateAttr)
        synchronizeRotate();
}

bool SVGTextPositioningElement::isKnownAttribute(const QualifiedName& attrName)
{
    return (attrName.matches(SVGNames::xAttr) ||
            attrName.matches(SVGNames::yAttr) ||
            attrName.matches(SVGNames::dxAttr) ||
            attrName.matches(SVGNames::dyAttr) ||
            attrName.matches(SVGNames::rotateAttr) ||
            SVGTextContentElement::isKnownAttribute(attrName));
}

static inline bool listContainsRelativeValue(SVGLengthList* list)
{
    if (!list)
        return false;

    ExceptionCode ec = 0;
    int length = list->numberOfItems();
    for (int i = 0; i < length; ++i) {
        SVGLength length(list->getItem(i, ec));
        ASSERT(!ec);

        if (length.isRelative())
            return true;
    }

    return false;
}

bool SVGTextPositioningElement::selfHasRelativeLengths() const
{
    if (SVGTextContentElement::selfHasRelativeLengths())
        return true;
    if (listContainsRelativeValue(x()))
        return true;
    if (listContainsRelativeValue(y()))
        return true;
    if (listContainsRelativeValue(dx()))
        return true;
    if (listContainsRelativeValue(dy()))
        return true;
    return false;
}

SVGTextPositioningElement* SVGTextPositioningElement::elementFromRenderer(RenderObject* renderer)
{
    if (!renderer)
        return 0;

    if (!renderer->isSVGText() && !renderer->isSVGInline())
        return 0;

    Node* node = renderer->node();
    ASSERT(node);
    ASSERT(node->isSVGElement());

    if (!node->hasTagName(SVGNames::textTag)
        && !node->hasTagName(SVGNames::tspanTag)
#if ENABLE(SVG_FONTS)
        && !node->hasTagName(SVGNames::altGlyphTag)
#endif
        && !node->hasTagName(SVGNames::trefTag))
        return 0;

    return static_cast<SVGTextPositioningElement*>(node);
}


}

#endif // ENABLE(SVG)
