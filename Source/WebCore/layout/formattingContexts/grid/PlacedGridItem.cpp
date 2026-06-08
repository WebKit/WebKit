/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "PlacedGridItem.h"

#include "GridAreaLines.h"
#include "LayoutBoxGeometry.h"
#include "RenderStyle+GettersInlines.h"
#include "StyleAlignSelf.h"
#include "StyleJustifySelf.h"
#include "UnplacedGridItem.h"

namespace WebCore {
namespace Layout {

PlacedGridItem::PlacedGridItem(const ElementBox& gridItem, const GridAreaLines& gridAreaLines, const BoxGeometry& gridItemGeometry, const RenderStyle& gridContainerStyle)
    : PlacedGridItem(gridItem, gridAreaLines, gridItemGeometry, gridContainerStyle, gridItem.style())
{
}

PlacedGridItem::PlacedGridItem(const ElementBox& gridItem, const GridAreaLines& gridAreaLines, const BoxGeometry& gridItemGeometry, const RenderStyle& gridContainerStyle, const RenderStyle& gridItemStyle)
    : m_layoutBox(gridItem)
    , m_inlineAxisSizes({ gridItemStyle.width(), gridItemStyle.minWidth(), gridItemStyle.maxWidth(), gridItemStyle.marginLeft(), gridItemStyle.marginRight() })
    , m_blockAxisSizes({ gridItemStyle.height(), gridItemStyle.minHeight(), gridItemStyle.maxHeight(), gridItemStyle.marginTop(), gridItemStyle.marginBottom() })
    , m_usedInlineBorderAndPadding(gridItemGeometry.horizontalBorderAndPadding())
    , m_usedBlockBorderAndPadding(gridItemGeometry.verticalBorderAndPadding())
    , m_inlineAxisAlignment(gridItemStyle.justifySelf().resolve(&gridContainerStyle))
    , m_blockAxisAlignment(gridItemStyle.alignSelf().resolve(&gridContainerStyle))
    , m_writingMode(gridItemStyle.writingMode())
    , m_usedZoom(gridItemStyle.usedZoomForLength())
    , m_gridAreaLines(gridAreaLines)
{
}

// https://drafts.csswg.org/css-sizing-4/#aspect-ratio
std::optional<double> PlacedGridItem::preferredAspectRatio() const
{
    auto& computedAspectRatio = protect(m_layoutBox->style())->aspectRatio();

    auto isDegenerateRatio = [&] {
        auto ratio = computedAspectRatio.tryRatio();
        return !ratio || !ratio->numerator.value || !ratio->denominator.value;
    };

    // "If the <ratio> is degenerate, the property instead behaves as auto."
    //
    // auto: "Replaced elements with a natural aspect ratio use that aspect ratio;
    // otherwise the box has no preferred aspect ratio."
    if (computedAspectRatio.isAuto() || isDegenerateRatio()) {
        if (m_layoutBox->isReplacedBox() && m_layoutBox->hasIntrinsicRatio())
            return m_layoutBox->intrinsicRatio();
        return { };
    }

    // <ratio>: "The box's preferred aspect ratio is the specified ratio of width / height."
    if (computedAspectRatio.isRatio()) {
        auto ratio = *computedAspectRatio.tryRatio();
        return ratio.numerator.value / ratio.denominator.value;
    }

    // auto && <ratio>: "The preferred aspect ratio is the specified ratio of width / height
    // unless it is a replaced element with a natural aspect ratio, in which case that aspect
    // ratio is used instead."
    if (computedAspectRatio.isAutoAndRatio()) {
        if (m_layoutBox->isReplacedBox() && m_layoutBox->hasIntrinsicRatio())
            return m_layoutBox->intrinsicRatio();
        auto ratio = *computedAspectRatio.tryRatio();
        return ratio.numerator.value / ratio.denominator.value;
    }

    ASSERT_NOT_REACHED();
    return { };
}

} // namespace Layout
} // namespace WebCore
