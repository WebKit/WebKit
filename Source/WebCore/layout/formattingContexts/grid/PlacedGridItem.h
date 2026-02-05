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

#include "GridAreaLines.h"
#include "LayoutElementBox.h"
#include "StyleMargin.h"
#include "StyleMaximumSize.h"
#include "StyleMinimumSize.h"
#include "StylePreferredSize.h"
#include "StyleSelfAlignmentData.h"
#include <wtf/HashTraits.h>

namespace WebCore {
namespace Layout {

class UnplacedGridItem;

struct ComputedSizes {
    Style::PreferredSize preferredSize;
    Style::MinimumSize minimumSize;
    Style::MaximumSize maximumSize;

    Style::MarginEdge marginStart;
    Style::MarginEdge marginEnd;
};

class PlacedGridItem {
public:
    PlacedGridItem(const UnplacedGridItem&, GridAreaLines, const ComputedSizes& inlineAxisSizes, const ComputedSizes& blockAxisSizes,
    const StyleSelfAlignmentData& inlineAxisAlignment, const StyleSelfAlignmentData& blockAxisAlignment, const Style::ZoomFactor& usedZoom);

    const ComputedSizes& inlineAxisSizes() const { return m_inlineAxisSizes; }
    const ComputedSizes& blockAxisSizes() const { return m_blockAxisSizes; }

    size_t columnStartLine() const { return m_gridAreaLines.columnStartLine; }
    size_t columnEndLine() const { return m_gridAreaLines.columnEndLine; }
    size_t rowStartLine() const { return m_gridAreaLines.rowStartLine; }
    size_t rowEndLine() const { return m_gridAreaLines.rowEndLine; }

    const ElementBox& layoutBox() const { return m_layoutBox; }
    const StyleSelfAlignmentData& inlineAxisAlignment() const { return m_inlineAxisAlignment; }
    const StyleSelfAlignmentData& blockAxisAlignment() const { return m_blockAxisAlignment; }

    // FIXME: Add support for grid item's with preferred aspect ratios.
    bool hasPreferredAspectRatio() const { return false; }
    bool isReplacedElement() const { return m_layoutBox->isReplacedBox(); }

    const GridAreaLines& gridAreaLines() const { return m_gridAreaLines; }

    const Style::ZoomFactor& usedZoom() const { return m_usedZoom; }

private:
    const CheckedRef<const ElementBox> m_layoutBox;

    const ComputedSizes m_inlineAxisSizes;
    const ComputedSizes m_blockAxisSizes;

    const StyleSelfAlignmentData m_inlineAxisAlignment;
    const StyleSelfAlignmentData m_blockAxisAlignment;

    const Style::ZoomFactor m_usedZoom { 1.0f };

    GridAreaLines m_gridAreaLines;
};

} // namespace Layout
} // namespace WebCore
