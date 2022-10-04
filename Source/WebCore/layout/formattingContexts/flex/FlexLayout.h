/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#include "FlexFormattingConstraints.h"
#include "FlexFormattingState.h"
#include "FlexRect.h"
#include <wtf/Range.h>

namespace WebCore {
namespace Layout {

// This class implements the layout logic for flex formatting contexts.
// https://www.w3.org/TR/css-flexbox-1/
class FlexLayout {
public:
    FlexLayout(const ElementBox& flexBox);

    struct LogicalFlexItem {
    public:
        struct LogicalTypes {
            LengthType width { LengthType::Auto };
            LengthType height { LengthType::Auto };

            LengthType leftMargin { LengthType::Auto };
            LengthType rightMargin { LengthType::Auto };
            LengthType topMargin { LengthType::Auto };
            LengthType bottomMargin { LengthType::Auto };
        };
        LogicalFlexItem(LayoutSize marginBoxSize, LogicalTypes, IntrinsicWidthConstraints, const ElementBox&);
        LogicalFlexItem() = default;

        LayoutUnit flexBasis() const { return m_marginBoxSize.width(); }

        LayoutUnit width() const { return std::min(maximumSize(), std::max(minimumSize(), m_marginBoxSize.width())); }
        LayoutUnit height() const { return m_marginBoxSize.height(); }

        bool isHeightAuto() const { return m_logicalTypes.height == LengthType::Auto; }

        bool hasAutoMarginLeft() const { return m_logicalTypes.leftMargin == LengthType::Auto; }
        bool hasAutoMarginRight() const { return m_logicalTypes.rightMargin == LengthType::Auto; }
        bool hasAutoMarginTop() const { return m_logicalTypes.topMargin == LengthType::Auto; }
        bool hasAutoMarginBottom() const { return m_logicalTypes.bottomMargin == LengthType::Auto; }

        LayoutUnit minimumSize() const { return m_intrinsicWidthConstraints.minimum; }
        LayoutUnit maximumSize() const { return m_intrinsicWidthConstraints.maximum; }

        const RenderStyle& style() const { return m_layoutBox->style(); }
        const ElementBox& layoutBox() const { return *m_layoutBox; }

    private:
        LayoutSize m_marginBoxSize;
        LogicalTypes m_logicalTypes { };
        IntrinsicWidthConstraints m_intrinsicWidthConstraints { };
        CheckedPtr<const ElementBox> m_layoutBox;        
    };
    using LogicalFlexItems = Vector<LogicalFlexItem>;
    struct LogicalConstraints {
        std::optional<LayoutUnit> verticalSpace;
        struct HorizontalSpace {
            std::optional<LayoutUnit> available; // This is how much space there is for flexing in main axis direction.
            std::optional<LayoutUnit> minimum; // This is how much space is at least available for flexing in main axis direction.
        };
        HorizontalSpace horizontalSpace;
    };
    struct FlexItemRect {
        FlexRect& operator()() { return marginRect; }

        FlexRect marginRect;
        struct AutoMargin {
            std::optional<LayoutUnit> left;
            std::optional<LayoutUnit> right;
            std::optional<LayoutUnit> top;
            std::optional<LayoutUnit> bottom;
        };
        AutoMargin autoMargin;
    };
    using LogicalFlexItemRects = Vector<FlexItemRect>;
    LogicalFlexItemRects layout(const LogicalConstraints&, const LogicalFlexItems&);

private:
    using LineRange = WTF::Range<size_t>;
    void computeLogicalWidthForFlexItems(const LogicalFlexItems&, const LineRange&, LayoutUnit availableSpace, LogicalFlexItemRects&);
    void computeLogicalWidthForStretchingFlexItems(const LogicalFlexItems&, const LineRange&, LayoutUnit availableSpace, LogicalFlexItemRects&);
    void computeLogicalWidthForShrinkingFlexItems(const LogicalFlexItems&, const LineRange&, LayoutUnit availableSpace, LogicalFlexItemRects&);
    void computeLogicalHeightForFlexItems(const LogicalFlexItems&, const LineRange&, LayoutUnit availableSpace, LogicalFlexItemRects&);
    void alignFlexItems(const LogicalFlexItems&, const LineRange&, VerticalConstraints, LogicalFlexItemRects&);
    void justifyFlexItems(const LogicalFlexItems&, const LineRange&, LayoutUnit availableSpace, LogicalFlexItemRects&);
    void distributeMarginAutoInMainAxis(const LogicalFlexItems&, const LineRange&, LayoutUnit availableSpace, LogicalFlexItemRects&);
    void distributeMarginAutoInCrossAxis(const LogicalFlexItems&, const LineRange&, LayoutUnit availableSpace, LogicalFlexItemRects&);

    using WrappingPositions = Vector<size_t>;
    WrappingPositions computeWrappingPositions(const LogicalFlexItems&, LayoutUnit availableSpace) const;
    LayoutUnit computeAvailableLogicalHorizontalSpace(const LogicalFlexItems&, const LogicalConstraints&) const;

    using LineHeightList = Vector<LayoutUnit>;
    LineHeightList computeAvailableLogicalVerticalSpace(const LogicalFlexItems&, const WrappingPositions&, const LogicalConstraints&) const;

    const ElementBox& flexBox() const { return m_flexBox; }
    const RenderStyle& flexBoxStyle() const { return flexBox().style(); }

    const ElementBox& m_flexBox;
};

inline FlexLayout::LogicalFlexItem::LogicalFlexItem(LayoutSize marginBoxSize, LogicalTypes logicalTypes, IntrinsicWidthConstraints intrinsicWidthConstraints, const ElementBox& layoutBox)
    : m_marginBoxSize(marginBoxSize)
    , m_logicalTypes(logicalTypes)
    , m_intrinsicWidthConstraints(intrinsicWidthConstraints)
    , m_layoutBox(layoutBox)
{
}

}
}

