/*
 * Copyright (C) 2004, 2005, 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 *               2004, 2005, 2006, 2007, 2008 Rob Buis <buis@kde.org>
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
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
#include "RenderSVGResourceClipper.h"

#include "AffineTransform.h"
#include "FloatRect.h"
#include "GraphicsContext.h"
#include "RenderSVGResource.h"
#include "SVGClipPathElement.h"
#include "SVGElement.h"
#include "SVGRenderSupport.h"
#include "SVGStyledElement.h"
#include "SVGStyledTransformableElement.h"
#include "SVGUnitTypes.h"

namespace WebCore {

RenderSVGResourceType RenderSVGResourceClipper::s_resourceType = ClipperResourceType;

RenderSVGResourceClipper::RenderSVGResourceClipper(SVGStyledElement* node)
    : RenderSVGResource(node)
{
}

RenderSVGResourceClipper::~RenderSVGResourceClipper()
{
    m_clipper.clear();
}

void RenderSVGResourceClipper::invalidateClients()
{
    HashSet<RenderObject*>::const_iterator end = m_clipper.end();
    for (HashSet<RenderObject*>::const_iterator it = m_clipper.begin(); it != end; ++it)
        (*it)->setNeedsLayout(true);
    m_clipper.clear();
}

void RenderSVGResourceClipper::invalidateClient(RenderObject* object)
{
    ASSERT(object);

    // FIXME: The HashSet should always contain the object on calling invalidateClient. A race condition
    // during the parsing can causes a call of invalidateClient right before the call of applyResource.
    // We return earlier for the moment. This bug should be fixed in:
    // https://bugs.webkit.org/show_bug.cgi?id=35181
    if (!m_clipper.contains(object))
        return;

    m_clipper.remove(object);
}

bool RenderSVGResourceClipper::applyResource(RenderObject* object, GraphicsContext* context)
{
    ASSERT(object);
    ASSERT(context);

    m_clipper.add(object);

    context->beginPath();

    AffineTransform obbTransform;
    FloatRect objectBoundingBox = object->objectBoundingBox();
    bool bbox = static_cast<SVGClipPathElement*>(node())->clipPathUnits() == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX;
    if (bbox) {
        obbTransform.translate(objectBoundingBox.x(), objectBoundingBox.y());
        obbTransform.scaleNonUniform(objectBoundingBox.width(), objectBoundingBox.height());
    }

    bool hasClipPath = false;
    WindRule clipRule = RULE_EVENODD;
    for (Node* childNode = node()->firstChild(); childNode; childNode = childNode->nextSibling()) {
        if (!childNode->isSVGElement() || !static_cast<SVGElement*>(childNode)->isStyledTransformable())
            continue;
        SVGStyledTransformableElement* styled = static_cast<SVGStyledTransformableElement*>(childNode);
        RenderStyle* style = styled->renderer() ? styled->renderer()->style() : 0;
        if (!style || style->display() == NONE)
            continue;
        Path pathData = styled->toClipPath();
        if (pathData.isEmpty())
            continue;
        if (bbox)
            pathData.transform(obbTransform);
        hasClipPath = true;
        context->addPath(pathData);
        clipRule = style->svgStyle()->clipRule();
    }

    if (!hasClipPath) {
        Path clipPath;
        clipPath.addRect(FloatRect());
        context->addPath(clipPath);
    }

    // FIXME!
    // We don't currently allow for heterogenous clip rules.
    // we would have to detect such, draw to a mask, and then clip
    // to that mask
    context->clipPath(clipRule);

    return true;
}

FloatRect RenderSVGResourceClipper::resourceBoundingBox(const FloatRect& objectBoundingBox) const
{
    FloatRect clipRect;
    for (Node* childNode = node()->firstChild(); childNode; childNode = childNode->nextSibling()) {
        if (!childNode->isSVGElement() || !static_cast<SVGElement*>(childNode)->isStyledTransformable())
            continue;
        SVGStyledTransformableElement* styled = static_cast<SVGStyledTransformableElement*>(childNode);
        RenderStyle* style = styled->renderer() ? styled->renderer()->style() : 0;
        if (!style || style->display() == NONE || styled->toClipPath().isEmpty())
            continue;
        clipRect.unite(styled->renderer()->objectBoundingBox());
    }

    if (clipRect.isEmpty())
        return FloatRect();

    if (static_cast<SVGClipPathElement*>(node())->clipPathUnits() == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX) {
        AffineTransform obbTransform;
        obbTransform.translate(objectBoundingBox.x(), objectBoundingBox.y());
        obbTransform.scaleNonUniform(objectBoundingBox.width(), objectBoundingBox.height());
        return obbTransform.mapRect(clipRect);
    }

    return clipRect;
}

}
