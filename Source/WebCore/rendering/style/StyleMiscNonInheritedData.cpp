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

#include "RenderStyleDifference.h"
#include "RenderStyleInlines.h"
#include "StyleDeprecatedFlexibleBoxData.h"
#include "StyleFilterData.h"
#include "StyleFlexibleBoxData.h"
#include "StyleMultiColData.h"
#include "StylePrimitiveKeyword+Logging.h"
#include "StylePrimitiveNumericTypes+Logging.h"
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
    , visitedLinkColor(StyleVisitedLinkColorData::create())
    , mask(RenderStyle::initialMaskLayers())
    , animations(RenderStyle::initialAnimations())
    , transitions(RenderStyle::initialTransitions())
    , content(RenderStyle::initialContent())
    , boxShadow(RenderStyle::initialBoxShadow())
    , aspectRatio(RenderStyle::initialAspectRatio())
    , alignContent(RenderStyle::initialAlignContent())
    , alignItems(RenderStyle::initialAlignItems())
    , alignSelf(RenderStyle::initialAlignSelf())
    , justifyContent(RenderStyle::initialJustifyContent())
    , justifyItems(RenderStyle::initialJustifyItems())
    , justifySelf(RenderStyle::initialJustifySelf())
    , objectPosition(RenderStyle::initialObjectPosition())
    , order(RenderStyle::initialOrder())
    , tableLayout(static_cast<unsigned>(RenderStyle::initialTableLayout()))
    , appearance(static_cast<unsigned>(RenderStyle::initialAppearance()))
    , usedAppearance(static_cast<unsigned>(RenderStyle::initialAppearance()))
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
    , visitedLinkColor(o.visitedLinkColor)
    , mask(o.mask)
    , animations(o.animations)
    , transitions(o.transitions)
    , content(o.content)
    , boxShadow(o.boxShadow)
    , aspectRatio(o.aspectRatio)
    , alignContent(o.alignContent)
    , alignItems(o.alignItems)
    , alignSelf(o.alignSelf)
    , justifyContent(o.justifyContent)
    , justifyItems(o.justifyItems)
    , justifySelf(o.justifySelf)
    , objectPosition(o.objectPosition)
    , order(o.order)
    , hasAttrContent(o.hasAttrContent)
    , hasDisplayAffectedByAnimations(o.hasDisplayAffectedByAnimations)
#if ENABLE(DARK_MODE_CSS)
    , hasExplicitlySetColorScheme(o.hasExplicitlySetColorScheme)
#endif
    , hasExplicitlySetDirection(o.hasExplicitlySetDirection)
    , hasExplicitlySetWritingMode(o.hasExplicitlySetWritingMode)
    , tableLayout(o.tableLayout)
    , appearance(o.appearance)
    , usedAppearance(o.usedAppearance)
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
        && visitedLinkColor == o.visitedLinkColor
        && mask == o.mask
        && animations == o.animations
        && transitions == o.transitions
        && content == o.content
        && boxShadow == o.boxShadow
        && aspectRatio == o.aspectRatio
        && alignContent == o.alignContent
        && alignItems == o.alignItems
        && alignSelf == o.alignSelf
        && justifyContent == o.justifyContent
        && justifyItems == o.justifyItems
        && justifySelf == o.justifySelf
        && objectPosition == o.objectPosition
        && order == o.order
        && hasAttrContent == o.hasAttrContent
        && hasDisplayAffectedByAnimations == o.hasDisplayAffectedByAnimations
#if ENABLE(DARK_MODE_CSS)
        && hasExplicitlySetColorScheme == o.hasExplicitlySetColorScheme
#endif
        && hasExplicitlySetDirection == o.hasExplicitlySetDirection
        && hasExplicitlySetWritingMode == o.hasExplicitlySetWritingMode
        && tableLayout == o.tableLayout
        && appearance == o.appearance
        && usedAppearance == o.usedAppearance
        && textOverflow == o.textOverflow
        && userDrag == o.userDrag
        && objectFit == o.objectFit
        && resize == o.resize;
}

bool StyleMiscNonInheritedData::hasFilters() const
{
    return !filter->filter.isNone();
}

#if !LOG_DISABLED
void StyleMiscNonInheritedData::dumpDifferences(TextStream& ts, const StyleMiscNonInheritedData& other) const
{
    LOG_IF_DIFFERENT(opacity);

    deprecatedFlexibleBox->dumpDifferences(ts, other.deprecatedFlexibleBox);
    flexibleBox->dumpDifferences(ts, other.flexibleBox);
    multiCol->dumpDifferences(ts, other.multiCol);

    filter->dumpDifferences(ts, other.filter);
    transform->dumpDifferences(ts, other.transform);

    visitedLinkColor->dumpDifferences(ts, other.visitedLinkColor);

    LOG_IF_DIFFERENT(mask);

    LOG_IF_DIFFERENT(animations);
    LOG_IF_DIFFERENT(transitions);

    LOG_IF_DIFFERENT(content);
    LOG_IF_DIFFERENT(boxShadow);

    LOG_IF_DIFFERENT(aspectRatio);
    LOG_IF_DIFFERENT(alignContent);
    LOG_IF_DIFFERENT(alignItems);
    LOG_IF_DIFFERENT(alignSelf);
    LOG_IF_DIFFERENT(justifyContent);
    LOG_IF_DIFFERENT(justifyItems);
    LOG_IF_DIFFERENT(justifySelf);
    LOG_IF_DIFFERENT(objectPosition);
    LOG_IF_DIFFERENT(order);

    LOG_IF_DIFFERENT_WITH_CAST(bool, hasAttrContent);
    LOG_IF_DIFFERENT_WITH_CAST(bool, hasDisplayAffectedByAnimations);

#if ENABLE(DARK_MODE_CSS)
    LOG_IF_DIFFERENT_WITH_CAST(bool, hasExplicitlySetColorScheme);
#endif

    LOG_IF_DIFFERENT_WITH_CAST(bool, hasExplicitlySetDirection);
    LOG_IF_DIFFERENT_WITH_CAST(bool, hasExplicitlySetWritingMode);

    LOG_IF_DIFFERENT_WITH_CAST(TableLayoutType, tableLayout);
    LOG_IF_DIFFERENT_WITH_CAST(StyleAppearance, appearance);
    LOG_IF_DIFFERENT_WITH_CAST(StyleAppearance, usedAppearance);

    LOG_IF_DIFFERENT_WITH_CAST(bool, textOverflow);

    LOG_IF_DIFFERENT_WITH_CAST(UserDrag, objectFit);
    LOG_IF_DIFFERENT_WITH_CAST(ObjectFit, textOverflow);
    LOG_IF_DIFFERENT_WITH_CAST(Style::Resize, resize);
}
#endif // !LOG_DISABLED

} // namespace WebCore
