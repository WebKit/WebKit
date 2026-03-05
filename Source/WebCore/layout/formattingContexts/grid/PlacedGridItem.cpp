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

#include "config.h"
#include "PlacedGridItem.h"

#include "GridAreaLines.h"
#include "UnplacedGridItem.h"

namespace WebCore {
namespace Layout {

PlacedGridItem::PlacedGridItem(const ElementBox& gridItem, const GridAreaLines& gridAreaLines, const BoxGeometry& gridItemGeometry, const RenderStyle& gridContainerStyle)
    : PlacedGridItem(gridItem, gridAreaLines, gridItemGeometry, gridContainerStyle, gridItem.style())
{
}

PlacedGridItem::PlacedGridItem(const ElementBox& gridItem, const GridAreaLines& gridAreaLines, const BoxGeometry& gridItemGeometry, const RenderStyle& gridContainerStyle, const RenderStyle& gridItemStyle)
    : m_layoutBox(gridItem)
    , m_inlineAxisSizes({ gridItemStyle.width(), gridItemStyle.minWidth(), gridItemStyle.maxWidth(), gridItemStyle.marginLeft(), gridItemStyle.marginRight() })
    , m_blockAxisSizes({ gridItemStyle.height(), gridItemStyle.minHeight(), gridItemStyle.maxHeight(), gridItemStyle.marginTop(), gridItemStyle.marginBottom() })
    , m_usedInlineBorderAndPadding(gridItemGeometry.horizontalBorderAndPadding())
    , m_usedBlockBorderAndPadding(gridItemGeometry.verticalBorderAndPadding())
    , m_inlineAxisAlignment(gridItemStyle.justifySelf().resolve(&gridContainerStyle))
    , m_blockAxisAlignment(gridItemStyle.alignSelf().resolve(&gridContainerStyle))
    , m_writingMode(gridItemStyle.writingMode())
    , m_usedZoom(gridItemStyle.usedZoomForLength())
    , m_gridAreaLines(gridAreaLines)
{
}

} // namespace Layout
} // namespace WebCore
