/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#include "StyleMiscNonInheritedData.h"

#include "AnimationList.h"
#include "ContentData.h"
#include "FillLayer.h"
#include "RenderStyleInlines.h"
#include "ShadowData.h"
#include "StyleDeprecatedFlexibleBoxData.h"
#include "StyleFilterData.h"
#include "StyleFlexibleBoxData.h"
#include "StyleMultiColData.h"
#include "StyleTransformData.h"
#include "StyleVisitedLinkColorData.h"
#include <wtf/PointerComparison.h>

namespace WebCore {

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(StyleMiscNonInheritedData);

StyleMiscNonInheritedData::StyleMiscNonInheritedData()
    : opacity(RenderStyle::initialOpacity())
    , deprecatedFlexibleBox(StyleDeprecatedFlexibleBoxData::create())
    , flexibleBox(StyleFlexibleBoxData::create())
    , multiCol(StyleMultiColData::create())
    , filter(StyleFilterData::create())
    , transform(StyleTransformData::create())
    , mask(FillLayer::create(FillLayerType::Mask))
    , visitedLinkColor(StyleVisitedLinkColorData::create())
    , aspectRatioWidth(RenderStyle::initialAspectRatioWidth())
    , aspectRatioHeight(RenderStyle::initialAspectRatioHeight())
    , alignContent(RenderStyle::initialContentAlignment())
    , justifyContent(RenderStyle::initialContentAlignment())
    , alignItems(RenderStyle::initialDefaultAlignment())
    , alignSelf(RenderStyle::initialSelfAlignment())
    , justifyItems(RenderStyle::initialJustifyItems())
    , justifySelf(RenderStyle::initialSelfAlignment())
    , objectPosition(RenderStyle::initialObjectPosition())
    , order(RenderStyle::initialOrder())
    , aspectRatioType(static_cast<unsigned>(RenderStyle::initialAspectRatioType()))
    , appearance(static_cast<unsigned>(RenderStyle::initialAppearance()))
    , effectiveAppearance(static_cast<unsigned>(RenderStyle::initialAppearance()))
    , textOverflow(static_cast<unsigned>(RenderStyle::initialTextOverflow()))
    , userDrag(static_cast<unsigned>(RenderStyle::initialUserDrag()))
    , objectFit(static_cast<unsigned>(RenderStyle::initialObjectFit()))
    , resize(static_cast<unsigned>(RenderStyle::initialResize()))
{
}

StyleMiscNonInheritedData::StyleMiscNonInheritedData(const StyleMiscNonInheritedData& o)
    : RefCounted<StyleMiscNonInheritedData>()
    , opacity(o.opacity)
    , deprecatedFlexibleBox(o.deprecatedFlexibleBox)
    , flexibleBox(o.flexibleBox)
    , multiCol(o.multiCol)
    , filter(o.filter)
    , transform(o.transform)
    , mask(o.mask)
    , visitedLinkColor(o.visitedLinkColor)
    , animations(o.animations ? o.animations->copy() : o.animations)
    , transitions(o.transitions ? o.transitions->copy() : o.transitions)
    , content(o.content ? o.content->clone() : nullptr)
    , boxShadow(o.boxShadow ? makeUnique<ShadowData>(*o.boxShadow) : nullptr)
    , altText(o.altText)
    , aspectRatioWidth(o.aspectRatioWidth)
    , aspectRatioHeight(o.aspectRatioHeight)
    , alignContent(o.alignContent)
    , justifyContent(o.justifyContent)
    , alignItems(o.alignItems)
    , alignSelf(o.alignSelf)
    , justifyItems(o.justifyItems)
    , justifySelf(o.justifySelf)
    , objectPosition(o.objectPosition)
    , order(o.order)
    , hasAttrContent(o.hasAttrContent)
#if ENABLE(DARK_MODE_CSS)
    , hasExplicitlySetColorScheme(o.hasExplicitlySetColorScheme)
#endif
    , hasExplicitlySetDirection(o.hasExplicitlySetDirection)
    , hasExplicitlySetWritingMode(o.hasExplicitlySetWritingMode)
    , aspectRatioType(o.aspectRatioType)
    , appearance(o.appearance)
    , effectiveAppearance(o.effectiveAppearance)
    , textOverflow(o.textOverflow)
    , userDrag(o.userDrag)
    , objectFit(o.objectFit)
    , resize(o.resize)
{
}

StyleMiscNonInheritedData::~StyleMiscNonInheritedData() = default;

Ref<StyleMiscNonInheritedData> StyleMiscNonInheritedData::copy() const
{
    return adoptRef(*new StyleMiscNonInheritedData(*this));
}

bool StyleMiscNonInheritedData::operator==(const StyleMiscNonInheritedData& o) const
{
    return opacity == o.opacity
        && deprecatedFlexibleBox == o.deprecatedFlexibleBox
        && flexibleBox == o.flexibleBox
        && multiCol == o.multiCol
        && filter == o.filter
        && transform == o.transform
        && mask == o.mask
        && visitedLinkColor == o.visitedLinkColor
        && arePointingToEqualData(animations, o.animations)
        && arePointingToEqualData(transitions, o.transitions)
        && contentDataEquivalent(o)
        && arePointingToEqualData(boxShadow, o.boxShadow)
        && altText == o.altText
        && aspectRatioWidth == o.aspectRatioWidth
        && aspectRatioHeight == o.aspectRatioHeight
        && alignContent == o.alignContent
        && justifyContent == o.justifyContent
        && alignItems == o.alignItems
        && alignSelf == o.alignSelf
        && justifyItems == o.justifyItems
        && justifySelf == o.justifySelf
        && objectPosition == o.objectPosition
        && order == o.order
        && hasAttrContent == o.hasAttrContent
#if ENABLE(DARK_MODE_CSS)
        && hasExplicitlySetColorScheme == o.hasExplicitlySetColorScheme
#endif
        && hasExplicitlySetDirection == o.hasExplicitlySetDirection
        && hasExplicitlySetWritingMode == o.hasExplicitlySetWritingMode
        && aspectRatioType == o.aspectRatioType
        && appearance == o.appearance
        && effectiveAppearance == o.effectiveAppearance
        && textOverflow == o.textOverflow
        && userDrag == o.userDrag
        && objectFit == o.objectFit
        && resize == o.resize;
}

bool StyleMiscNonInheritedData::contentDataEquivalent(const StyleMiscNonInheritedData& other) const
{
    auto* a = content.get();
    auto* b = other.content.get();
    while (a && b && *a == *b) {
        a = a->next();
        b = b->next();
    }
    return !a && !b;
}

bool StyleMiscNonInheritedData::hasFilters() const
{
    return !filter->operations.isEmpty();
}

} // namespace WebCore
