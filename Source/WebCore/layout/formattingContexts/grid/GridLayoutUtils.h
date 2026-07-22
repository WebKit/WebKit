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

#pragma once

#include "GridTypeAliases.h"
#include "LayoutUnit.h"
#include <optional>

namespace WebCore {

class WritingMode;

namespace Style {
struct PreferredSize;
}

namespace Layout {

class ElementBox;
class GridFormattingContext;
class IntegrationUtils;
class PlacedGridItem;
struct GridItemSizingFunctions;

namespace GridLayoutUtils {

LayoutUnit NODELETE totalGuttersSize(size_t tracksCount, LayoutUnit gapsSize);

LayoutUnit inlinePreferredSize(const PlacedGridItem&, LayoutUnit borderAndPadding, LayoutUnit columnsSize, const IntegrationUtils&);
LayoutUnit blockPreferredSize(const PlacedGridItem&, LayoutUnit borderAndPadding, LayoutUnit rowsSize, const GridFormattingContext&, LayoutUnit inlineAxisConstraint);

LayoutUnit inlineMinimumSize(const PlacedGridItem&, const TrackSizingFunctionsList&, LayoutUnit borderAndPadding, LayoutUnit columnsSize, const IntegrationUtils&);
LayoutUnit blockMinimumSize(const PlacedGridItem&, const TrackSizingFunctionsList&, LayoutUnit borderAndPadding, LayoutUnit rowsSize, const GridFormattingContext&, LayoutUnit inlineAxisConstraint);
LayoutUnit inlineMaximumSize(const PlacedGridItem&, LayoutUnit borderAndPadding);
LayoutUnit blockMaximumSize(const PlacedGridItem&, LayoutUnit borderAndPadding);
LayoutUnit inlineUsedSize(const PlacedGridItem&, const TrackSizingFunctionsList&, LayoutUnit borderAndPadding, LayoutUnit columnsSize, const IntegrationUtils&);
LayoutUnit blockUsedSize(const PlacedGridItem&, const TrackSizingFunctionsList&, LayoutUnit borderAndPadding, LayoutUnit rowsSize, const GridFormattingContext&, LayoutUnit inlineAxisConstraint);

LayoutUnit computeGridLinePosition(size_t gridLineIndex, const TrackSizes&, LayoutUnit gap);
LayoutUnit gridAreaDimensionSize(size_t startLine, size_t endLine, const TrackSizes&, LayoutUnit gap);

LayoutUnit inlineAxisMinContentContribution(const PlacedGridItem&, LayoutUnit blockAxisConstraint, const IntegrationUtils&);
LayoutUnit inlineAxisMaxContentContribution(const PlacedGridItem&, LayoutUnit blockAxisConstraint, const IntegrationUtils&);

LayoutUnit blockAxisMinContentContribution(const PlacedGridItem&, LayoutUnit inlineAxisConstraint, const GridFormattingContext&);
LayoutUnit blockAxisMaxContentContribution(const PlacedGridItem&, LayoutUnit inlineAxisConstraint, const GridFormattingContext&);

bool preferredSizeBehavesAsAuto(const Style::PreferredSize&);
template<typename SizeType>
bool sizeDependsOnContainingBlockSize(const SizeType& size)
{
    return size.isStretch() || size.isPercentOrCalculated();
}

std::optional<double> preferredAspectRatio(const ElementBox&);
bool inlineContributionMayRequireFullSizingAlgorithmForIntrinsicWidth(const ElementBox&, WritingMode containerWritingMode);

} // namespace GridLayoutUtils
} // namespace Layout
} // namespace WebCore
