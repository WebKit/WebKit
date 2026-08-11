/*
 * Copyright (C) 2026 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ScrollerPairCoordinated.h"

#if USE(COORDINATED_GRAPHICS_ASYNC_SCROLLBAR)

#include "ScrollTypes.h"
#include "ScrollerCoordinated.h"
#include "ScrollingTreeScrollingNode.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(ScrollerPairCoordinated);

ScrollerPairCoordinated::ScrollerPairCoordinated(ScrollingTreeScrollingNode& node)
    : m_scrollingNode(node)
    , m_verticalScroller(makeUniqueRef<ScrollerCoordinated>(*this, ScrollbarOrientation::Vertical))
    , m_horizontalScroller(makeUniqueRef<ScrollerCoordinated>(*this, ScrollbarOrientation::Horizontal))
{
}

ScrollerPairCoordinated::~ScrollerPairCoordinated() = default;

void ScrollerPairCoordinated::updateValues()
{
    RefPtr node = m_scrollingNode.get();
    if (!node)
        return;

    m_horizontalScroller->updateValues();
    m_verticalScroller->updateValues();
}

ScrollerPairCoordinated::Values ScrollerPairCoordinated::valuesForOrientation(ScrollbarOrientation orientation)
{
    RefPtr node = m_scrollingNode.get();
    if (!node)
        return { };

    float position;
    float totalSize;
    float visibleSize;
    if (orientation == ScrollbarOrientation::Vertical) {
        position = node->currentScrollOffset().y();
        totalSize = node->totalContentsSize().height();
        visibleSize = node->scrollableAreaSize().height();
    } else {
        position = node->currentScrollOffset().x();
        totalSize = node->totalContentsSize().width();
        visibleSize = node->scrollableAreaSize().width();
    }

    float value;
    float overhang;
    ScrollableArea::computeScrollbarValueAndOverhang(position, totalSize, visibleSize, value, overhang);

    float proportion = totalSize ? (visibleSize - overhang) / totalSize : 1;

    return { value, proportion, visibleSize };
}

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS_ASYNC_SCROLLBAR)
