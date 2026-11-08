/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include "StyleGridPosition.h"
#include "UnplacedGridItem.h"

namespace WebCore {
namespace Layout {

class NamedLineResolver;

// Resolves the grid-*-start and grid-*-end of one axis of a grid item into lines, ahead of the placement algorithm
// (https://drafts.csswg.org/css-grid-1/#line-placement). Both take the axis's NamedLineResolver, which resolves the lines given with a
// name and holds the number of explicit tracks that negative lines count back from.

// The 0-based explicit line range [start, end) for an axis, or std::nullopt when the axis is auto-positioned (neither edge is a
// line, which is an <integer> or a lone <custom-ident>). The resolved lines may still be negative for negative line placements that
// count past the start edge; the leading implicit track count is applied later in resolveAxisPosition(). maxTracks limits the
// resolved lines, see https://drafts.csswg.org/css-grid-1/#overlarge-grids.
std::optional<std::pair<int, int>> resolveDefinitePosition(const Style::GridPosition& start, const Style::GridPosition& end, const NamedLineResolver&, size_t maxTracks);

// The axis's position for an UnplacedGridItem: the range of resolveDefinitePosition() shifted forward by the leading implicit tracks
// count so that it maps to non-negative matrix indices, or the span of an auto-positioned axis.
UnplacedGridItem::AxisPosition resolveAxisPosition(const Style::GridPosition& start, const Style::GridPosition& end, const NamedLineResolver&, size_t leadingImplicitTracksCount, size_t maxTracks);

} // namespace Layout
} // namespace WebCore
