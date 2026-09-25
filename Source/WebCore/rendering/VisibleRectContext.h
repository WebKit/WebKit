/*
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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <WebCore/LayoutRect.h>
#include <WebCore/RepaintRectCalculation.h>
#include <optional>
#include <wtf/OptionSet.h>
#include <wtf/ScopedLambda.h>

namespace WebCore {

class Frame;

// Given a scroll container or frame viewport clip rect (in the coordinate space of the clipping
// renderer or frame), and the frame that it belongs to, returns an adjusted clip rect,
// or std::nullopt to leave the clip unchanged.
using ClipRectAdjuster = ScopedLambda<std::optional<LayoutRect>(const Frame&, const LayoutRect& clipRect)>;

// Configuration for a visible rect computation. Constant for the duration of a traversal,
// so it is passed by const reference.
struct VisibleRectContext {
    enum class Option : uint8_t {
        UseEdgeInclusiveIntersection        = 1 << 0,
        ApplyCompositedClips                = 1 << 1,
        ApplyCompositedContainerScrolls     = 1 << 2,
        ApplyContainerClip                  = 1 << 3,
        CalculateAccurateRepaintRect        = 1 << 4,
    };

    OptionSet<Option> options { };
    const ClipRectAdjuster* scrollContainerClipRectAdjuster { nullptr };

    RepaintRectCalculation repaintRectCalculation() const
    {
        return options.contains(Option::CalculateAccurateRepaintRect) ? RepaintRectCalculation::Accurate : RepaintRectCalculation::Fast;
    }

    std::optional<LayoutRect> adjustScrollContainerClipRect(const Frame& frame, const LayoutRect& clipRect) const
    {
        if (!scrollContainerClipRectAdjuster)
            return std::nullopt;
        return (*scrollContainerClipRectAdjuster)(frame, clipRect);
    }
};

// State accumulated while walking from a renderer towards its container. Passed by value so
// that each step's changes are seen by the steps above it, but not by its own caller.
struct VisibleRectState {
    bool hasPositionFixedDescendant { false };
    bool dirtyRectIsFlipped { false };
    bool descendantNeedsEnclosingIntRect { false };
};

} // namespace WebCore
