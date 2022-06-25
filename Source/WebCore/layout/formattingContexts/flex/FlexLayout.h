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

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

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
    FlexLayout(const RenderStyle& flexBoxStyle);

    struct LogicalFlexItem {
    public:
        LogicalFlexItem(const FlexRect&, LengthType widthType, LengthType heightType, IntrinsicWidthConstraints, const ContainerBox&);
        LogicalFlexItem() = default;

        void setTop(LayoutUnit top) { m_marginRect.value.setTop(top); }
        void setLeft(LayoutUnit left) { m_marginRect.value.setLeft(left); }
        void setWidth(LayoutUnit width) { m_marginRect.value.setWidth(width); }
        void setHeight(LayoutUnit height) { m_marginRect.value.setHeight(height); }

        FlexRect rect() const { return m_marginRect.value; }
        LayoutPoint topLeft() const { return m_marginRect.value.topLeft(); }
        LayoutUnit top() const { return m_marginRect.value.top(); }
        LayoutUnit bottom() const { return m_marginRect.value.bottom(); }
        LayoutUnit left() const { return m_marginRect.value.left(); }
        LayoutUnit right() const { return m_marginRect.value.right(); }
        LayoutUnit width() const { return m_marginRect.value.width(); }
        LayoutUnit height() const { return m_marginRect.value.height(); }

        bool isHeightAuto() const { return m_marginRect.heightType == LengthType::Auto; }

        LayoutUnit minimumContentWidth() const { return m_intrinsicWidthConstraints.minimum; }

        const RenderStyle& style() const { return m_layoutBox->style(); }
        const ContainerBox& layoutBox() const { return *m_layoutBox; }

    private:
        struct MarginRect {
            FlexRect value;
            LengthType widthType { LengthType::Auto };
            LengthType heightType { LengthType::Auto };
        };
        MarginRect m_marginRect { };
        IntrinsicWidthConstraints m_intrinsicWidthConstraints { };
        CheckedPtr<const ContainerBox> m_layoutBox;        
    };
    using LogicalFlexItems = Vector<LogicalFlexItem>;
    struct LogicalConstraints {
        std::optional<LayoutUnit> verticalSpace;
        std::optional<LayoutUnit> horizontalSpace;
    };
    void layout(const LogicalConstraints&, LogicalFlexItems&);

private:
    using LineRange = WTF::Range<size_t>;
    void computeLogicalWidthForFlexItems(LogicalFlexItems&, const LineRange&, LayoutUnit availableSpace);
    void computeLogicalWidthForStretchingFlexItems(LogicalFlexItems&, const LineRange&, LayoutUnit availableSpace);
    void computeLogicalWidthForShrinkingFlexItems(LogicalFlexItems&, const LineRange&, LayoutUnit availableSpace);
    void computeLogicalHeightForFlexItems(LogicalFlexItems&, const LineRange&, LayoutUnit availableSpace);
    void alignFlexItems(LogicalFlexItems&, const LineRange&, VerticalConstraints);
    void justifyFlexItems(LogicalFlexItems&, const LineRange&, LayoutUnit availableSpace);

    using WrappingPositions = Vector<size_t>;
    WrappingPositions computeWrappingPositions(const LogicalFlexItems&, LayoutUnit availableSpace) const;
    LayoutUnit computeAvailableLogicalHorizontalSpace(const LogicalFlexItems&, const LogicalConstraints&) const;

    using LineHeightList = Vector<LayoutUnit>;
    LineHeightList computeAvailableLogicalVerticalSpace(const LogicalFlexItems&, const WrappingPositions&, const LogicalConstraints&) const;

    const RenderStyle& flexBoxStyle() const { return m_flexBoxStyle; }

    const RenderStyle& m_flexBoxStyle;
};

inline FlexLayout::LogicalFlexItem::LogicalFlexItem(const FlexRect& marginRect, LengthType widthType, LengthType heightType, IntrinsicWidthConstraints intrinsicWidthConstraints, const ContainerBox& layoutBox)
    : m_marginRect({ marginRect, widthType, heightType })
    , m_intrinsicWidthConstraints(intrinsicWidthConstraints)
    , m_layoutBox(layoutBox)
{
}

}
}

#endif
