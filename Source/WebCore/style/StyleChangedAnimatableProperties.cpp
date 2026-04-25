/*
 * Copyright (C) 2004-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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
#include "StyleChangedAnimatableProperties.h"

#include "RenderStyle+GettersInlines.h"
#include "StyleChangedAnimatablePropertiesGenerated.h"

namespace WebCore {
namespace Style {

void conservativelyCollectChangedAnimatableProperties(const RenderStyle& a, const RenderStyle& b, CSSPropertiesBitSet& changingProperties, EnumSet<AnimatablePropertiesCollectionQuirks> quirks)
{
    // Check property values on RenderStyle for changes.

    ChangedAnimatablePropertiesGenerated::conservativelyCollectChangedAnimatableProperties(a.computedStyle(), b.computedStyle(), changingProperties);

    // Also, check some non-property and/or derived values on RenderStyle for changes.

    // `writingMode` changes conversion of logical -> physical properties, thus, we need to add all physical properties.
    if (a.writingMode() != b.writingMode()) {
        changingProperties.m_properties.merge(CSSProperty::physicalProperties);
        if (a.writingMode().isVerticalTypographic() != b.writingMode().isVerticalTypographic())
            changingProperties.m_properties.set(CSSPropertyTextEmphasisStyle);
    }

    // `insideLink` changes visited / non-visited colors, thus, we need to add all color properties.
    if (a.insideLink() != b.insideLink())
        changingProperties.m_properties.merge(CSSProperty::colorProperties);

    if (quirks.contains(AnimatablePropertiesCollectionQuirks::ComparareUsedValuesForBorderWidth)) {
        // Don't transition if the used value does not change. This is also affected by `border-*-style`.
        if (a.usedBorderTopWidth() == b.usedBorderTopWidth())
            changingProperties.m_properties.clear(CSSPropertyBorderTopWidth);
        if (a.usedBorderRightWidth() == b.usedBorderRightWidth())
            changingProperties.m_properties.clear(CSSPropertyBorderRightWidth);
        if (a.usedBorderBottomWidth() == b.usedBorderBottomWidth())
            changingProperties.m_properties.clear(CSSPropertyBorderBottomWidth);
        if (a.usedBorderLeftWidth() == b.usedBorderLeftWidth())
            changingProperties.m_properties.clear(CSSPropertyBorderLeftWidth);
    }
}

} // namespace Style
} // namespace WebCore
