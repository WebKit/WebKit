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

#include "InlineElementBox.h"
#include "LineLayoutTraversalComplexPath.h"
#include "LineLayoutTraversalDisplayRunPath.h"
#include "LineLayoutTraversalSimplePath.h"
#include <wtf/Variant.h>

namespace WebCore {

class RenderLineBreak;
class RenderText;

namespace LineLayoutTraversal {

class ElementBoxIterator;
class TextBoxIterator;

struct EndIterator { };

class Box {
public:
    using PathVariant = Variant<
#if ENABLE(LAYOUT_FORMATTING_CONTEXT)
        DisplayRunPath,
#endif
        ComplexPath,
        SimplePath
    >;

    Box(PathVariant&&);

    FloatRect rect() const;

    float baseline() const;

    bool isLeftToRightDirection() const;
    bool isHorizontal() const;
    bool dirOverride() const;
    bool isLineBreak() const;
    bool useLineBreakBoxRenderTreeDumpQuirk() const;

protected:
    friend class ElementBoxIterator;
    friend class TextBoxIterator;

    PathVariant m_pathVariant;
};

class TextBox : public Box {
public:
    TextBox(PathVariant&&);

    bool hasHyphen() const;
    StringView text() const;

    // These offsets are relative to the text renderer (not flow).
    unsigned localStartOffset() const;
    unsigned localEndOffset() const;
    unsigned length() const;

    bool isLastOnLine() const;
    bool isLast() const;
};

class TextBoxIterator {
public:
    TextBoxIterator() : m_textBox(ComplexPath { nullptr, { } }) { };
    TextBoxIterator(Box::PathVariant&&);

    TextBoxIterator& operator++() { return traverseNextInVisualOrder(); }
    TextBoxIterator& traverseNextInVisualOrder();
    TextBoxIterator& traverseNextInTextOrder();

    explicit operator bool() const { return !atEnd(); }

    bool operator==(const TextBoxIterator&) const;
    bool operator!=(const TextBoxIterator& other) const { return !(*this == other); }

    bool operator==(EndIterator) const { return atEnd(); }
    bool operator!=(EndIterator) const { return !atEnd(); }

    const TextBox& operator*() const { return m_textBox; }
    const TextBox* operator->() const { return &m_textBox; }

    bool atEnd() const;

private:
    TextBox m_textBox;
};

class ElementBoxIterator {
public:
    ElementBoxIterator() : m_box(ComplexPath { nullptr, { } }) { };
    ElementBoxIterator(Box::PathVariant&&);

    explicit operator bool() const { return !atEnd(); }

    const Box& operator*() const { return m_box; }
    const Box* operator->() const { return &m_box; }

    bool atEnd() const;

private:
    Box m_box;
};

class TextBoxRange {
public:
    TextBoxRange(TextBoxIterator begin)
        : m_begin(begin)
    {
    }

    TextBoxIterator begin() const { return m_begin; }
    EndIterator end() const { return { }; }

private:
    TextBoxIterator m_begin;
};

TextBoxIterator firstTextBoxFor(const RenderText&);
TextBoxIterator firstTextBoxInTextOrderFor(const RenderText&);
TextBoxRange textBoxesFor(const RenderText&);
ElementBoxIterator elementBoxFor(const RenderLineBreak&);

// -----------------------------------------------

inline Box::Box(PathVariant&& path)
    : m_pathVariant(WTFMove(path))
{
}

inline FloatRect Box::rect() const
{
    return WTF::switchOn(m_pathVariant, [](auto& path) {
        return path.rect();
    });
}

inline float Box::baseline() const
{
    return WTF::switchOn(m_pathVariant, [](auto& path) {
        return path.baseline();
    });
}

inline bool Box::isLeftToRightDirection() const
{
    return WTF::switchOn(m_pathVariant, [](auto& path) {
        return path.isLeftToRightDirection();
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

inline bool Box::useLineBreakBoxRenderTreeDumpQuirk() const
{
    return WTF::switchOn(m_pathVariant, [](auto& path) {
        return path.useLineBreakBoxRenderTreeDumpQuirk();
    });
}

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

inline unsigned TextBox::localStartOffset() const
{
    return WTF::switchOn(m_pathVariant, [](auto& path) {
        return path.localStartOffset();
    });
}

inline unsigned TextBox::localEndOffset() const
{
    return WTF::switchOn(m_pathVariant, [](auto& path) {
        return path.localEndOffset();
    });
}

inline unsigned TextBox::length() const
{
    return WTF::switchOn(m_pathVariant, [](auto& path) {
        return path.length();
    });
}

inline bool TextBox::isLastOnLine() const
{
    return WTF::switchOn(m_pathVariant, [](auto& path) {
        return path.isLastOnLine();
    });
}

inline bool TextBox::isLast() const
{
    return WTF::switchOn(m_pathVariant, [](auto& path) {
        return path.isLast();
    });
}


}
}
