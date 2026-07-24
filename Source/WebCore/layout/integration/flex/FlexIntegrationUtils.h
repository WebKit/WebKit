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

#include <WebCore/LayoutUnit.h>
#include <wtf/CheckedRef.h>

namespace WebCore {

enum class LogicalBoxAxis : uint8_t;

namespace Style { enum class MarginTrimSide : uint8_t; }

struct FlexContainerUsedExtents;

class FlexLayoutItem;
class FlexLayoutState;
class RenderFlexibleBox;

namespace LayoutIntegration {

class FlexIntegrationUtils {
public:
    FlexIntegrationUtils(RenderFlexibleBox&);

    RenderFlexibleBox& flexBox() const LIFETIME_BOUND { return m_flexBox; }
    FlexLayoutState& flexLayoutState() const;

    void applyStretchedLogicalHeightToFlexItem(const FlexLayoutItem&, LayoutUnit blockSize);
    void layoutFlexItemForStretchedCrossSize(const FlexLayoutItem&, LayoutUnit crossSize, LogicalBoxAxis crossAxis);
    void layoutFlexItemWithMainSize(FlexLayoutItem&, LayoutUnit mainSize);
    FlexContainerUsedExtents updateFlexContainerLogicalHeight(LayoutUnit flexContentBlockExtent);

    void setTrimmedMarginForChild(const FlexLayoutItem&, Style::MarginTrimSide);
    LayoutUnit adjustBorderBoxLogicalWidthForBoxSizing(LayoutUnit computedLogicalWidth) const;

    void addItemAtFlexLineStart(const FlexLayoutItem&);
    void addItemAtFlexLineEnd(const FlexLayoutItem&);
    void addItemOnFirstFlexLine(const FlexLayoutItem&);
    void addItemOnLastFlexLine(const FlexLayoutItem&);
    bool flexItemHasPercentHeightDescendants(const FlexLayoutItem&) const;

private:
    const CheckedRef<RenderFlexibleBox> m_flexBox;
};

} // namespace LayoutIntegration
} // namespace WebCore
