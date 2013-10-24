/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#ifndef SimpleLineLayoutResolver_h
#define SimpleLineLayoutResolver_h

#include "LayoutRect.h"
#include "RenderBlockFlow.h"
#include "RenderText.h"
#include "SimpleLineLayoutFunctions.h"
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {
namespace SimpleLineLayout {

class Resolver {
public:
    class Iterator;

    class Line {
    public:
        Line(const Resolver&, unsigned lineIndex);

        LayoutRect rect() const;
        LayoutPoint baseline() const;
        String text() const;

    private:
        const Resolver& m_resolver;
        unsigned m_lineIndex;
    };

    class Iterator {
    public:
        Iterator(const Resolver&, unsigned lineIndex);

        Iterator& operator++();
        Iterator& operator--();
        bool operator==(const Iterator&) const;
        bool operator!=(const Iterator&) const;

        Line operator*() const;

    private:
        const Resolver& m_resolver;
        unsigned m_lineIndex;
    };

    Resolver(const Lines&, const RenderBlockFlow&);

    unsigned size() const;

    Iterator begin() const;
    Iterator end() const;
    Iterator last() const;
    Iterator operator[](unsigned) const;

private:
    const Lines& m_lines;
    const String m_string;
    const LayoutUnit m_lineHeight;
    const LayoutUnit m_baseline;
    const float m_ascent;
    const float m_descent;
    const LayoutPoint m_contentOffset;
};

inline Resolver::Line::Line(const Resolver& resolver, unsigned lineIndex)
    : m_resolver(resolver)
    , m_lineIndex(lineIndex)
{
}

inline LayoutRect Resolver::Line::rect() const
{
    auto& line = m_resolver.m_lines[m_lineIndex];

    LayoutPoint linePosition(0, m_resolver.m_lineHeight * m_lineIndex + m_resolver.m_baseline - m_resolver.m_ascent);
    LayoutSize lineSize(ceiledLayoutUnit(line.width), m_resolver.m_ascent + m_resolver.m_descent);
    return LayoutRect(linePosition + m_resolver.m_contentOffset, lineSize);
}

inline LayoutPoint Resolver::Line::baseline() const
{
    float baselineY = m_resolver.m_lineHeight * m_lineIndex + m_resolver.m_baseline;
    return LayoutPoint(0, baselineY) + m_resolver.m_contentOffset;
}

inline String Resolver::Line::text() const
{
    auto& line = m_resolver.m_lines[m_lineIndex];
    return m_resolver.m_string.substringSharingImpl(line.textOffset, line.textLength);
}

inline Resolver::Iterator::Iterator(const Resolver& resolver, unsigned lineIndex)
    : m_resolver(resolver)
    , m_lineIndex(lineIndex)
{
}

inline Resolver::Iterator& Resolver::Iterator::operator++()
{
    ++m_lineIndex;
    return *this;
}

inline Resolver::Iterator& Resolver::Iterator::operator--()
{
    --m_lineIndex;
    return *this;
}

inline bool Resolver::Iterator::operator==(const Iterator& other) const
{
    ASSERT(&m_resolver == &other.m_resolver);
    return m_lineIndex == other.m_lineIndex;
}

inline bool Resolver::Iterator::operator!=(const Iterator& other) const
{
    return !(*this == other);
}

inline Resolver::Line Resolver::Iterator::operator*() const
{
    return Line(m_resolver, m_lineIndex);
}

inline Resolver::Resolver(const Lines& lines, const RenderBlockFlow& flow)
    : m_lines(lines)
    , m_string(toRenderText(*flow.firstChild()).text())
    , m_lineHeight(lineHeightFromFlow(flow))
    , m_baseline(baselineFromFlow(flow))
    , m_ascent(flow.style()->font().fontMetrics().ascent())
    , m_descent(flow.style()->font().fontMetrics().descent())
    , m_contentOffset(flow.borderLeft() + flow.paddingLeft(), flow.borderTop() + flow.paddingTop())
{
}

inline unsigned Resolver::size() const
{
    return m_lines.size();
}

inline Resolver::Iterator Resolver::begin() const
{
    return Iterator(*this, 0);
}

inline Resolver::Iterator Resolver::end() const
{
    return Iterator(*this, size());
}

inline Resolver::Iterator Resolver::last() const
{
    ASSERT(size());
    return Iterator(*this, size() - 1);
}

inline Resolver::Iterator Resolver::operator[](unsigned index) const
{
    ASSERT(index < size());
    return Iterator(*this, index);
}

}
}

#endif
