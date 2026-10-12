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

namespace WebCore {

class RenderLineBreak;
class RenderObject;
class RenderSVGText;

namespace Style {
class ComputedStyle;
}

namespace InlineIterator {

class LineBoxIterator;
class BoxIterator;
class LeafBoxIterator;
class TextBoxIterator;

struct EndIterator { };

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

    // Text-relative left/right
    LeafBoxIterator nextLineRightwardOnLine() const;
    LeafBoxIterator nextLineLeftwardOnLine() const;
    LeafBoxIterator nextLineRightwardOnLineIgnoringLineBreak() const;
    LeafBoxIterator nextLineLeftwardOnLineIgnoringLineBreak() const;

    // Coordinate-relative left/right
    inline LeafBoxIterator nextLogicalRightwardOnLine() const;
    inline LeafBoxIterator nextLogicalLeftwardOnLine() const;
    inline LeafBoxIterator nextLogicalRightwardOnLineIgnoringLineBreak() const;
    inline LeafBoxIterator nextLogicalLeftwardOnLineIgnoringLineBreak() const;

    InlineBoxIterator parentInlineBox() const;

    LineBoxIterator lineBox() const;
    size_t lineIndex() const;

    const InlineDisplay::Box& displayBox() const { return displayBoxes()[m_boxIndex]; }
    size_t boxIndex() const { return m_boxIndex; }
    const LayoutIntegration::InlineContent& inlineContent() const { return *m_inlineContent; }

protected:
    friend class BoxIterator;
    friend class InlineBoxIterator;
    friend class LeafBoxIterator;
    friend class TextBoxIterator;

    Box() = default;

    bool atEnd() const { return !m_inlineContent || m_boxIndex == displayBoxes().size(); }

    void traverseNextTextBox();
    void traverseNextLeafOnLine();
    void traversePreviousLeafOnLine();
    void traverseNextInlineBox();
    void traversePreviousInlineBox();
    void traverseNextBoxOnLine();
    void traverseNextBoxOnLineSkippingChildren();
    void traversePreviousBoxOnLine();

    Box firstLeafBoxForInlineBox() const;
    Box lastLeafBoxForInlineBox() const;

    const InlineDisplay::Line& line() const { return m_inlineContent->lineForBox(displayBox()); }

private:
    bool hasRenderer() const;
    bool isWithinInlineBox(const Layout::Box&) const;

    void traverseNextBox();
    void traversePreviousBox();
    void traverseNextLeaf();
    void traversePreviousLeaf();
    void traverseNextWithSameLayoutBox();
    void traversePreviousWithSameLayoutBox();

    void setAtEnd() { m_boxIndex = displayBoxes().size(); }

    const InlineDisplay::Boxes& displayBoxes() const { return m_inlineContent->displayContent().boxes; }

    WeakPtr<const LayoutIntegration::InlineContent> m_inlineContent;
    size_t m_boxIndex { 0 };
};

class BoxIterator {
public:
    BoxIterator() = default;
    BoxIterator(const Box&);

    explicit operator bool() const { return !atEnd(); }

    bool operator==(const BoxIterator&) const;
    bool operator==(EndIterator) const { return atEnd(); }

    const Box& operator*() const LIFETIME_BOUND { return m_box; }
    const Box* operator->() const LIFETIME_BOUND { return &m_box; }

    BoxIterator& traverseLineRightwardOnLine();
    BoxIterator& traverseLineRightwardOnLineSkippingChildren();
    BoxIterator& traverseLineLeftwardOnLine();

    BoxIterator& operator++() { return traverseLineRightwardOnLine(); }
    BoxIterator& operator--() { return traverseLineLeftwardOnLine(); }

    bool atEnd() const;

protected:
    Box m_box;
};

class LeafBoxIterator : public BoxIterator {
public:
    LeafBoxIterator() = default;
    LeafBoxIterator(const Box&);

    // Text-relative left/right
    LeafBoxIterator& traverseLineRightwardOnLine();
    LeafBoxIterator& traverseLineLeftwardOnLine();
    LeafBoxIterator& traverseLineRightwardOnLineIgnoringLineBreak();
    LeafBoxIterator& traverseLineLeftwardOnLineIgnoringLineBreak();

    // Coordinate-relative left/right
    inline LeafBoxIterator& traverseLogicalRightwardOnLine();
    inline LeafBoxIterator& traverseLogicalLeftwardOnLine();
    inline LeafBoxIterator& traverseLogicalRightwardOnLineIgnoringLineBreak();
    inline LeafBoxIterator& traverseLogicalLeftwardOnLineIgnoringLineBreak();

