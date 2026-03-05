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

#include <wtf/OptionSet.h>

namespace WebCore {

class RenderStyle;

namespace Style {

class ComputedStyle;

// The difference between two styles.  The following values are used:
// - DifferenceResult::Equal - The two styles are identical
// - DifferenceResult::RecompositeLayer - The layer needs its position and transform updated, but no repaint
// - DifferenceResult::Repaint - The object just needs to be repainted.
// - DifferenceResult::RepaintIfText - The object needs to be repainted if it contains text.
// - DifferenceResult::RepaintLayer - The layer and its descendant layers needs to be repainted.
// - DifferenceResult::LayoutOutOfFlowMovementOnly - Only the position of this out-of-flow box has been updated
// - DifferenceResult::Overflow - Only overflow needs to be recomputed
// - DifferenceResult::OverflowAndOutOfFlowMovement - Both out-of-flow movement and overflow updates are required.
// - DifferenceResult::Layout - A full layout is required.
enum class DifferenceResult : uint8_t {
    Equal,
    RecompositeLayer,
    Repaint,
    RepaintIfText,
    RepaintLayer,
    LayoutOutOfFlowMovementOnly,
    Overflow,
    OverflowAndOutOfFlowMovement,
    Layout,
    NewStyle
};

// When some style properties change, different amounts of work have to be done depending on
// context (e.g. whether the property is changing on an element which has a compositing layer).
// A simple DifferenceResult does not provide enough information so we return a bit mask of
// DifferenceContextSensitiveProperties from difference() as well.
enum class DifferenceContextSensitiveProperty : uint8_t {
    Transform   = 1 << 0,
    Opacity     = 1 << 1,
    Filter      = 1 << 2,
    ClipRect    = 1 << 3,
    ClipPath    = 1 << 4,
    WillChange  = 1 << 5,
};

struct Difference {
    constexpr explicit Difference() = default;
    constexpr Difference(DifferenceResult result, OptionSet<DifferenceContextSensitiveProperty> contextSensitiveProperties = { })
        : result { result }
        , contextSensitiveProperties { contextSensitiveProperties }
    {
    }

    DifferenceResult result { DifferenceResult::Equal };
    OptionSet<DifferenceContextSensitiveProperty> contextSensitiveProperties { };

    bool operator==(const Difference&) const = default;
    bool operator==(const DifferenceResult& other) const { return result == other; }

    std::strong_ordering operator<=>(const Difference& other) const { return result <=> other.result; }
    std::strong_ordering operator<=>(const DifferenceResult& other) const { return result <=> other; }
};
Difference difference(const RenderStyle&, const RenderStyle&);

bool differenceRequiresLayerRepaint(const RenderStyle&, const RenderStyle&, bool isComposited);
bool borderIsEquivalentForPainting(const RenderStyle&, const RenderStyle&);

WTF::TextStream& operator<<(WTF::TextStream&, Difference);
WTF::TextStream& operator<<(WTF::TextStream&, DifferenceResult);
WTF::TextStream& operator<<(WTF::TextStream&, DifferenceContextSensitiveProperty);

#if !LOG_DISABLED
void dumpDifferences(WTF::TextStream&, const RenderStyle&, const RenderStyle&);
#endif

} // namespace Style
} // namespace WebCore
