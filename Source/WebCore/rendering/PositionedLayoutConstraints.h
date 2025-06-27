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
#include "LayoutRange.h"
#include "RenderBox.h"
#include "StyleInset.h"
#include "StyleMargin.h"
#include "StyleSelfAlignmentData.h"

namespace WebCore {

class PositionedLayoutConstraints {
public:
    PositionedLayoutConstraints(const RenderBox&, LogicalBoxAxis selfAxis);
    PositionedLayoutConstraints(const RenderBox&, const RenderStyle& selfStyleOverride, LogicalBoxAxis selfAxis);

    // Logical top or left wrt containing block.
    Style::MarginEdge marginBefore() const { return m_marginBefore; }
    // Logical bottom or right wrt containing block.
    Style::MarginEdge marginAfter() const { return m_marginAfter; }
    Style::InsetEdge insetBefore() const { return m_insetBefore; }
    Style::InsetEdge insetAfter() const { return m_insetAfter; }
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
    bool startIsBefore() const { return m_containingAxis == LogicalBoxAxis::Block || m_containingWritingMode.isLogicalLeftInlineStart(); }
    bool containingCoordsAreFlipped() const;

    LayoutUnit containingSize() const { return m_containingRange.size(); }
    LayoutUnit marginBeforeValue() const { return Style::evaluateMinimum(m_marginBefore, m_marginPercentageBasis); }
    LayoutUnit marginAfterValue() const { return Style::evaluateMinimum(m_marginAfter, m_marginPercentageBasis); }
    LayoutUnit insetBeforeValue() const { return Style::evaluateMinimum(m_insetBefore, containingSize()); }
    LayoutUnit insetAfterValue() const { return Style::evaluateMinimum(m_insetAfter, containingSize()); }
    LayoutUnit insetModifiedContainingSize() const { return m_insetModifiedContainingRange.size(); }
    LayoutUnit availableContentSpace() const { return insetModifiedContainingSize() - marginBeforeValue() - bordersPlusPadding() - marginAfterValue(); } // This may be negative.

    void resolvePosition(RenderBox::LogicalExtentComputedValues&) const;
    LayoutUnit resolveAlignmentShift(const LayoutUnit unusedSpace, const LayoutUnit itemSize) const;
    ItemPosition resolveAlignmentValue() const;
    bool alignmentAppliesStretch(ItemPosition normalAlignment) const;

    void fixupLogicalLeftPosition(RenderBox::LogicalExtentComputedValues&) const;
    void fixupLogicalTopPosition(RenderBox::LogicalExtentComputedValues&, const RenderBox& renderer) const;

private:
    void captureInsets(const RenderBox&, const LogicalBoxAxis selfAxis);
    void captureGridArea(const RenderBox&);
    void captureAnchorGeometry(const RenderBox&);
    LayoutRange adjustForPositionArea(const LayoutRange rangeToAdjust, const LayoutRange anchorArea, const BoxAxis containerAxis);

    bool supportsNewStaticPositionPath(const RenderBox&, LogicalBoxAxis selfAxis) const;
    LayoutUnit newComputeInlineStaticPosition(const RenderBox&) const;

    void computeStaticPosition(const RenderBox&, LogicalBoxAxis selfAxis);
    void computeInlineStaticDistance(const RenderBox&);
    void computeBlockStaticDistance(const RenderBox&);

    CheckedPtr<const RenderBoxModelObject> m_container;
    const WritingMode m_containingWritingMode;
    const WritingMode m_writingMode;
    const BoxAxis m_physicalAxis;
    const LogicalBoxAxis m_containingAxis;
    const RenderStyle& m_style;
    StyleSelfAlignmentData m_alignment;
    const CheckedPtr<const RenderBoxModelObject> m_defaultAnchorBox; // Only set if needed.

    LayoutRange m_anchorArea; // Only valid if defaultAnchor exists.
    LayoutRange m_containingRange;
    LayoutRange m_originalContainingRange;
    LayoutRange m_insetModifiedContainingRange;
    LayoutUnit m_marginPercentageBasis;

    LayoutUnit m_bordersPlusPadding;
    Style::MarginEdge m_marginBefore;
    Style::MarginEdge m_marginAfter;
    Style::InsetEdge m_insetBefore;
    Style::InsetEdge m_insetAfter;
    bool m_useStaticPosition { false };
};

}
