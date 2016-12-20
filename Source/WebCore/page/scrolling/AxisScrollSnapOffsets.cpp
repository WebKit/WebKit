/*
 * Copyright (C) 2014-2015 Apple Inc. All rights reserved.
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
#include "AxisScrollSnapOffsets.h"

#include "ElementChildIterator.h"
#include "HTMLCollection.h"
#include "HTMLElement.h"
#include "Length.h"
#include "Logging.h"
#include "RenderBox.h"
#include "RenderView.h"
#include "ScrollableArea.h"
#include "StyleScrollSnapPoints.h"

#if ENABLE(CSS_SCROLL_SNAP)

namespace WebCore {

enum class InsetOrOutset {
    Inset,
    Outset
};

static LayoutRect computeScrollSnapPortOrAreaRect(const LayoutRect& rect, const LengthBox& insetOrOutsetBox, InsetOrOutset insetOrOutset)
{
    LayoutBoxExtent extents(valueForLength(insetOrOutsetBox.top(), rect.height()), valueForLength(insetOrOutsetBox.right(), rect.width()), valueForLength(insetOrOutsetBox.bottom(), rect.height()), valueForLength(insetOrOutsetBox.left(), rect.width()));
    auto snapPortOrArea(rect);
    if (insetOrOutset == InsetOrOutset::Inset)
        snapPortOrArea.contract(extents);
    else
        snapPortOrArea.expand(extents);
    return snapPortOrArea;
}

static LayoutUnit computeScrollSnapAlignOffset(const LayoutUnit& leftOrTop, const LayoutUnit& widthOrHeight, ScrollSnapAxisAlignType alignment)
{
    switch (alignment) {
    case ScrollSnapAxisAlignType::Start:
        return leftOrTop;
    case ScrollSnapAxisAlignType::Center:
        return leftOrTop + widthOrHeight / 2;
    case ScrollSnapAxisAlignType::End:
        return leftOrTop + widthOrHeight;
    default:
        ASSERT_NOT_REACHED();
        return 0;
    }
}

#if !LOG_DISABLED

static String snapOffsetsToString(const Vector<LayoutUnit>& snapOffsets)
{
    StringBuilder s;
    s.append("[ ");
    for (auto offset : snapOffsets)
        s.append(String::format("%.1f ", offset.toFloat()));

    s.append("]");
    return s.toString();
}

static String snapPortOrAreaToString(const LayoutRect& rect)
{
    return String::format("{{%.1f, %.1f} {%.1f, %.1f}}", rect.x().toFloat(), rect.y().toFloat(), rect.width().toFloat(), rect.height().toFloat());
}

#endif

void updateSnapOffsetsForScrollableArea(ScrollableArea& scrollableArea, HTMLElement& scrollingElement, const RenderBox& scrollingElementBox, const RenderStyle& scrollingElementStyle)
{
    auto* scrollContainer = scrollingElement.renderer();
    auto scrollSnapType = scrollingElementStyle.scrollSnapType();
    if (!scrollContainer || scrollSnapType.strictness == ScrollSnapStrictness::None || scrollContainer->view().boxesWithScrollSnapPositions().isEmpty()) {
        scrollableArea.clearHorizontalSnapOffsets();
        scrollableArea.clearVerticalSnapOffsets();
        return;
    }

    auto verticalSnapOffsets = std::make_unique<Vector<LayoutUnit>>();
    auto horizontalSnapOffsets = std::make_unique<Vector<LayoutUnit>>();
    HashSet<float> seenVerticalSnapOffsets;
    HashSet<float> seenHorizontalSnapOffsets;
    bool hasHorizontalSnapOffsets = scrollSnapType.axis == ScrollSnapAxis::Both || scrollSnapType.axis == ScrollSnapAxis::XAxis || scrollSnapType.axis == ScrollSnapAxis::Inline;
    bool hasVerticalSnapOffsets = scrollSnapType.axis == ScrollSnapAxis::Both || scrollSnapType.axis == ScrollSnapAxis::YAxis || scrollSnapType.axis == ScrollSnapAxis::Block;
    auto maxScrollLeft = scrollingElementBox.scrollWidth() - scrollingElementBox.contentWidth();
    auto maxScrollTop = scrollingElementBox.scrollHeight() - scrollingElementBox.contentHeight();
    LayoutPoint containerScrollOffset(scrollingElementBox.scrollLeft(), scrollingElementBox.scrollTop());

    // The bounds of the scrolling container's snap port, where the top left of the scrolling container's border box is the origin.
    auto scrollSnapPort = computeScrollSnapPortOrAreaRect(scrollingElementBox.paddingBoxRect(), scrollingElementStyle.scrollPadding(), InsetOrOutset::Inset);
#if !LOG_DISABLED
    LOG(Scrolling, "Computing scroll snap offsets in snap port: %s", snapPortOrAreaToString(scrollSnapPort).utf8().data());
#endif
    for (auto* child : scrollContainer->view().boxesWithScrollSnapPositions()) {
        if (child->findEnclosingScrollableContainer() != scrollContainer)
            continue;

        // The bounds of the child element's snap area, where the top left of the scrolling container's border box is the origin.
        // The snap area is the bounding box of the child element's border box, after applying transformations.
        auto scrollSnapArea = LayoutRect(child->localToContainerQuad(FloatQuad(child->borderBoundingBox()), scrollingElement.renderBox()).boundingBox());
        scrollSnapArea.moveBy(containerScrollOffset);
        scrollSnapArea = computeScrollSnapPortOrAreaRect(scrollSnapArea, child->style().scrollSnapMargin(), InsetOrOutset::Outset);
#if !LOG_DISABLED
        LOG(Scrolling, "    Considering scroll snap area: %s", snapPortOrAreaToString(scrollSnapArea).utf8().data());
#endif
        auto alignment = child->style().scrollSnapAlign();
        if (hasHorizontalSnapOffsets && alignment.x != ScrollSnapAxisAlignType::None) {
            auto absoluteScrollOffset = clampTo<LayoutUnit>(computeScrollSnapAlignOffset(scrollSnapArea.x(), scrollSnapArea.width(), alignment.x) - computeScrollSnapAlignOffset(scrollSnapPort.x(), scrollSnapPort.width(), alignment.x), 0, maxScrollLeft);
            if (!seenHorizontalSnapOffsets.contains(absoluteScrollOffset)) {
                seenHorizontalSnapOffsets.add(absoluteScrollOffset);
                horizontalSnapOffsets->append(absoluteScrollOffset);
            }
        }
        if (hasVerticalSnapOffsets && alignment.y != ScrollSnapAxisAlignType::None) {
            auto absoluteScrollOffset = clampTo<LayoutUnit>(computeScrollSnapAlignOffset(scrollSnapArea.y(), scrollSnapArea.height(), alignment.y) - computeScrollSnapAlignOffset(scrollSnapPort.y(), scrollSnapPort.height(), alignment.y), 0, maxScrollTop);
            if (!seenVerticalSnapOffsets.contains(absoluteScrollOffset)) {
                seenVerticalSnapOffsets.add(absoluteScrollOffset);
                verticalSnapOffsets->append(absoluteScrollOffset);
            }
        }
    }

    std::sort(horizontalSnapOffsets->begin(), horizontalSnapOffsets->end());
    if (horizontalSnapOffsets->size()) {
        if (horizontalSnapOffsets->last() != maxScrollLeft)
            horizontalSnapOffsets->append(maxScrollLeft);
        if (horizontalSnapOffsets->first())
            horizontalSnapOffsets->insert(0, 0);
#if !LOG_DISABLED
        LOG(Scrolling, " => Computed horizontal scroll snap offsets: %s", snapOffsetsToString(*horizontalSnapOffsets).utf8().data());
#endif
        scrollableArea.setHorizontalSnapOffsets(WTFMove(horizontalSnapOffsets));
    } else
        scrollableArea.clearHorizontalSnapOffsets();

    std::sort(verticalSnapOffsets->begin(), verticalSnapOffsets->end());
    if (verticalSnapOffsets->size()) {
        if (verticalSnapOffsets->last() != maxScrollTop)
            verticalSnapOffsets->append(maxScrollTop);
        if (verticalSnapOffsets->first())
            verticalSnapOffsets->insert(0, 0);
#if !LOG_DISABLED
        LOG(Scrolling, " => Computed vertical scroll snap offsets: %s", snapOffsetsToString(*verticalSnapOffsets).utf8().data());
#endif
        scrollableArea.setVerticalSnapOffsets(WTFMove(verticalSnapOffsets));
    } else
        scrollableArea.clearVerticalSnapOffsets();
}

} // namespace WebCore

#endif // CSS_SCROLL_SNAP
