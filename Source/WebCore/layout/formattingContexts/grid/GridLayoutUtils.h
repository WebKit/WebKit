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

namespace WebCore {

class LayoutUnit;

namespace Layout {

class PlacedGridItem;
struct GridItemSizingFunctions;

namespace GridLayoutUtils {

LayoutUnit computeGapValue(const Style::GapGutter&);

LayoutUnit usedInlineSizeForGridItem(const PlacedGridItem&, LayoutUnit borderAndPadding, LayoutUnit columnsSize);
LayoutUnit usedBlockSizeForGridItem(const PlacedGridItem&, LayoutUnit borderAndPadding, LayoutUnit rowsSize);

LayoutUnit usedInlineMinimumSize(const PlacedGridItem&, const TrackSizingFunctionsList&, LayoutUnit borderAndPadding, LayoutUnit columnsSize, const IntegrationUtils&);
LayoutUnit usedBlockMinimumSize(const PlacedGridItem&, const TrackSizingFunctionsList&, LayoutUnit borderAndPadding, LayoutUnit rowsSize, const IntegrationUtils&);

LayoutUnit computeGridLinePosition(size_t gridLineIndex, const TrackSizes&, LayoutUnit gap);
LayoutUnit gridAreaDimensionSize(size_t startLine, size_t endLine, const TrackSizes&, LayoutUnit gap);

LayoutUnit inlineAxisMinContentContribution(const ElementBox& gridItem, const IntegrationUtils&);
LayoutUnit inlineAxisMaxContentContribution(const ElementBox& gridItem, const IntegrationUtils&);
GridItemSizingFunctions inlineAxisGridItemSizingFunctions();

LayoutUnit blockAxisMinContentContribution(const ElementBox& gridItem, const IntegrationUtils&);
LayoutUnit blockAxisMaxContentContribution(const ElementBox& gridItem, const IntegrationUtils&);
GridItemSizingFunctions blockAxisGridItemSizingFunctions();

bool preferredSizeBehavesAsAuto(const Style::PreferredSize&);
bool preferredSizeDependsOnContainingBlockSize(const Style::PreferredSize&);

} // namespace GridLayoutUtils
} // namespace Layout
} // namespace WebCore
