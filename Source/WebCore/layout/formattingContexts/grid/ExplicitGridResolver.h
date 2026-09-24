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

#include "ExplicitGridTrackSizes.h"

namespace WebCore {

namespace Style {
class ComputedStyle;
struct GridTemplateList;
}

namespace Layout {

struct AxisConstraint;
struct GridLayoutConstraints;

// https://drafts.csswg.org/css-grid-1/#explicit-grids
// Resolves grid-template-{columns,rows} into the explicit grid's track lists. The rest of grid
// layout reads the explicit grid from the result rather than from style, because repeat() and
// auto-repeat mean the track lists in style are not the explicit grid's final tracks.
class ExplicitGridResolver {
public:
    static ExplicitGridTrackSizes resolve(const Style::ComputedStyle& gridContainerStyle, const GridLayoutConstraints&);

private:
    static Vector<Style::GridTrackSize> resolveTrackSizes(const Style::GridTemplateList&, const AxisConstraint&);
};

} // namespace Layout
} // namespace WebCore
