/*
 * Copyright (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
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
#include "RenderSVGTextPath.h"

#include "FloatQuad.h"
#include "RenderBlock.h"
#include "RenderBoxModelObjectInlines.h"
#include "RenderLayer.h"
#include "RenderSVGInlineInlines.h"
#include "RenderSVGShape.h"
#include "SVGElementTypeHelpers.h"
#include "SVGGeometryElement.h"
#include "SVGInlineTextBox.h"
#include "SVGNames.h"
#include "SVGPathData.h"
#include "SVGPathElement.h"
#include "SVGRootInlineBox.h"
#include "SVGTextPathElement.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderSVGTextPath);

RenderSVGTextPath::RenderSVGTextPath(SVGTextPathElement& element, RenderStyle&& style)
    : RenderSVGInline(Type::SVGTextPath, element, WTFMove(style))
{
    ASSERT(isRenderSVGTextPath());
}

SVGTextPathElement& RenderSVGTextPath::textPathElement() const
{
    return downcast<SVGTextPathElement>(RenderSVGInline::graphicsElement());
}

SVGGeometryElement* RenderSVGTextPath::targetElement() const
{
    auto target = SVGURIReference::targetElementFromIRIString(textPathElement().href(), textPathElement().treeScopeForSVGReferences());
    return dynamicDowncast<SVGGeometryElement>(target.element.get());
}

Path RenderSVGTextPath::layoutPath() const
{
    RefPtr element = targetElement();
    if (!is<SVGGeometryElement>(element))
        return { };

    auto path = pathFromGraphicsElement(*element);

    // Spec:  The transform attribute on the referenced 'path' element represents a
    // supplemental transformation relative to the current user coordinate system for
    // the current 'text' element, including any adjustments to the current user coordinate
    // system due to a possible transform attribute on the current 'text' element.
    // http://www.w3.org/TR/SVG/text.html#TextPathElement
#if ENABLE(LAYER_BASED_SVG_ENGINE)
    if (element->renderer() && document().settings().layerBasedSVGEngineEnabled()) {
        auto& renderer = downcast<RenderSVGShape>(*element->renderer());
        if (auto* layer = renderer.layer()) {
            const auto& layerTransform = layer->currentTransform(RenderStyle::individualTransformOperations()).toAffineTransform();
            if (!layerTransform.isIdentity())
                path.transform(layerTransform);
            return path;
        }
    }
#endif

    path.transform(element->animatedLocalTransform());
    return path;
}

const SVGLengthValue& RenderSVGTextPath::startOffset() const
{
    return textPathElement().startOffset();
}

}
