/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "TableFormattingContext.h"

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "LayoutBox.h"
#include "TableFormattingState.h"

namespace WebCore {
namespace Layout {

ContentHeightAndMargin TableFormattingContext::Geometry::tableCellHeightAndMargin(const Box& layoutBox) const
{
    ASSERT(layoutBox.isInFlow());

    auto height = computedContentHeight(layoutBox);
    if (!height)
        height = contentHeightForFormattingContextRoot(layoutBox);

    // Margins don't apply to internal table elements.
    return ContentHeightAndMargin { *height, { } };
}

Optional<LayoutUnit> TableFormattingContext::Geometry::computedColumnWidth(const Box& columnBox) const
{
    // Check both style and <col>'s width attribute.
    // FIXME: Figure out what to do with calculated values, like <col style="width: 10%">.
    if (auto computedWidthValue = computedContentWidth(columnBox, { }))
        return computedWidthValue;
    return columnBox.columnWidth();
}

}
}

#endif
