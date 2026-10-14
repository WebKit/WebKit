/*
 * Copyright (C) 2019-2026 Apple Inc. All rights reserved.
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

#include <WebCore/LayoutElementBox.h>
#include <WebCore/LayoutInlineTextBox.h>
#include <WebCore/LayoutIntegrationInlineContent.h>
#include <WebCore/RenderBlockFlow.h>
#include <WebCore/RenderSVGInlineText.h>
#include <WebCore/TextBoxSelectableRange.h>
#include <ranges>

namespace WebCore {

class RenderLineBreak;
class RenderObject;
class RenderSVGText;

namespace Style {
class ComputedStyle;
}

namespace InlineIterator {

class BoxIteratorBase;
class InlineBox;
class InlineBoxIterator;
class LeafBoxIterator;
class LineBox;

// Which boxes a box iterator visits.
enum class BoxFilter : uint8_t {
    All,
    Leaf,
    SameLayoutBox,
};

template<typename, typename, BoxFilter> class BasicBoxIterator;
template<typename> class BoxRange;

class Box {
public:
    Box(const LayoutIntegration::InlineContent&, size_t boxIndex);

    bool isText() const;
    bool isSVGText() const;
    bool isInlineBox() const;
    bool isRubyBase() const;
    bool isRootInlineBox() const;
    bool isLineBreak() const;
    bool isBlockLevelBox() const;
    bool isAtomicInlineBox() const;

    FloatRect visualRect() const;
    FloatRect visualRectIgnoringBlockDirection() const;
    // Visual in inline direction, logical for writing mode.
    inline FloatRect logicalRectIgnoringInlineDirection() const;

    inline float logicalTop() const;
    inline float logicalBottom() const;
    inline float logicalHeight() const;
    inline float logicalWidth() const;
    inline float logicalLeft() const;
    inline float logicalRight() const;

    // Return line-relative left/right coords (they are still considered logical values as there's no flip for writing mode).
    inline float logicalLeftIgnoringInlineDirection() const;
    inline float logicalRightIgnoringInlineDirection() const;

    inline bool isHorizontal() const;

    unsigned minimumCaretOffset() const;
    unsigned maximumCaretOffset() const;
    unsigned leftmostCaretOffset() const { return isLeftToRightDirection() ? minimumCaretOffset() : maximumCaretOffset(); }
    unsigned rightmostCaretOffset() const { return isLeftToRightDirection() ? maximumCaretOffset() : minimumCaretOffset(); }

    // isLeftToRightDirection() here is not the same as writingMode().isBidiLTR().
    unsigned char bidiLevel() const;
    TextDirection direction() const { return bidiLevel() % 2 ? TextDirection::RTL : TextDirection::LTR; }
    bool isLeftToRightDirection() const { return direction() == TextDirection::LTR; }
    bool isInlineFlipped() const { return !(isLeftToRightDirection() == writingMode().isLogicalLeftLineLeft()); }

    RenderObject::HighlightState selectionState() const;

    const RenderObject& renderer() const;
    const RenderBlockFlow& formattingContextRoot() const;
    CheckedRef<const Style::ComputedStyle> style() const;
    WritingMode writingMode() const { return style()->writingMode(); }

    // Text-relative left/right leaf boxes.
    LeafBoxIterator nextLineRightwardOnLine() const;
    LeafBoxIterator nextLineLeftwardOnLine() const;
    LeafBoxIterator nextLineRightwardOnLineIgnoringLineBreak() const;
    LeafBoxIterator nextLineLeftwardOnLineIgnoringLineBreak() const;

    // Coordinate-relative left/right leaf boxes.
    inline LeafBoxIterator nextLogicalRightwardOnLine() const;
    inline LeafBoxIterator nextLogicalLeftwardOnLine() const;
    inline LeafBoxIterator nextLogicalRightwardOnLineIgnoringLineBreak() const;
    inline LeafBoxIterator nextLogicalLeftwardOnLineIgnoringLineBreak() const;

    InlineBoxIterator parentInlineBox() const;

    LineBox lineBox() const;
    size_t lineIndex() const;

    const InlineDisplay::Box& displayBox() const { return displayBoxes()[m_boxIndex]; }
    size_t boxIndex() const { return m_boxIndex; }
    const LayoutIntegration::InlineContent& inlineContent() const { return *m_inlineContent; }

protected:
    friend class BoxIteratorBase;
    friend class InlineBox;
    template<typename, typename, BoxFilter> friend class BasicBoxIterator;
    template<typename> friend class BoxRange;

    Box() = default;

    bool atEnd() const { return !m_inlineContent || m_boxIndex == displayBoxes().size(); }

    const InlineDisplay::Line& line() const { return m_inlineContent->lineForBox(displayBox()); }

private:
    bool hasRenderer() const;
    bool isWithinInlineBox(const Layout::Box&) const;

    void traverseNextBoxOnLineSkippingChildren();

    void traverseNextBox();
    void traversePreviousBox();

    void setAtEnd() { m_boxIndex = displayBoxes().size(); }

    const InlineDisplay::Boxes& displayBoxes() const { return m_inlineContent->displayContent().boxes; }

    SingleThreadWeakPtr<const LayoutIntegration::InlineContent> m_inlineContent;
    size_t m_boxIndex { 0 };
};

// Box iterators are bidirectional and work with std::ranges. They are also the nullable box type: an iterator can be
// positioned at any box, tests false at the end, and decrementing past the first box moves it to the end too. An
// iterator converts to any other kind of box iterator positioned at the same box.
// BoxIterator and LeafBoxIterator visit the boxes of one line. TextBoxIterator, SVGTextBoxIterator and
// InlineBoxIterator visit the boxes of one layout box.
// An iterator reads as at the end once the inline content is destroyed. Relayout rebuilds the content in place, so
// iterators must not be held across layout.
class BoxIteratorBase {
public:
    explicit operator bool() const { return !atEnd(); }

    bool operator==(const BoxIteratorBase& other) const
    {
        if (atEnd() || other.atEnd())
            return atEnd() == other.atEnd();
        return m_box.m_inlineContent == other.m_box.m_inlineContent && m_box.m_boxIndex == other.m_box.m_boxIndex;
    }

    bool operator==(const Box& box) const { return !atEnd() && m_box.m_inlineContent == box.m_inlineContent && m_box.m_boxIndex == box.m_boxIndex; }

    bool atEnd() const { return !m_box.m_inlineContent || m_box.m_boxIndex == m_endIndex; }

protected:
    template<typename, typename, BoxFilter> friend class BasicBoxIterator;
    template<typename> friend class BoxRange;

    BoxIteratorBase() = default;

    Box m_box;
    size_t m_beginIndex { 0 };
    size_t m_endIndex { 0 };
    size_t m_referenceIndex { 0 };
};

// Dereferencing returns the box handle by value since the iterator holds it (a reference would dangle in
// std::reverse_iterator). The pointer from operator-> has the same problem, so don't use it on a std::reverse_iterator.
template<typename DerivedIterator, typename BoxType, BoxFilter boxFilter>
class BasicBoxIterator : public BoxIteratorBase {
public:
    static constexpr auto filter = boxFilter;

    using value_type = BoxType;
    using difference_type = std::ptrdiff_t;
    using iterator_concept = std::bidirectional_iterator_tag;
    using iterator_category = std::input_iterator_tag;

    BasicBoxIterator() = default;
    BasicBoxIterator(const Box& box)
    {
        if (box.atEnd())
            return;
        m_box = box;
        m_referenceIndex = box.m_boxIndex;
        if constexpr (filter == BoxFilter::SameLayoutBox)
            m_endIndex = box.displayBoxes().size();
        else {
            m_beginIndex = box.line().firstBoxIndex();
            m_endIndex = m_beginIndex + box.line().boxCount();
        }
    }
    // Positioned at the same box as another kind of box iterator.
    BasicBoxIterator(const BoxIteratorBase& other)
        : BasicBoxIterator(other ? other.m_box : Box { })
    {
    }

    const BoxType operator*() const { ASSERT(!atEnd()); return get(); }
    const BoxType* operator->() const LIFETIME_BOUND { ASSERT(!atEnd()); return &get(); }

    DerivedIterator& operator++()
    {
        ASSERT(!atEnd());
        auto& index = m_box.m_boxIndex;
        if constexpr (filter == BoxFilter::SameLayoutBox) {
            if (m_box.displayBox().isLastForLayoutBox()) {
                index = m_endIndex;
                return derived();
            }
        }
        do {
            ++index;
        } while (index < m_endIndex && !matches(index));
        return derived();
    }

    DerivedIterator& operator--()
    {
        if (!m_box.m_inlineContent)
            return derived();
        auto& index = m_box.m_boxIndex;
        if constexpr (filter == BoxFilter::SameLayoutBox) {
            if (!atEnd() && m_box.displayBox().isFirstForLayoutBox()) {
                index = m_endIndex;
                return derived();
            }
        }
        for (auto candidate = index; candidate > m_beginIndex;) {
            if (matches(--candidate)) {
                index = candidate;
                return derived();
            }
        }
        index = m_endIndex;
        return derived();
    }

    DerivedIterator operator++(int)
    {
        auto copy = derived();
        ++*this;
        return copy;
    }

    DerivedIterator operator--(int)
    {
        auto copy = derived();
        --*this;
        return copy;
    }

    // Named versions of ++ and -- for cursor-style code.
    DerivedIterator& traverseLineRightwardOnLine() requires (filter != BoxFilter::SameLayoutBox) { return ++*this; }
    DerivedIterator& traverseLineLeftwardOnLine() requires (filter != BoxFilter::SameLayoutBox) { return --*this; }

    DerivedIterator& traverseLineRightwardOnLineIgnoringLineBreak() requires (filter == BoxFilter::Leaf)
    {
        do {
            ++*this;
        } while (!atEnd() && m_box.isLineBreak());
        return derived();
    }

    DerivedIterator& traverseLineLeftwardOnLineIgnoringLineBreak() requires (filter == BoxFilter::Leaf)
    {
        do {
            --*this;
        } while (!atEnd() && m_box.isLineBreak());
        return derived();
    }

    // Coordinate-relative left/right.
    DerivedIterator& traverseLogicalRightwardOnLine() requires (filter == BoxFilter::Leaf)
    {
        return m_box.writingMode().isLogicalLeftLineLeft() ? traverseLineRightwardOnLine() : traverseLineLeftwardOnLine();
    }
    DerivedIterator& traverseLogicalLeftwardOnLine() requires (filter == BoxFilter::Leaf)
    {
        return m_box.writingMode().isLogicalLeftLineLeft() ? traverseLineLeftwardOnLine() : traverseLineRightwardOnLine();
    }
    DerivedIterator& traverseLogicalRightwardOnLineIgnoringLineBreak() requires (filter == BoxFilter::Leaf)
    {
        return m_box.writingMode().isLogicalLeftLineLeft() ? traverseLineRightwardOnLineIgnoringLineBreak() : traverseLineLeftwardOnLineIgnoringLineBreak();
    }
    DerivedIterator& traverseLogicalLeftwardOnLineIgnoringLineBreak() requires (filter == BoxFilter::Leaf)
    {
        return m_box.writingMode().isLogicalLeftLineLeft() ? traverseLineLeftwardOnLineIgnoringLineBreak() : traverseLineRightwardOnLineIgnoringLineBreak();
    }

    // Moves past the descendants of the current inline box.
    DerivedIterator& traverseLineRightwardOnLineSkippingChildren() requires (filter == BoxFilter::All)
    {
        ASSERT(!atEnd());
        auto box = m_box;
        box.traverseNextBoxOnLineSkippingChildren();
        m_box.m_boxIndex = box.atEnd() ? m_endIndex : std::min(box.m_boxIndex, m_endIndex);
        return derived();
    }

private:
    const BoxType& get() const LIFETIME_BOUND
    {
        if constexpr (std::is_same_v<BoxType, Box>)
            return m_box;
        else
            return downcast<BoxType>(m_box);
    }

    DerivedIterator& derived() { SUPPRESS_MEMORY_UNSAFE_CAST return static_cast<DerivedIterator&>(*this); }
    const DerivedIterator& derived() const { SUPPRESS_MEMORY_UNSAFE_CAST return static_cast<const DerivedIterator&>(*this); }

    bool matches(size_t index) const
    {
        auto& displayBox = m_box.displayBoxes()[index];
        if constexpr (filter == BoxFilter::Leaf)
            return !displayBox.isInlineBox();
        else if constexpr (filter == BoxFilter::SameLayoutBox)
            return &displayBox.layoutBox() == &m_box.displayBoxes()[m_referenceIndex].layoutBox();
        else
            return true;
    }
};

// Every box on a line, including inline boxes.
class BoxIterator : public BasicBoxIterator<BoxIterator, Box, BoxFilter::All> {
public:
    using BasicBoxIterator::BasicBoxIterator;
};

// Leaf boxes on a line.
class LeafBoxIterator : public BasicBoxIterator<LeafBoxIterator, Box, BoxFilter::Leaf> {
public:
    using BasicBoxIterator::BasicBoxIterator;
};

// Boxes matching the iterator filter, in display order.
template<typename IteratorType>
class BoxRange : public std::ranges::view_interface<BoxRange<IteratorType>> {
public:
    using Iterator = IteratorType;

    BoxRange() = default;

    // The range ends where the iterator kind ends unless an end box index is given.
    BoxRange(const BoxIteratorBase& first, std::optional<size_t> endIndex = { })
        : m_begin(first)
    {
        if (!m_begin)
            return;
        if (endIndex)
            m_begin.m_endIndex = *endIndex;
        m_end = m_begin;
        m_end.m_box.m_boxIndex = m_end.m_endIndex;
    }

    Iterator begin() const { return m_begin; }
    Iterator end() const { return m_end; }

private:
    Iterator m_begin;
    Iterator m_end;
};

using LeafBoxRange = BoxRange<LeafBoxIterator>;

LeafBoxIterator boxFor(const RenderLineBreak&);
LeafBoxIterator boxFor(const RenderBox&);
Box boxFor(const LayoutIntegration::InlineContent&, size_t boxIndex);

// -----------------------------------------------

inline Box::Box(const LayoutIntegration::InlineContent& inlineContent, size_t boxIndex)
    : m_inlineContent(&inlineContent)
    , m_boxIndex(boxIndex)
{
}

inline bool Box::isText() const
{
    return displayBox().isTextOrSoftLineBreak();
}

inline bool Box::isInlineBox() const
{
    return displayBox().isInlineBox();
}

inline bool Box::isRootInlineBox() const
{
    return displayBox().isRootInlineBox();
}

inline bool Box::isRubyBase() const
{
    return displayBox().isRubyBase();
}

inline FloatRect Box::visualRectIgnoringBlockDirection() const
{
    return displayBox().visualRectIgnoringBlockDirection();
}

inline bool Box::isLineBreak() const
{
    return displayBox().isLineBreak();
}

inline bool Box::isBlockLevelBox() const
{
    return displayBox().isBlockLevelBox();
}

inline bool Box::isAtomicInlineBox() const
{
    return displayBox().isAtomicInlineBox();
}

inline unsigned Box::minimumCaretOffset() const
{
    return isText() ? displayBox().text().start() : 0;
}

inline unsigned Box::maximumCaretOffset() const
{
    return isText() ? displayBox().text().end() : 1;
}

inline unsigned char Box::bidiLevel() const
{
    return displayBox().bidiLevel();
}

inline size_t Box::lineIndex() const
{
    return displayBox().lineIndex();
}

inline const RenderObject& Box::renderer() const
{
    return *displayBox().layoutBox().rendererForIntegration();
}

inline bool Box::hasRenderer() const
{
    return !!displayBox().layoutBox().rendererForIntegration();
}

inline const RenderBlockFlow& Box::formattingContextRoot() const
{
    return m_inlineContent->formattingContextRoot();
}

inline CheckedRef<const Style::ComputedStyle> Box::style() const
{
    return displayBox().style();
}

inline void Box::traverseNextBoxOnLineSkippingChildren()
{
    auto lineIndex = displayBox().lineIndex();
    bool wasInlineBox = displayBox().isInlineBox();
    auto& startBox = displayBox().layoutBox();

    traverseNextBox();

    if (wasInlineBox) {
        while (!atEnd() && isWithinInlineBox(startBox))
            traverseNextBox();
    }

    if (!atEnd() && lineIndex != displayBox().lineIndex())
        setAtEnd();
}

inline bool Box::isWithinInlineBox(const Layout::Box& inlineBox) const
{
    for (auto* layoutBox = &displayBox().layoutBox().parent();; layoutBox = &layoutBox->parent()) {
        if (layoutBox == &inlineBox)
            return true;
        if (!layoutBox->isInlineBox())
            return false;
    }
    return false;
}

inline void Box::traverseNextBox()
{
    ASSERT(!atEnd());
    ++m_boxIndex;
}

inline void Box::traversePreviousBox()
{
    ASSERT(!atEnd());
    m_boxIndex = m_boxIndex ? m_boxIndex - 1 : displayBoxes().size();
}

}
}

// Range iterators hold their own box, so they stay valid after the range object goes away.
template<typename IteratorType>
inline constexpr bool std::ranges::enable_borrowed_range<WebCore::InlineIterator::BoxRange<IteratorType>> = true;
