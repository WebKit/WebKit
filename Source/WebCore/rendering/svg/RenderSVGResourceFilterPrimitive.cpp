/*
 * Copyright (C) 2024 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "RenderSVGResourceFilterPrimitive.h"

#include "RenderSVGModelObjectInlines.h"
#include "RenderSVGResourceFilter.h"
#include "SVGElementTypeHelpers.h"
#include "SVGFEDiffuseLightingElement.h"
#include "SVGFEDropShadowElement.h"
#include "SVGFEFloodElement.h"
#include "SVGFESpecularLightingElement.h"
#include "SVGFilterPrimitiveStandardAttributes.h"
#include "SVGNames.h"
#include "SVGRenderStyle.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(RenderSVGResourceFilterPrimitive);

RenderSVGResourceFilterPrimitive::RenderSVGResourceFilterPrimitive(SVGFilterPrimitiveStandardAttributes& element, RenderStyle&& style)
    : RenderSVGHiddenContainer(Type::SVGResourceFilterPrimitive, element, WTFMove(style))
{
    ASSERT(isRenderSVGResourceFilterPrimitive());
}

void RenderSVGResourceFilterPrimitive::markFilterEffectForRepaint(FilterEffect* effect)
{
    if (!effect)
        return;

    CheckedPtr parent = dynamicDowncast<RenderSVGResourceFilter>(this->parent());
    if (!parent)
        return;

    parent->invalidateFilter();
}

void RenderSVGResourceFilterPrimitive::markFilterEffectForRebuild()
{
    CheckedPtr parent = dynamicDowncast<RenderSVGResourceFilter>(this->parent());
    if (!parent)
        return;

    parent->invalidateFilter();
}

SVGFilterPrimitiveStandardAttributes& RenderSVGResourceFilterPrimitive::filterPrimitiveElement() const
{
    return static_cast<SVGFilterPrimitiveStandardAttributes&>(RenderSVGHiddenContainer::element());
}

void RenderSVGResourceFilterPrimitive::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderSVGHiddenContainer::styleDidChange(diff, oldStyle);

    if (diff == StyleDifference::Equal || !oldStyle)
        return;

    Ref newStyle = style().svgStyle();
    if (is<SVGFEFloodElement>(filterPrimitiveElement()) || is<SVGFEDropShadowElement>(filterPrimitiveElement())) {
        if (newStyle->floodColor() != oldStyle->svgStyle().floodColor())
            filterPrimitiveElement().primitiveAttributeChanged(SVGNames::flood_colorAttr);
        if (newStyle->floodOpacity() != oldStyle->svgStyle().floodOpacity())
            filterPrimitiveElement().primitiveAttributeChanged(SVGNames::flood_opacityAttr);
        return;
    }
    if (is<SVGFEDiffuseLightingElement>(filterPrimitiveElement()) || is<SVGFESpecularLightingElement>(filterPrimitiveElement())) {
        if (newStyle->lightingColor() != oldStyle->svgStyle().lightingColor())
            filterPrimitiveElement().primitiveAttributeChanged(SVGNames::lighting_colorAttr);
    }
}

} // namespace WebCore
