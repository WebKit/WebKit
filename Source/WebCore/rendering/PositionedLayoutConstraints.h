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

#include "BoxSides.h"
#include "RenderBox.h"

namespace WebCore {

// This is basically a 1-dimentional LayoutRect.
class LayoutRange {
public:
    LayoutRange() = default;
    LayoutRange(LayoutUnit location, LayoutUnit size)
        : m_location(location)
        , m_size(size)
    { }
    LayoutUnit min() const { return m_location; }
    LayoutUnit max() const { return m_location + m_size; }
    LayoutUnit size() const { return m_size; }

    void set(LayoutUnit location, LayoutUnit size)
    {
        m_location = location;
        m_size = size;
    }
    void reset(LayoutUnit size = 0_lu)
    {
        m_location = 0_lu;
        m_size = size;
    }
    void moveBy(LayoutUnit shift) { m_location += shift; }
    void moveTo(LayoutUnit location) { m_location = location; }
    void shiftMinEdgeBy(LayoutUnit shift)
    {
        m_location += shift;
        m_size -= shift;
    }
    void shiftMaxEdgeBy(LayoutUnit shift) { m_size += shift; }
    void shiftMinEdgeTo(LayoutUnit target) { shiftMinEdgeBy(target - min()); }
    void shiftMaxEdgeTo(LayoutUnit target) { shiftMaxEdgeBy(target - max()); }

private:
    LayoutUnit m_location;
    LayoutUnit m_size;
};

// Convenience struct to package constraints and inputs.
struct PositionedLayoutConstraints {
public:
    PositionedLayoutConstraints(const RenderBox&, const RenderStyle&, LogicalBoxAxis);
    PositionedLayoutConstraints(const RenderBox&, LogicalBoxAxis);

    // Logical top or left wrt containing block.
    Length marginBefore() const { return m_marginBefore; }
    // Logical bottom or right wrt containing block.
    Length marginAfter() const { return m_marginAfter; }
    Length insetBefore() const { return m_insetBefore; }
    Length insetAfter() const { return m_insetAfter; }
    const RenderBoxModelObject& container() const { return *m_container; }
    const RenderBoxModelObject* defaultAnchorBox() const { return m_defaultAnchorBox.get(); }
    LayoutUnit bordersPlusPadding() const { return m_bordersPlusPadding; }
    const StyleSelfAlignmentData& alignment() const { return m_alignment; }
    LogicalBoxAxis containingAxis() const { return m_containingAxis; }
    BoxAxis physicalAxis() const { return m_physicalAxis; }
    WritingMode containingWritingMode() const { return m_containingWritingMode; }

    bool needsAnchor() const;
    bool isOrthogonal() const { return m_containingWritingMode.isOrthogonal(m_writingMode); }
    bool isBlockOpposing() const { return m_containingWritingMode.isBlockOpposing(m_writingMode); }
    bool isBlockFlipped() const { return m_containingWritingMode.isBlockFlipped(); }
    bool isLogicalLeftInlineStart() const { return m_containingWritingMode.isLogicalLeftInlineStart(); }
    bool containingCoordsAreFlipped() const;

    LayoutUnit containingSize() const { return m_containingRange.size(); }
    LayoutUnit marginBeforeValue() const { return minimumValueForLength(m_marginBefore, m_marginPercentageBasis); }
    LayoutUnit marginAfterValue() const { return minimumValueForLength(m_marginAfter, m_marginPercentageBasis); }
    LayoutUnit insetBeforeValue() const { return minimumValueForLength(m_insetBefore, containingSize()); }
    LayoutUnit insetAfterValue() const { return minimumValueForLength(m_insetAfter, containingSize()); }
    LayoutUnit insetModifiedContainingSize() const { return m_insetModifiedContainingRange.size(); }
    LayoutUnit availableContentSpace() const { return insetModifiedContainingSize() - marginBeforeValue() - bordersPlusPadding() - marginAfterValue(); } // This may be negative.

    void resolvePosition(RenderBox::LogicalExtentComputedValues&) const;
    LayoutUnit resolveAlignmentAdjustment(const LayoutUnit unusedSpace) const;
    ItemPosition resolveAlignmentPosition() const;
    bool alignmentAppliesStretch(ItemPosition normalAlignment) const;

    void fixupLogicalLeftPosition(RenderBox::LogicalExtentComputedValues&) const;
    void fixupLogicalTopPosition(RenderBox::LogicalExtentComputedValues&, const RenderBox& renderer) const;

private:
    void captureInsets(const RenderBox&, const LogicalBoxAxis selfAxis);
    void computeAnchorGeometry(const RenderBox&);
    LayoutRange adjustForPositionArea(const LayoutRange rangeToAdjust, const LayoutRange anchorArea, const BoxAxis containerAxis);

    void computeInlineStaticDistance(const RenderBox&);
    void computeBlockStaticDistance(const RenderBox&);

private:
    // These values are captured by the constructor and may be tweaked by the user.
    Length m_marginBefore;
    Length m_marginAfter;
    Length m_insetBefore;
    Length m_insetAfter;

    // These values are calculated by the constructor.
    LayoutRange m_containingRange;
    LayoutRange m_insetModifiedContainingRange;
    LayoutUnit m_marginPercentageBasis;
    CheckedPtr<const RenderBoxModelObject> m_container;
    const WritingMode m_containingWritingMode;
    const WritingMode m_writingMode;
    const BoxAxis m_physicalAxis;
    const LogicalBoxAxis m_containingAxis;
    const StyleSelfAlignmentData& m_alignment;
    const RenderStyle& m_style;
    const CheckedPtr<const RenderBoxModelObject> m_defaultAnchorBox; // Only set if needed.
    LayoutRange m_anchorArea; // Only valid if defaultAnchor exists.

    // These values are cached by the constructor, and should not be changed afterwards.
    LayoutUnit m_bordersPlusPadding;
    bool m_useStaticPosition { false };
};

}
