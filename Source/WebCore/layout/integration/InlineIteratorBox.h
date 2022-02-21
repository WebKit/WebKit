/*
 * Copyright (C) 2019-2021 Apple Inc. All rights reserved.
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
#include <variant>

namespace WebCore {

class RenderLineBreak;
class RenderObject;
class RenderStyle;

namespace InlineIterator {

class LineIterator;
class BoxIterator;
class LeafBoxIterator;
class TextBoxIterator;

struct EndIterator { };

class Box {
public:
    using PathVariant = std::variant<
#if ENABLE(LAYOUT_FORMATTING_CONTEXT)
        BoxModernPath,
#endif
        BoxLegacyPath
    >;

    Box(PathVariant&&);

    bool isText() const;
    bool isInlineBox() const;
    bool isRootInlineBox() const;

    FloatRect rect() const;

    float logicalTop() const { return isHorizontal() ? rect().y() : rect().x(); }
    float logicalBottom() const { return isHorizontal() ? rect().maxY() : rect().maxX(); }
    float logicalLeft() const { return isHorizontal() ? rect().x() : rect().y(); }
    float logicalRight() const { return isHorizontal() ? rect().maxX() : rect().maxY(); }
    float logicalWidth() const { return isHorizontal() ? rect().width() : rect().height(); }
    float logicalHeight() const { return isHorizontal() ? rect().height() : rect().width(); }

    bool isHorizontal() const;
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

    // FIXME: Remove. For intermediate porting steps only.
    const LegacyInlineBox* legacyInlineBox() const;
#if ENABLE(LAYOUT_FORMATTING_CONTEXT)
    const InlineDisplay::Box* inlineBox() const;
#endif

    LeafBoxIterator nextOnLine() const;
    LeafBoxIterator previousOnLine() const;
    LeafBoxIterator nextOnLineIgnoringLineBreak() const;
    LeafBoxIterator previousOnLineIgnoringLineBreak() const;

    LineIterator line() const;

protected:
    friend class BoxIterator;
    friend class InlineBoxIterator;
    friend class LeafBoxIterator;
    friend class TextBoxIterator;

    // To help with debugging.
#if ENABLE(LAYOUT_FORMATTING_CONTEXT)
    const BoxModernPath& modernPath() const;
#endif
    const BoxLegacyPath& legacyPath() const;

    PathVariant m_pathVariant;
};

class BoxIterator {
public:
    explicit operator bool() const { return !atEnd(); }

    bool operator==(const BoxIterator&) const;
    bool operator!=(const BoxIterator& other) const { return !(*this == other); }

    bool operator==(EndIterator) const { return atEnd(); }
    bool operator!=(EndIterator) const { return !atEnd(); }

    const Box& operator*() const { return m_box; }
    const Box* operator->() const { return &m_box; }

    bool atEnd() const;

protected:
    BoxIterator() : m_box(BoxLegacyPath { nullptr }) { };
    BoxIterator(Box::PathVariant&&);
    BoxIterator(const Box&);

    Box m_box;
};

class LeafBoxIterator : public BoxIterator {
public:
    LeafBoxIterator() = default;
    LeafBoxIterator(Box::PathVariant&&);
    LeafBoxIterator(const Box&);

    LeafBoxIterator& traverseNextOnLine();
    LeafBoxIterator& traversePreviousOnLine();
    LeafBoxIterator& traverseNextOnLineIgnoringLineBreak();
    LeafBoxIterator& traversePreviousOnLineIgnoringLineBreak();
};

LeafBoxIterator boxFor(const RenderLineBreak&);
LeafBoxIterator boxFor(const RenderBox&);
#if ENABLE(LAYOUT_FORMATTING_CONTEXT)
LeafBoxIterator boxFor(const LayoutIntegration::InlineContent&, size_t boxIndex);
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

inline bool Box::isInlineBox() const
{
    return WTF::switchOn(m_pathVariant, [](auto& path) {
        return path.isInlineBox();
    });
}

inline bool Box::isRootInlineBox() const
{
    return WTF::switchOn(m_pathVariant, [](auto& path) {
        return path.isRootInlineBox();
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
    if (!std::holds_alternative<BoxLegacyPath>(m_pathVariant))
        return nullptr;
    return std::get<BoxLegacyPath>(m_pathVariant).legacyInlineBox();
}

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)
inline const InlineDisplay::Box* Box::inlineBox() const
{
    if (!std::holds_alternative<BoxModernPath>(m_pathVariant))
        return nullptr;
    return &std::get<BoxModernPath>(m_pathVariant).box();
}
#endif

}
}
