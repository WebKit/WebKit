/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
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
#include "StyleNonInheritedMiscData.h"

#include "StyleComputedStyle+DifferenceLogging.h"
#include "StyleComputedStyle+InitialInlines.h"
#include "StylePrimitiveKeyword+Logging.h"
#include "StylePrimitiveNumericTypes+Logging.h"

namespace WebCore {
namespace Style {

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(NonInheritedMiscData);

NonInheritedMiscData::NonInheritedMiscData()
    : opacity(ComputedStyle::initialOpacity())
    , deprecatedFlexibleBox(DeprecatedFlexibleBoxData::create())
    , flexibleBox(FlexibleBoxData::create())
    , multiCol(MultiColumnData::create())
    , filter(FilterData::create())
    , transform(TransformData::create())
    , visitedLinkColor(VisitedLinkColorData::create())
    , mask(CSS::Keyword::None { })
    , animations(CSS::Keyword::None { })
    , transitions(CSS::Keyword::All { })
    , content(ComputedStyle::initialContent())
    , boxShadow(ComputedStyle::initialBoxShadow())
    , aspectRatio(ComputedStyle::initialAspectRatio())
    , alignContent(ComputedStyle::initialAlignContent())
    , alignItems(ComputedStyle::initialAlignItems())
    , alignSelf(ComputedStyle::initialAlignSelf())
    , justifyContent(ComputedStyle::initialJustifyContent())
    , justifyItems(ComputedStyle::initialJustifyItems())
    , justifySelf(ComputedStyle::initialJustifySelf())
    , objectPosition(ComputedStyle::initialObjectPosition())
    , order(ComputedStyle::initialOrder())
    , tableLayout(static_cast<unsigned>(ComputedStyle::initialTableLayout()))
    , appearance(static_cast<unsigned>(ComputedStyle::initialAppearance()))
    , usedAppearance(static_cast<unsigned>(ComputedStyle::initialAppearance()))
    , textOverflow(static_cast<unsigned>(ComputedStyle::initialTextOverflow()))
    , userDrag(static_cast<unsigned>(ComputedStyle::initialUserDrag()))
    , objectFit(static_cast<unsigned>(ComputedStyle::initialObjectFit()))
    , resize(static_cast<unsigned>(ComputedStyle::initialResize()))
{
}

NonInheritedMiscData::NonInheritedMiscData(const NonInheritedMiscData& o)
    : RefCounted<NonInheritedMiscData>()
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

NonInheritedMiscData::~NonInheritedMiscData() = default;

Ref<NonInheritedMiscData> NonInheritedMiscData::copy() const
{
    return adoptRef(*new NonInheritedMiscData(*this));
}

bool NonInheritedMiscData::operator==(const NonInheritedMiscData& o) const
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

bool NonInheritedMiscData::hasFilters() const
{
    return !filter->filter.isNone();
}

#if !LOG_DISABLED
void NonInheritedMiscData::dumpDifferences(TextStream& ts, const NonInheritedMiscData& other) const
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
    LOG_IF_DIFFERENT_WITH_CAST(Resize, resize);
}
#endif // !LOG_DISABLED

} // namespace Style
} // namespace WebCore
