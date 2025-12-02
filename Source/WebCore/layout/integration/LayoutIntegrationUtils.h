/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#include <WebCore/BlockLayoutState.h>
#include <WebCore/LayoutPoint.h>
#include <WebCore/LayoutUnit.h>
#include <WebCore/RenderBlockFlow.h>

#include <optional>
#include <wtf/CheckedRef.h>

namespace WebCore {
namespace Layout {

class ElementBox;
class InlineLayoutState;
class LayoutState;

class IntegrationUtils {
public:
    IntegrationUtils(const LayoutState&);

    void layoutWithFormattingContextForBox(const ElementBox&, std::optional<LayoutUnit> widthConstraint = { }, std::optional<LayoutUnit> heightConstraint = { }) const;
    LayoutUnit maxContentWidth(const ElementBox&) const;
    LayoutUnit minContentWidth(const ElementBox&) const;
    LayoutUnit minContentHeight(const ElementBox&) const;
    LayoutUnit preferredMinWidth(const ElementBox&) const;
    LayoutUnit preferredMaxWidth(const ElementBox&) const;
    void layoutWithFormattingContextForBlockInInline(const ElementBox& block, LayoutPoint blockLogicalTopLeft, const InlineLayoutState&) const;

    static BlockLayoutState::MarginState toMarginState(const RenderBlockFlow::MarginInfo&);
    static RenderBlockFlow::MarginInfo toMarginInfo(const Layout::BlockLayoutState::MarginState&);
    static std::pair<LayoutRect, LayoutRect> toMarginAndBorderBoxVisualRect(const BoxGeometry& logicalGeometry, const LayoutSize& containerSize, WritingMode);

private:
    const CheckedRef<const LayoutState> m_globalLayoutState;
};

}
}

