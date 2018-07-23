/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "TextFlags.h"

namespace WebCore {
namespace Layout {

using ContentPosition = unsigned;
using ItemPosition = unsigned;

struct TextRun {
public:
    enum class Type {
        Whitespace,
        NonWhitespace,
        SoftLineBreak,
        HardLineBreak,
        Invalid
    };
    static TextRun createWhitespaceRun(ContentPosition start, ContentPosition end, float width, bool isCollapsed);
    static TextRun createNonWhitespaceRun(ContentPosition start, ContentPosition end, float width);
    static TextRun createNonWhitespaceRunWithHyphen(ContentPosition start, ContentPosition end, float width);
    static TextRun createSoftLineBreakRun(ContentPosition);
    static TextRun createHardLineBreakRun(ContentPosition);

    TextRun() = default;
    TextRun(ContentPosition start, ContentPosition end, Type, float width = 0, bool isCollapsed = false, bool hasHyphen = false);

    ContentPosition start() const;
    ContentPosition end() const;
    unsigned length() const;

    float width() const;

    bool isWhitespace() const { return m_type == Type::Whitespace; }
    bool isNonWhitespace() const { return m_type == Type::NonWhitespace; }
    bool isLineBreak() const { return isSoftLineBreak() || isHardLineBreak(); }
    bool isSoftLineBreak() const { return m_type == Type::SoftLineBreak; }
    bool isHardLineBreak() const { return m_type == Type::HardLineBreak; }
    bool isValid() const { return m_type != Type::Invalid; }
    bool isCollapsed() const { return m_isCollapsed; }
    Type type() const { return m_type; }

    void setIsCollapsed(bool isCollapsed) { m_isCollapsed = isCollapsed; }
    bool hasHyphen() const { return m_hasHyphen; }
    void setWidth(float width) { m_width = width; }

private:
    ContentPosition m_start { 0 };
    ContentPosition m_end { 0 };
    Type m_type { Type::Invalid };
    float m_width { 0 };
    bool m_isCollapsed { false };
    bool m_hasHyphen { false };
};

struct LayoutRun {
public:
    LayoutRun(ContentPosition start, ContentPosition end, float left, float right, bool hasHyphen);

    ContentPosition start() const { return m_start; }
    ContentPosition end() const { return m_end; }
    unsigned length() const { return end() - start(); }

    float left() const { return m_left; }
    float right() const { return m_right; }
    float width() const { return right() - left(); }

    bool isEndOfLine() const { return m_isEndOfLine; }

    void setEnd(ContentPosition end) { m_end = end; }
    void setLeft(float left) { m_left = left; }
    void setRight(float right) { m_right = right; }
    void setIsEndOfLine() { m_isEndOfLine = true; }

    void setExpansion(ExpansionBehavior, float expansion);
    void setHasHyphen() { m_hasHyphen = true; }
    bool hasHyphen() const { return m_hasHyphen; }

private:
    ContentPosition m_start { 0 };
    ContentPosition m_end { 0 };
    float m_left { 0 };
    float m_right { 0 };
    bool m_isEndOfLine { false };
    bool m_hasHyphen { false };
    float m_expansion { 0 };
    ExpansionBehavior m_expansionBehavior { ForbidLeadingExpansion | ForbidTrailingExpansion };
};

template<typename T>
class ConstVectorIterator {
public:
    ConstVectorIterator(const Vector<T>&);

    const T* current() const;
    ConstVectorIterator<T>& operator++();
    void reset() { m_index = 0; }

private:
    const Vector<T>& m_content;
    unsigned m_index { 0 };
};

template<typename T>
inline ConstVectorIterator<T>::ConstVectorIterator(const Vector<T>& content)
    : m_content(content)
{
}

template<typename T>
inline const T* ConstVectorIterator<T>::current() const
{
    if (m_index == m_content.size())
        return nullptr;
    return &m_content[m_index];
}

template<typename T>
inline ConstVectorIterator<T>& ConstVectorIterator<T>::operator++()
{
    ++m_index;
    return *this;
}

inline LayoutRun::LayoutRun(ContentPosition start, ContentPosition end, float left, float right, bool hasHyphen)
    : m_start(start)
    , m_end(end)
    , m_left(left)
    , m_right(right)
    , m_hasHyphen(hasHyphen)
{
}

inline void LayoutRun::setExpansion(ExpansionBehavior expansionBehavior, float expansion)
{
    m_expansionBehavior = expansionBehavior;
    m_expansion = expansion;
}

inline TextRun TextRun::createWhitespaceRun(ContentPosition start, ContentPosition end, float width, bool isCollapsed)
{
    return { start, end, Type::Whitespace, width, isCollapsed };
}

inline TextRun TextRun::createNonWhitespaceRun(ContentPosition start, ContentPosition end, float width)
{
    return { start, end, Type::NonWhitespace, width };
}

inline TextRun TextRun::createNonWhitespaceRunWithHyphen(ContentPosition start, ContentPosition end, float width)
{
    return { start, end, Type::NonWhitespace, width, false, true };
}

inline TextRun TextRun::createSoftLineBreakRun(ContentPosition position)
{
    return { position, position + 1, Type::SoftLineBreak };
}

inline TextRun TextRun::createHardLineBreakRun(ContentPosition position)
{
    return { position, position, Type::HardLineBreak };
}

inline TextRun::TextRun(ContentPosition start, ContentPosition end, Type type, float width, bool isCollapsed, bool hasHyphen)
    : m_start(start)
    , m_end(end)
    , m_type(type)
    , m_width(width)
    , m_isCollapsed(isCollapsed)
    , m_hasHyphen(hasHyphen)
{
}

inline ContentPosition TextRun::start() const
{
    ASSERT(type() != Type::Invalid);
    return m_start;
}

inline ContentPosition TextRun::end() const
{
    ASSERT(type() != Type::Invalid);
    return m_end;
}

inline unsigned TextRun::length() const
{
    ASSERT(type() != Type::Invalid);
    return m_end - m_start;
}

inline float TextRun::width() const
{
    ASSERT(type() != Type::Invalid);
    return m_width;
}

}
}
#endif