    LeafBoxIterator& operator++() { return traverseLineRightwardOnLine(); }
};

template<class IteratorType>
class BoxRange {
public:
    BoxRange(IteratorType begin)
        : m_begin(begin)
    {
    }

    IteratorType begin() const { return m_begin; }
    EndIterator end() const { return { }; }

private:
    IteratorType m_begin;
};

LeafBoxIterator boxFor(const RenderLineBreak&);
LeafBoxIterator boxFor(const RenderBox&);
LeafBoxIterator boxFor(const LayoutIntegration::InlineContent&, size_t boxIndex);

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

inline void Box::traverseNextTextBox()
{
    ASSERT(!atEnd());
    ASSERT(displayBox().isTextOrSoftLineBreak());

    if (displayBox().isLastForLayoutBox()) {
        setAtEnd();
        return;
    }

    traverseNextWithSameLayoutBox();

    ASSERT(displayBox().isTextOrSoftLineBreak());
}

inline void Box::traverseNextLeafOnLine()
{
    ASSERT(!atEnd());

    auto oldLineIndex = displayBox().lineIndex();

    traverseNextLeaf();

    if (!atEnd() && oldLineIndex != displayBox().lineIndex())
        setAtEnd();
}

inline void Box::traversePreviousLeafOnLine()
{
    ASSERT(!atEnd());

    auto oldLineIndex = displayBox().lineIndex();

    traversePreviousLeaf();

    if (!atEnd() && oldLineIndex != displayBox().lineIndex())
        setAtEnd();
}

inline void Box::traverseNextInlineBox()
{
    ASSERT(!atEnd());
    ASSERT(displayBox().isInlineBox());

    if (displayBox().isLastForLayoutBox()) {
        setAtEnd();
        return;
    }

    traverseNextWithSameLayoutBox();

    ASSERT(displayBox().isInlineBox());
}

inline void Box::traversePreviousInlineBox()
{
    ASSERT(!atEnd());
    ASSERT(displayBox().isInlineBox());

    if (displayBox().isFirstForLayoutBox()) {
        setAtEnd();
        return;
    }

    traversePreviousWithSameLayoutBox();

    ASSERT(displayBox().isInlineBox());
}

inline void Box::traverseNextBoxOnLine()
{
    auto lineIndex = displayBox().lineIndex();

    traverseNextBox();

    if (!atEnd() && lineIndex != displayBox().lineIndex())
        setAtEnd();
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

inline void Box::traversePreviousBoxOnLine()
{
    auto lineIndex = displayBox().lineIndex();

    traversePreviousBox();

    if (!atEnd() && lineIndex != displayBox().lineIndex())
        setAtEnd();
}

inline Box Box::firstLeafBoxForInlineBox() const
{
    ASSERT(displayBox().isInlineBox());

    auto& inlineBox = displayBox().layoutBox();

    // The next box is the first descendant of this box.
    auto first = *this;
    first.traverseNextLeafOnLine();

    if (!first.atEnd() && !first.isWithinInlineBox(inlineBox))
        first.setAtEnd();

    return first;
}

inline Box Box::lastLeafBoxForInlineBox() const
{
    ASSERT(displayBox().isInlineBox());

    auto& inlineBox = displayBox().layoutBox();

    // FIXME: Get the last box index directly from the display box.
    auto last = firstLeafBoxForInlineBox();
    for (auto box = last; !box.atEnd() && box.isWithinInlineBox(inlineBox); box.traverseNextLeafOnLine())
        last = box;

    return last;
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

inline void Box::traverseNextLeaf()
{
    do {
        traverseNextBox();
    } while (!atEnd() && displayBox().isInlineBox());
}

inline void Box::traversePreviousLeaf()
{
    do {
        traversePreviousBox();
    } while (!atEnd() && displayBox().isInlineBox());
}

inline void Box::traverseNextWithSameLayoutBox()
{
    auto& layoutBox = displayBox().layoutBox();
    do {
        traverseNextBox();
    } while (!atEnd() && &displayBox().layoutBox() != &layoutBox);
}

inline void Box::traversePreviousWithSameLayoutBox()
{
    auto& layoutBox = displayBox().layoutBox();
    do {
        traversePreviousBox();
    } while (!atEnd() && &displayBox().layoutBox() != &layoutBox);
}

}
}
