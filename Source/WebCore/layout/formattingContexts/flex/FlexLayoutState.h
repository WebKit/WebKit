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

#include <cstdint>
#include <optional>
#include <wtf/StdLibExtras.h>
#include <wtf/WeakHashSet.h>

namespace WebCore {

class RenderBox;

// How far the flex algorithm has got. Monotonic: the flex formatting context advances it as it works through
// css-flexbox-1 9.2 - 9.6, and RenderFlexibleBox consults it to tell which of a flex item's sizes are settled.
enum class LayoutPhase : uint8_t {
    PreparingFlexItems,       // layoutBlock setup, before the flex algorithm runs.
    ComputingFlexBaseSizes,   // computeFlexBaseAndHypotheticalMainSizes (CSS Flexbox 9.2).
    CollectingLines,          // computeFlexLines (9.3 #5).
    ResolvingFlexibleLengths, // computeMainSizeForFlexItems (9.3 #6).
    MainAxisItemSizing,       // layoutFlexItems — items laid out at their resolved main size.
    CrossSizing,              // hypotheticalCrossSizeForFlexItems / crossSizeForFlexLines (9.4 #7-#8).
    MainAxisAlignment,        // handleMainAxisAlignment (9.5 #12).
    CrossAxisItemSizing,      // computeCrossSizeForFlexItems — stretch items to the line's cross size (9.4 #11).
    CrossAxisAlignment,       // handleCrossAxisAlignmentForFlexItems (9.6 #13-#14).
};

class FlexLayoutState {
public:
    // Which flex items had a margin trimmed, one set per side the container can trim. The flex algorithm fills
    // these in as it trims, and the items read them back as they lay out.
    struct MarginTrimItems {
        SingleThreadWeakHashSet<const RenderBox> itemsAtFlexLineStart;
        SingleThreadWeakHashSet<const RenderBox> itemsAtFlexLineEnd;
        SingleThreadWeakHashSet<const RenderBox> itemsOnFirstFlexLine;
        SingleThreadWeakHashSet<const RenderBox> itemsOnLastFlexLine;
    };

    FlexLayoutState() = default;
    // Starts from the margins the container trimmed before the algorithm ran, and adds to them as it trims.
    explicit FlexLayoutState(MarginTrimItems&& marginTrimItems)
        : m_marginTrimItems(WTF::move(marginTrimItems))
    {
    }

    const MarginTrimItems& marginTrimItems() const LIFETIME_BOUND { return m_marginTrimItems; }
    void addItemAtFlexLineStart(const RenderBox& flexItem) { m_marginTrimItems.itemsAtFlexLineStart.add(flexItem); }
    void addItemAtFlexLineEnd(const RenderBox& flexItem) { m_marginTrimItems.itemsAtFlexLineEnd.add(flexItem); }
    void addItemOnFirstFlexLine(const RenderBox& flexItem) { m_marginTrimItems.itemsOnFirstFlexLine.add(flexItem); }
    void addItemOnLastFlexLine(const RenderBox& flexItem) { m_marginTrimItems.itemsOnLastFlexLine.add(flexItem); }

    LayoutPhase phase() const { return m_phase; }
    void setPhase(LayoutPhase phase)
    {
        if (phase > m_phase)
            m_phase = phase;
    }

    void setFlexItemHasCompletedLayout(const RenderBox& flexItem) { m_flexItemsWithCompletedLayout.add(flexItem); }
    bool hasFlexItemCompletedLayout(const RenderBox& flexItem) const { return m_flexItemsWithCompletedLayout.contains(flexItem); }

    bool isFlexBoxBlockSizeDefinite() const { return m_isFlexBoxBlockSizeDefinite && *m_isFlexBoxBlockSizeDefinite; }
    bool isFlexBoxBlockSizeIndefinite() const { return m_isFlexBoxBlockSizeDefinite && !*m_isFlexBoxBlockSizeDefinite; }
    void setFlexBoxBlockSizeIsDefinite(bool isDefinite) { m_isFlexBoxBlockSizeDefinite = isDefinite; }
    void resetFlexBoxBlockSizeDefiniteness() { m_isFlexBoxBlockSizeDefinite = { }; }

private:
    MarginTrimItems m_marginTrimItems;
    LayoutPhase m_phase { LayoutPhase::PreparingFlexItems };
    SingleThreadWeakHashSet<const RenderBox> m_flexItemsWithCompletedLayout;
    std::optional<bool> m_isFlexBoxBlockSizeDefinite;
};

} // namespace WebCore
