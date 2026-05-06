/*
 * Copyright (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2025-2026 Samuel Weinig <sam@webkit.org>
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
#include "StyleInheritedRareData.h"

#include "StyleComputedStyle+DifferenceLogging.h"
#include "StyleComputedStyle+InitialInlines.h"

namespace WebCore {
namespace Style {

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(InheritedRareData);

InheritedRareData::InheritedRareData()
    : usedZoom(1.0f)
    , usedTouchAction(ComputedStyle::initialTouchAction())
    , eventListenerRegionTypes { }
    , cssData(InheritedRareCSSData::create())
    , effectiveInert(false)
    , effectivelyTransparent(false)
    , isInSubtreeWithBlendMode(false)
    , isForceHidden(false)
    , usedContentVisibility(static_cast<unsigned>(ContentVisibility::Visible))
    , autoRevealsWhenFound(false)
    , insideDefaultButton(false)
    , insideSubmitButton(false)
    , evaluationTimeZoomEnabled(true)
#if HAVE(CORE_MATERIAL)
    , usedAppleVisualEffectForSubtree(static_cast<unsigned>(AppleVisualEffect::None))
#endif
{
}

inline InheritedRareData::InheritedRareData(const InheritedRareData& o)
    : RefCounted<InheritedRareData>()
    , usedZoom(o.usedZoom)
    , usedTouchAction(o.usedTouchAction)
    , eventListenerRegionTypes(o.eventListenerRegionTypes)
    , cssData(o.cssData)
    , effectiveInert(o.effectiveInert)
    , effectivelyTransparent(o.effectivelyTransparent)
    , isInSubtreeWithBlendMode(o.isInSubtreeWithBlendMode)
    , isForceHidden(o.isForceHidden)
    , usedContentVisibility(o.usedContentVisibility)
    , autoRevealsWhenFound(o.autoRevealsWhenFound)
    , insideDefaultButton(o.insideDefaultButton)
    , insideSubmitButton(o.insideSubmitButton)
    , evaluationTimeZoomEnabled(o.evaluationTimeZoomEnabled)
#if HAVE(CORE_MATERIAL)
    , usedAppleVisualEffectForSubtree(o.usedAppleVisualEffectForSubtree)
#endif
{
    ASSERT(o == *this, "InheritedRareData should be properly copied.");
}

Ref<InheritedRareData> InheritedRareData::copy() const
{
    return adoptRef(*new InheritedRareData(*this));
}

InheritedRareData::~InheritedRareData() = default;

bool InheritedRareData::operator==(const InheritedRareData& o) const
{
    return usedZoom == o.usedZoom
        && usedTouchAction == o.usedTouchAction
        && eventListenerRegionTypes == o.eventListenerRegionTypes
        && cssData == o.cssData
        && effectiveInert == o.effectiveInert
        && effectivelyTransparent == o.effectivelyTransparent
        && isInSubtreeWithBlendMode == o.isInSubtreeWithBlendMode
        && isForceHidden == o.isForceHidden
        && usedContentVisibility == o.usedContentVisibility
        && autoRevealsWhenFound == o.autoRevealsWhenFound
        && insideDefaultButton == o.insideDefaultButton
        && insideSubmitButton == o.insideSubmitButton
        && evaluationTimeZoomEnabled == o.evaluationTimeZoomEnabled
#if HAVE(CORE_MATERIAL)
        && usedAppleVisualEffectForSubtree == o.usedAppleVisualEffectForSubtree
#endif
        ;
}

#if !LOG_DISABLED
void InheritedRareData::dumpDifferences(TextStream& ts, const InheritedRareData& other) const
{
    LOG_IF_DIFFERENT(usedZoom);

    LOG_IF_DIFFERENT(usedTouchAction);
    LOG_IF_DIFFERENT(eventListenerRegionTypes);

    if (cssData.ptr() != other.cssData.ptr())
        cssData->dumpDifferences(ts, other.cssData);

    LOG_IF_DIFFERENT_WITH_CAST(bool, effectiveInert);
    LOG_IF_DIFFERENT_WITH_CAST(bool, effectivelyTransparent);

    LOG_IF_DIFFERENT_WITH_CAST(bool, isInSubtreeWithBlendMode);
    LOG_IF_DIFFERENT_WITH_CAST(bool, isForceHidden);
    LOG_IF_DIFFERENT_WITH_CAST(bool, autoRevealsWhenFound);

    LOG_IF_DIFFERENT_WITH_CAST(ContentVisibility, usedContentVisibility);

    LOG_IF_DIFFERENT_WITH_CAST(bool, insideDefaultButton);
    LOG_IF_DIFFERENT_WITH_CAST(bool, insideSubmitButton);

    LOG_IF_DIFFERENT(evaluationTimeZoomEnabled);

#if HAVE(CORE_MATERIAL)
    LOG_IF_DIFFERENT_WITH_CAST(AppleVisualEffect, usedAppleVisualEffectForSubtree);
#endif
}
#endif

} // namespace Style
} // namespace WebCore
