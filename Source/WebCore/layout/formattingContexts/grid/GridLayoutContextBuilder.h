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

#include "GridFormattingContext.h"
#include "GridLayoutConstraints.h"
#include "GridLayoutContext.h"
#include "GridTypeAliases.h"
#include <wtf/CheckedRef.h>

namespace WebCore {
namespace Layout {

class ElementBox;

// What differs, in one axis, between laying out a grid and measuring it.
struct GridAxisLayoutContextInput {
    // The container size that the number of auto-repeat repetitions is resolved against.
    AutoRepeatConstraint autoRepeat;
    // https://drafts.csswg.org/css-grid-1/#track-sizes
    // True when the size of the grid container depends on the size of its tracks in this axis, in which case
    // percentages in this axis' track sizes are treated as auto.
    bool containerSizeDependsOnTracks { false };
};

struct GridLayoutContextInput {
    GridAxisLayoutContextInput inlineAxis;
    GridAxisLayoutContextInput blockAxis;
    // layout() sizes the rows a second time against the container's used block size, so it needs the row track sizes as
    // specified. Only layout() sets this.
    bool keepSpecifiedRowTrackSizes { false };
};

// Builds a GridLayoutContext from the grid container's style and its in-flow children.
class GridLayoutContextBuilder {
public:
    explicit GridLayoutContextBuilder(const ElementBox& gridBox);

    GridLayoutContext build(const GridLayoutContextInput&) const;

private:
    PlacedGridItems constructPlacedGridItems(const GridAreas&) const;

    const CheckedRef<const ElementBox> m_gridBox;
};

} // namespace Layout
} // namespace WebCore
