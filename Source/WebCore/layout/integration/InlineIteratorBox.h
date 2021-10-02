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

#include "InlineIteratorBoxLegacyPath.h"
#include "InlineIteratorBoxModernPath.h"
#include "LegacyInlineElementBox.h"
#include <wtf/Variant.h>

namespace WebCore {

class RenderLineBreak;
class RenderObject;
class RenderStyle;
class RenderText;

namespace InlineIterator {

class LineIterator;
class BoxIterator;
class TextBoxIterator;

struct EndIterator { };

class Box {
public:
    using PathVariant = Variant<
#if ENABLE(LAYOUT_FORMATTING_CONTEXT)
        BoxIteratorModernPath,
#endif
        BoxIteratorLegacyPath
    >;

    Box(PathVariant&&);

    bool isText() const;

    FloatRect rect() const;

    float logicalTop() const { return isHorizontal() ? rect().y() : rect().x(); }
    float logicalBottom() const { return isHorizontal() ? rect().maxY() : rect().maxX(); }
    float logicalLeft() const { return isHorizontal() ? rect().x() : rect().y(); }
    float logicalRight() const { return isHorizontal() ? rect().maxX() : rect().maxY(); }
    float logicalWidth() const { return isHorizontal() ? rect().width() : rect().height(); }
    float logicalHeight() const { return isHorizontal() ? rect().height() : rect().width(); }

    bool isHorizontal() const;
    bool dirOverride() const;
    bool isLineBreak() const;

    unsigned minimumCaretOffset() const;
    unsigned maximumCaretOffset() const;
    unsigned leftmostCaretOffset() const { return isLeftToRightDirection() ? minimumCaretOffset() : maximumCaretOffset(); }
    unsigned rightmostCaretOffset() const { return isLeftToRightDirection() ? maximumCaretOffset() : minimumCaretOffset(); }

    unsigned char bidiLevel() const;
    TextDirection direction() const { return bidiLevel() % 2 ? TextDirection::RTL : TextDirection::LTR; }
    bool isLeftToRightDirection() const { return direction() == TextDirection::LTR; }

    RenderObject::HighlightState selectionState() const;

    const RenderObject& renderer() const;
    const RenderStyle& style() const;

    // For intermediate porting steps only.
    const LegacyInlineBox* legacyInlineBox() const;
#if ENABLE(LAYOUT_FORMATTING_CONTEXT)
    const InlineDisplay::Box* inlineBox() const;
#endif
    BoxIterator nextOnLine() const;
    BoxIterator previousOnLine() const;
    BoxIterator nextOnLineIgnoringLineBreak() const;
    BoxIterator previousOnLineIgnoringLineBreak() const;

    LineIterator line() const;

protected:
    friend class BoxIterator;
    friend class TextBoxIterator;

    // To help with debugging.
#if ENABLE(LAYOUT_FORMATTING_CONTEXT)
    const BoxIteratorModernPath& modernPath() const;
#endif
    const BoxIteratorLegacyPath& legacyPath() const;

    PathVariant m_pathVariant;
};

class TextBox : public Box {
public:
    TextBox(PathVariant&&);

    bool hasHyphen() const;
    StringView text() const;

    unsigned start() const;
    unsigned end() const;
    unsigned length() const;

    unsigned offsetForPosition(float x) const;
    float positionForOffset(unsigned) const;

    TextBoxSelectableRange selectableRange() const;
    LayoutRect selectionRect(unsigned start, unsigned end) const;

    bool isCombinedText() const;
    const FontCascade& fontCascade() const;

    TextRun createTextRun() const;

    const RenderText& renderer() const { return downcast<RenderText>(Box::renderer()); }
    const LegacyInlineTextBox* legacyInlineBox() const { return downcast<LegacyInlineTextBox>(Box::legacyInlineBox()); }

    TextBoxIterator nextTextRun() const;
    TextBoxIterator nextTextRunInTextOrder() const;
};

class BoxIterator {
public:
    BoxIterator() : m_run(BoxIteratorLegacyPath { nullptr, { } }) { };
    BoxIterator(Box::PathVariant&&);
    BoxIterator(const Box&);

    explicit operator bool() const { return !atEnd(); }

    bool operator==(const BoxIterator&) const;
    bool operator!=(const BoxIterator& other) const { return !(*this == other); }

    bool operator==(EndIterator) const { return atEnd(); }
    bool operator!=(EndIterator) const { return !atEnd(); }

    const Box& operator*() const { return m_run; }
    const Box* operator->() const { return &m_run; }

    bool atEnd() const;

    BoxIterator& traverseNextOnLine();
    BoxIterator& traversePreviousOnLine();
    BoxIterator& traverseNextOnLineIgnoringLineBreak();
    BoxIterator& traversePreviousOnLineIgnoringLineBreak();
    BoxIterator& traverseNextOnLineInLogicalOrder();
    BoxIterator& traversePreviousOnLineInLogicalOrder();

protected:
    Box m_run;
};

class TextBoxIterator : public BoxIterator {
public:
    TextBoxIterator() { }
    TextBoxIterator(Box::PathVariant&&);
    TextBoxIterator(const Box&);

    TextBoxIterator& operator++() { return traverseNextTextRun(); }

    const TextBox& operator*() const { return get(); }
    const TextBox* operator->() const { return &get(); }

