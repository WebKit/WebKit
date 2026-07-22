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

#include <WebCore/FlexFormattingContext.h>
#include <wtf/CheckedRef.h>

namespace WebCore {

class RenderFlexibleBox;

namespace LayoutIntegration {

class FlexLayout {
public:
    FlexLayout(RenderFlexibleBox&);

    void layout(RelayoutChildren);

    std::optional<LayoutUnit> firstLineBaseline() const;
    std::optional<LayoutUnit> lastLineBaseline() const;

    // Sets the static position of an out-of-flow flex item; returns true if it changed.
    bool setStaticPositionForPositionedLayout(const RenderBox&);

private:
    FlexLayoutItems collectFlexItems(RelayoutChildren);
    void prepareFlexItemForPositionedLayout(RenderBox&);
    const RenderBox* flexItemForFirstBaseline() const;
    const RenderBox* flexItemForLastBaseline() const;
    const RenderBox* baselineFlexItemInLine(size_t lineStart, size_t itemCount, bool reverse) const;
    LayoutUnit staticMainAxisPositionForPositionedFlexItem(const RenderBox&);
    LayoutUnit staticCrossAxisPositionForPositionedFlexItem(const RenderBox&);
    LayoutUnit staticInlinePositionForPositionedFlexItem(const RenderBox&);
    LayoutUnit staticBlockPositionForPositionedFlexItem(const RenderBox&);

    RenderFlexibleBox& flexBox() const LIFETIME_BOUND { return m_flexBox; }

    const CheckedRef<RenderFlexibleBox> m_flexBox;
};

}
}
