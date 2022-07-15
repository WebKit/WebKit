/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Google, Inc.
 * Copyright (c) 2020, 2021, 2022 Igalia S.L.
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
#include "RenderSVGViewportContainer.h"

#if ENABLE(LAYER_BASED_SVG_ENGINE)
#include "RenderLayer.h"
#include "RenderSVGModelObjectInlines.h"
#include "RenderSVGRoot.h"
#include "SVGContainerLayout.h"
#include "SVGElementTypeHelpers.h"
#include "SVGSVGElement.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderSVGViewportContainer);

RenderSVGViewportContainer::RenderSVGViewportContainer(SVGSVGElement& element, RenderStyle&& style)
    : RenderSVGContainer(element, WTFMove(style))
{
}

SVGSVGElement& RenderSVGViewportContainer::svgSVGElement() const
{
    return downcast<SVGSVGElement>(RenderSVGContainer::element());
}

FloatRect RenderSVGViewportContainer::computeViewport() const
{
    auto& useSVGSVGElement = svgSVGElement();

    SVGLengthContext lengthContext(&useSVGSVGElement);
    return { useSVGSVGElement.x().value(lengthContext), useSVGSVGElement.y().value(lengthContext),
        useSVGSVGElement.width().value(lengthContext), useSVGSVGElement.height().value(lengthContext) };
}

void RenderSVGViewportContainer::updateLayerTransform()
{
    RenderSVGContainer::updateLayerTransform();
}

void RenderSVGViewportContainer::applyTransform(TransformationMatrix&, const RenderStyle&, const FloatRect&, OptionSet<RenderStyle::TransformOperationOption>) const
{
}

}

#endif // ENABLE(LAYER_BASED_SVG_ENGINE)