    TextBoxIterator& traverseNextTextRun();
    TextBoxIterator& traverseNextTextRunInTextOrder();

private:
    BoxIterator& traverseNextOnLine() = delete;
    BoxIterator& traversePreviousOnLine() = delete;
    BoxIterator& traverseNextOnLineIgnoringLineBreak() = delete;
    BoxIterator& traversePreviousOnLineIgnoringLineBreak() = delete;
    BoxIterator& traverseNextOnLineInLogicalOrder() = delete;
    BoxIterator& traversePreviousOnLineInLogicalOrder() = delete;

    const TextBox& get() const { return downcast<TextBox>(m_run); }
};

class TextRunRange {
public:
    TextRunRange(TextBoxIterator begin)
        : m_begin(begin)
    {
    }

    TextBoxIterator begin() const { return m_begin; }
    EndIterator end() const { return { }; }

private:
    TextBoxIterator m_begin;
};

TextBoxIterator firstTextRunFor(const RenderText&);
TextBoxIterator firstTextRunInTextOrderFor(const RenderText&);
TextBoxIterator textRunFor(const LegacyInlineTextBox*);
#if ENABLE(LAYOUT_FORMATTING_CONTEXT)
TextBoxIterator textRunFor(const LayoutIntegration::InlineContent&, const InlineDisplay::Box&);
TextBoxIterator textRunFor(const LayoutIntegration::InlineContent&, size_t runIndex);
#endif
TextRunRange textRunsFor(const RenderText&);
BoxIterator runFor(const RenderLineBreak&);
BoxIterator runFor(const RenderBox&);
#if ENABLE(LAYOUT_FORMATTING_CONTEXT)
BoxIterator runFor(const LayoutIntegration::InlineContent&, size_t runIndex);
#endif

// -----------------------------------------------

inline Box::Box(PathVariant&& path)
    : m_pathVariant(WTFMove(path))
{
}

inline bool Box::isText() const
{
    return WTF::switchOn(m_pathVariant, [](auto& path) {
        return path.isText();
    });
}

inline FloatRect Box::rect() const
{
    return WTF::switchOn(m_pathVariant, [](auto& path) {
        return path.rect();
    });
}

inline bool Box::isHorizontal() const
{
    return WTF::switchOn(m_pathVariant, [](auto& path) {
        return path.isHorizontal();
    });
}

inline bool Box::dirOverride() const
{
    return WTF::switchOn(m_pathVariant, [](auto& path) {
        return path.dirOverride();
    });
}

inline bool Box::isLineBreak() const
{
    return WTF::switchOn(m_pathVariant, [](auto& path) {
        return path.isLineBreak();
    });
}

inline unsigned Box::minimumCaretOffset() const
{
    return WTF::switchOn(m_pathVariant, [](auto& path) {
        return path.minimumCaretOffset();
    });
}

inline unsigned Box::maximumCaretOffset() const
{
    return WTF::switchOn(m_pathVariant, [](auto& path) {
        return path.maximumCaretOffset();
    });
}

inline unsigned char Box::bidiLevel() const
{
    return WTF::switchOn(m_pathVariant, [](auto& path) {
        return path.bidiLevel();
    });
}

inline const RenderObject& Box::renderer() const
{
    return WTF::switchOn(m_pathVariant, [](auto& path) -> const RenderObject& {
        return path.renderer();
    });
}

inline const LegacyInlineBox* Box::legacyInlineBox() const
{
    if (!WTF::holds_alternative<BoxIteratorLegacyPath>(m_pathVariant))
        return nullptr;
    return WTF::get<BoxIteratorLegacyPath>(m_pathVariant).legacyInlineBox();
}

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)
inline const InlineDisplay::Box* Box::inlineBox() const
{
    if (!WTF::holds_alternative<BoxIteratorModernPath>(m_pathVariant))
        return nullptr;
    return &WTF::get<BoxIteratorModernPath>(m_pathVariant).box();
}
#endif

inline bool TextBox::hasHyphen() const
{
    return WTF::switchOn(m_pathVariant, [](auto& path) {
        return path.hasHyphen();
    });
}

inline TextBox::TextBox(PathVariant&& path)
    : Box(WTFMove(path))
{
}

inline StringView TextBox::text() const
{
    return WTF::switchOn(m_pathVariant, [](auto& path) {
        return path.text();
    });
}

inline unsigned TextBox::start() const
{
    return WTF::switchOn(m_pathVariant, [](auto& path) {
        return path.start();
    });
}

inline unsigned TextBox::end() const
{
    return WTF::switchOn(m_pathVariant, [](auto& path) {
        return path.end();
    });
}

inline unsigned TextBox::length() const
{
    return WTF::switchOn(m_pathVariant, [](auto& path) {
        return path.length();
    });
}

inline unsigned TextBox::offsetForPosition(float x) const
{
    return WTF::switchOn(m_pathVariant, [&](auto& path) {
        return path.offsetForPosition(x);
    });
}

inline float TextBox::positionForOffset(unsigned offset) const
{
    return WTF::switchOn(m_pathVariant, [&](auto& path) {
        return path.positionForOffset(offset);
    });
}

inline TextBoxSelectableRange TextBox::selectableRange() const
{
    return WTF::switchOn(m_pathVariant, [&](auto& path) {
        return path.selectableRange();
    });
}

inline TextRun TextBox::createTextRun() const
{
    return WTF::switchOn(m_pathVariant, [&](auto& path) {
        return path.createTextRun();
    });
}

}
}

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::InlineIterator::TextBox)
static bool isType(const WebCore::InlineIterator::Box& run) { return run.isText(); }
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::InlineIterator::TextBoxIterator)
static bool isType(const WebCore::InlineIterator::BoxIterator& runIterator) { return !runIterator || runIterator->isText(); }
SPECIALIZE_TYPE_TRAITS_END()
