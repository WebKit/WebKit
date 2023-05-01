/*
 * Copyright (C) 2007, 2013 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#pragma once

#include "FrameViewLayoutContext.h"
#include "LayoutRect.h"
#include <wtf/Noncopyable.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class RenderBlockFlow;
class RenderBox;
class RenderElement;
class RenderFragmentedFlow;
class RenderMultiColumnFlow;
class RenderObject;

class RenderLayoutState {
    WTF_MAKE_NONCOPYABLE(RenderLayoutState); WTF_MAKE_FAST_ALLOCATED;

public:
    struct LeadingTrim {
        bool trimFirstFormattedLine { false };
        WeakPtr<const RenderBlockFlow> trimLastFormattedLineOnTarget;
    };

    RenderLayoutState()
        : m_clipped(false)
        , m_isPaginated(false)
        , m_pageLogicalHeightChanged(false)
#if ASSERT_ENABLED
        , m_layoutDeltaXSaturated(false)
        , m_layoutDeltaYSaturated(false)
#endif
    {
    }
    RenderLayoutState(const FrameViewLayoutContext::LayoutStateStack&, RenderBox&, const LayoutSize& offset, LayoutUnit pageHeight, bool pageHeightChanged, std::optional<size_t> maximumLineCountForLineClamp, std::optional<size_t> visibleLineCountForLineClamp, std::optional<LeadingTrim>);
    enum class IsPaginated { No, Yes };
    explicit RenderLayoutState(RenderElement&, IsPaginated = IsPaginated::No);

    bool isPaginated() const { return m_isPaginated; }

    // The page logical offset is the object's offset from the top of the page in the page progression
    // direction (so an x-offset in vertical text and a y-offset for horizontal text).
    LayoutUnit pageLogicalOffset(RenderBox*, LayoutUnit childLogicalOffset) const;
    
    LayoutUnit pageLogicalHeight() const { return m_pageLogicalHeight; }
    bool pageLogicalHeightChanged() const { return m_pageLogicalHeightChanged; }

    RenderBlockFlow* lineGrid() const { return m_lineGrid.get(); }
    LayoutSize lineGridOffset() const { return m_lineGridOffset; }
    LayoutSize lineGridPaginationOrigin() const { return m_lineGridPaginationOrigin; }

    LayoutSize paintOffset() const { return m_paintOffset; }
    LayoutSize layoutOffset() const { return m_layoutOffset; }

    LayoutSize pageOffset() const { return m_pageOffset; }

    bool needsBlockDirectionLocationSetBeforeLayout() const { return m_lineGrid || (m_isPaginated && m_pageLogicalHeight); }

#if ASSERT_ENABLED
    RenderElement* renderer() const { return m_renderer; }
#endif
    LayoutRect clipRect() const { return m_clipRect; }
    bool isClipped() const { return m_clipped; }

    void addLayoutDelta(LayoutSize);
    LayoutSize layoutDelta() const { return m_layoutDelta; }
#if ASSERT_ENABLED
    bool layoutDeltaMatches(LayoutSize) const;
#endif

    bool hasLineClamp() const { return m_maximumLineCountForLineClamp.has_value(); }
    void resetLineClamp();
    void setMaximumLineCountForLineClamp(size_t maximumLineCount) { m_maximumLineCountForLineClamp = maximumLineCount; }
    std::optional<size_t> maximumLineCountForLineClamp() { return m_maximumLineCountForLineClamp; }
    void setVisibleLineCountForLineClamp(size_t visibleLineCount) { m_visibleLineCountForLineClamp = visibleLineCount; }
    std::optional<size_t> visibleLineCountForLineClamp() const { return m_visibleLineCountForLineClamp; }

    std::optional<LeadingTrim> leadingTrim() { return m_leadingTrim; }
    bool hasLeadingTrimStart() const { return m_leadingTrim && m_leadingTrim->trimFirstFormattedLine; }
    bool hasLeadingTrimEnd(const RenderBlockFlow& candidate) const { return m_leadingTrim && m_leadingTrim->trimLastFormattedLineOnTarget.get() == &candidate; }

    void addLeadingTrimStart();
    void removeLeadingTrimStart();

    void addLeadingTrimEnd(const RenderBlockFlow& targetInlineFormattingContext);
    void resetLeadingTrim() { m_leadingTrim = { }; }

private:
    void computeOffsets(const RenderLayoutState& ancestor, RenderBox&, LayoutSize offset);
    void computeClipRect(const RenderLayoutState& ancestor, RenderBox&);
    // FIXME: webkit.org/b/179440 these functions should be part of the pagination code/FrameViewLayoutContext.
    void computePaginationInformation(const FrameViewLayoutContext::LayoutStateStack&, RenderBox&, LayoutUnit pageLogicalHeight, bool pageLogicalHeightChanged);
    void propagateLineGridInfo(const RenderLayoutState& ancestor, RenderBox&);
    void establishLineGrid(const FrameViewLayoutContext::LayoutStateStack&, RenderBlockFlow&);
    void computeLineGridPaginationOrigin(const RenderMultiColumnFlow&);

    // Do not add anything apart from bitfields. See https://bugs.webkit.org/show_bug.cgi?id=100173
    bool m_clipped : 1;
    bool m_isPaginated : 1;
    // If our page height has changed, this will force all blocks to relayout.
    bool m_pageLogicalHeightChanged : 1;
#if ASSERT_ENABLED
    bool m_layoutDeltaXSaturated : 1;
    bool m_layoutDeltaYSaturated : 1;
#endif
    // The current line grid that we're snapping to and the offset of the start of the grid.
    WeakPtr<RenderBlockFlow> m_lineGrid;

    // FIXME: Distinguish between the layout clip rect and the paint clip rect which may be larger,
    // e.g., because of composited scrolling.
    LayoutRect m_clipRect;
    
    // x/y offset from layout root. Includes in-flow positioning and scroll offsets.
    LayoutSize m_paintOffset;
    // x/y offset from layout root. Does not include in-flow positioning or scroll offsets.
    LayoutSize m_layoutOffset;
    // Transient offset from the final position of the object
    // used to ensure that repaints happen in the correct place.
    // This is a total delta accumulated from the root. 
    LayoutSize m_layoutDelta;

    // The current page height for the pagination model that encloses us.
    LayoutUnit m_pageLogicalHeight;
    // The offset of the start of the first page in the nearest enclosing pagination model.
    LayoutSize m_pageOffset;
    LayoutSize m_lineGridOffset;
    LayoutSize m_lineGridPaginationOrigin;
    std::optional<size_t> m_maximumLineCountForLineClamp;
    std::optional<size_t> m_visibleLineCountForLineClamp;
    std::optional<LeadingTrim> m_leadingTrim;
#if ASSERT_ENABLED
    RenderElement* m_renderer { nullptr };
#endif
};

// Stack-based class to assist with LayoutState push/pop
class LayoutStateMaintainer {
    WTF_MAKE_NONCOPYABLE(LayoutStateMaintainer);
public:
    explicit LayoutStateMaintainer(RenderBox&, LayoutSize offset, bool disableState = false, LayoutUnit pageHeight = 0_lu, bool pageHeightChanged = false);
    ~LayoutStateMaintainer();

private:
    FrameViewLayoutContext& m_context;
    bool m_paintOffsetCacheIsDisabled { false };
    bool m_didPushLayoutState { false };
};

class SubtreeLayoutStateMaintainer {
public:
    SubtreeLayoutStateMaintainer(RenderElement* subtreeLayoutRoot);
    ~SubtreeLayoutStateMaintainer();

private:
    FrameViewLayoutContext* m_context { nullptr };
    bool m_didDisablePaintOffsetCache { false };
};

class LayoutStateDisabler {
    WTF_MAKE_NONCOPYABLE(LayoutStateDisabler);
public:
    LayoutStateDisabler(FrameViewLayoutContext&);
    ~LayoutStateDisabler();

private:
    FrameViewLayoutContext& m_context;
};

class PaginatedLayoutStateMaintainer {
public:
    PaginatedLayoutStateMaintainer(RenderBlockFlow&);
    ~PaginatedLayoutStateMaintainer();

private:
    FrameViewLayoutContext& m_context;
    bool m_pushed { false };
};

inline void RenderLayoutState::addLeadingTrimStart()
{
    if (m_leadingTrim) {
        m_leadingTrim->trimFirstFormattedLine = true;
        return;
    }
    m_leadingTrim = { true, { } };
}

inline void RenderLayoutState::removeLeadingTrimStart()
{
    ASSERT(m_leadingTrim && m_leadingTrim->trimFirstFormattedLine);
    m_leadingTrim->trimFirstFormattedLine = false;
}

inline void RenderLayoutState::addLeadingTrimEnd(const RenderBlockFlow& targetInlineFormattingContext)
{
    if (m_leadingTrim) {
        m_leadingTrim->trimLastFormattedLineOnTarget = &targetInlineFormattingContext;
        return;
    }
    m_leadingTrim = { false, &targetInlineFormattingContext };
}

inline void RenderLayoutState::resetLineClamp()
{
    m_maximumLineCountForLineClamp = { };
    m_visibleLineCountForLineClamp = { };
}

} // namespace WebCore
