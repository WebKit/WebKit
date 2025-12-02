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

#pragma once


#include <WebCore/InlineDisplayContent.h>
#include <wtf/HashMap.h>
#include <wtf/IteratorRange.h>
#include <wtf/Vector.h>
#include <wtf/WeakPtr.h>

namespace WebCore {
namespace LayoutIntegration {
class InlineContent;
}
}

namespace WTF {
template<typename T> struct IsDeprecatedWeakRefSmartPointerException;
template<> struct IsDeprecatedWeakRefSmartPointerException<WebCore::LayoutIntegration::InlineContent> : std::true_type { };
}

namespace WebCore {

class RenderBlockFlow;
class RenderObject;
struct SVGTextFragment;

namespace Layout {
class Box;
}

namespace InlineDisplay {
struct Box;
class Line;
} 

namespace LayoutIntegration {

class LineLayout;

class InlineContent : public CanMakeWeakPtr<InlineContent> {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(InlineContent);
public:
    InlineContent(const RenderBlockFlow& formattingContextRoot);

    InlineDisplay::Content& displayContent() { return m_displayContent; }
    const InlineDisplay::Content& displayContent() const { return m_displayContent; }
    bool hasInflowContent() const;

    FloatRect scrollableOverflow() const { return m_scrollableOverflow; }
    FloatRect inkOverflow() const { return m_inkOverflow; }
    bool hasInkOverflow() const { return inkOverflow() != scrollableOverflow(); }

    bool isPaginated() const { return m_firstLinePaginationOffset.has_value(); }
    float firstLinePaginationOffset() const { return m_firstLinePaginationOffset.value_or(0.f); }
    float clearBeforeAfterGaps() const { return m_clearGapBeforeFirstLine + m_clearGapAfterLastLine; }
    float clearGapBeforeFirstLine() const { return m_clearGapBeforeFirstLine; }
    bool hasBlockLevelBoxes() const { return m_hasBlockLevelBoxes; }

    IteratorRange<const InlineDisplay::Box*> boxesForRect(const LayoutRect&) const;

    const InlineDisplay::Line& lineForBox(const InlineDisplay::Box& box) const { return displayContent().lines[box.lineIndex()]; }
    size_t indexForBox(const InlineDisplay::Box&) const;
    const InlineDisplay::Box* firstBoxForLayoutBox(const Layout::Box&) const;
    std::optional<size_t> firstBoxIndexForLayoutBox(const Layout::Box&) const;

    // Returns a block level box if the line is for block-in-inline.
    const InlineDisplay::Box* blockLevelBoxForLine(const InlineDisplay::Line&) const;
    bool isInlineBoxWrapperForBlockLevelBox(const InlineDisplay::Box&) const;

    template<typename Function> void traverseNonRootInlineBoxes(const Layout::Box&, Function&&);

    const RenderBlockFlow& formattingContextRoot() const;

    const Vector<SVGTextFragment>& svgTextFragments(size_t boxIndex) const;
    Vector<Vector<SVGTextFragment>>& svgTextFragmentsForBoxes() { return m_svgTextFragmentsForBoxes; }

    void shrinkToFit();
    void releaseCaches();

private:
    friend class InlineContentBuilder;
    friend class LineLayout;

    void setInkOverflow(const FloatRect& inkOverflow) { m_inkOverflow = inkOverflow; }
    void setScrollableOverflow(const FloatRect& scrollableOverflow) { m_scrollableOverflow = scrollableOverflow; }
    void setHasMultilinePaintOverlap() { m_hasMultilinePaintOverlap = true; }
    void setClearGapBeforeFirstLine(float clearGapBeforeFirstLine) { m_clearGapBeforeFirstLine = clearGapBeforeFirstLine; }
    void setClearGapAfterLastLine(float clearGapAfterLastLine) { m_clearGapAfterLastLine = clearGapAfterLastLine; }
    void setFirstLinePaginationOffset(float firstLinePaginationOffset) { m_firstLinePaginationOffset = firstLinePaginationOffset; }
    void setHasBlockLevelBoxes() { m_hasBlockLevelBoxes = true; }

    const Vector<size_t>& nonRootInlineBoxIndexesForLayoutBox(const Layout::Box&) const;

    CheckedRef<const RenderBlockFlow> m_formattingContextRoot;

    InlineDisplay::Content m_displayContent;
    using FirstBoxIndexCache = HashMap<CheckedRef<const Layout::Box>, size_t>;
    mutable std::unique_ptr<FirstBoxIndexCache> m_firstBoxIndexCache;

    using InlineBoxIndexCache = HashMap<CheckedRef<const Layout::Box>, Vector<size_t>>;
    mutable std::unique_ptr<InlineBoxIndexCache> m_inlineBoxIndexCache;
    FloatRect m_scrollableOverflow;
    FloatRect m_inkOverflow;
    float m_clearGapBeforeFirstLine { 0 };
    float m_clearGapAfterLastLine { 0 };
    std::optional<float> m_firstLinePaginationOffset { };

    bool m_hasMultilinePaintOverlap { false };
    bool m_hasBlockLevelBoxes { false };

    Vector<Vector<SVGTextFragment>> m_svgTextFragmentsForBoxes;
};

template<typename Function> void InlineContent::traverseNonRootInlineBoxes(const Layout::Box& layoutBox, Function&& function)
{
    for (auto index : nonRootInlineBoxIndexesForLayoutBox(layoutBox))
        function(displayContent().boxes[index]);
}

}
}

