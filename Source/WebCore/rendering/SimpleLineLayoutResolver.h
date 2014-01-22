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

template <class IteratorType>
class Range {
public:
    Range(IteratorType begin, IteratorType end) : m_begin(begin), m_end(end) { }

    IteratorType begin() const { return m_begin; }
    IteratorType end() const { return m_end; }

private:
    IteratorType m_begin;
    IteratorType m_end;
};

class RunResolver {
public:
    class Iterator;

    class Run {
    public:
        explicit Run(const Iterator&);

        unsigned start() const;
        unsigned end() const;

        LayoutRect rect() const;
        FloatPoint baseline() const;
        String text() const;

        unsigned lineIndex() const;

    private:
        const Iterator& m_iterator;
    };

    class Iterator {
    public:
        Iterator(const RunResolver&, unsigned runIndex, unsigned lineIndex);

        Iterator& operator++();

        bool operator==(const Iterator&) const;
        bool operator!=(const Iterator&) const;

        Run operator*() const;

        Iterator& advance();
        Iterator& advanceLines(unsigned);

        const RunResolver& resolver() const { return m_resolver; }
        const SimpleLineLayout::Run& simpleRun() const;
        unsigned lineIndex() const { return m_lineIndex; }

    private:
        const RunResolver& m_resolver;
        unsigned m_runIndex;
        unsigned m_lineIndex;
    };

    RunResolver(const RenderBlockFlow&, const Layout&);

    Iterator begin() const;
    Iterator end() const;

    Range<Iterator> rangeForRect(const LayoutRect&) const;

private:
    unsigned lineIndexForHeight(LayoutUnit) const;

    const Layout& m_layout;
    const String m_string;
    const LayoutUnit m_lineHeight;
    const LayoutUnit m_baseline;
    const LayoutUnit m_borderAndPaddingBefore;
    const float m_ascent;
    const float m_descent;
};

class LineResolver {
public:
    class Iterator;

    class Iterator {
    public:
        explicit Iterator(RunResolver::Iterator);

        Iterator& operator++();
        bool operator==(const Iterator&) const;
        bool operator!=(const Iterator&) const;

        const LayoutRect operator*() const;

    private:
        RunResolver::Iterator m_runIterator;
        LayoutRect m_rect;
    };

    LineResolver(const RenderBlockFlow&, const Layout&);

    Iterator begin() const;
    Iterator end() const;

    Range<Iterator> rangeForRect(const LayoutRect&) const;

private:
    RunResolver m_runResolver;
};

RunResolver runResolver(const RenderBlockFlow&, const Layout&);
LineResolver lineResolver(const RenderBlockFlow&, const Layout&);

inline RunResolver::Run::Run(const Iterator& iterator)
    : m_iterator(iterator)
{
}

inline unsigned RunResolver::Run::start() const
{
    return m_iterator.simpleRun().start;
}

inline unsigned RunResolver::Run::end() const
{
    return m_iterator.simpleRun().end;
}

inline LayoutRect RunResolver::Run::rect() const
{
    auto& resolver = m_iterator.resolver();
    auto& run = m_iterator.simpleRun();

    float baselinePosition = resolver.m_lineHeight * m_iterator.lineIndex() + resolver.m_baseline;
    LayoutPoint linePosition(LayoutUnit::fromFloatFloor(run.left), roundToInt(baselinePosition - resolver.m_ascent + resolver.m_borderAndPaddingBefore));
    LayoutSize lineSize(LayoutUnit::fromFloatCeil(run.right) - LayoutUnit::fromFloatFloor(run.left), resolver.m_ascent + resolver.m_descent);
    return LayoutRect(linePosition, lineSize);
}

inline FloatPoint RunResolver::Run::baseline() const
{
    auto& resolver = m_iterator.resolver();
    auto& run = m_iterator.simpleRun();

    float baselinePosition = resolver.m_lineHeight * m_iterator.lineIndex() + resolver.m_baseline;
    return FloatPoint(run.left, roundToInt(baselinePosition + resolver.m_borderAndPaddingBefore));
}

inline String RunResolver::Run::text() const
{
    auto& resolver = m_iterator.resolver();
    auto& run = m_iterator.simpleRun();
    return resolver.m_string.substringSharingImpl(run.start, run.end - run.start);
}

inline unsigned RunResolver::Run::lineIndex() const
{
    return m_iterator.lineIndex();
}

inline RunResolver::Iterator::Iterator(const RunResolver& resolver, unsigned runIndex, unsigned lineIndex)
    : m_resolver(resolver)
    , m_runIndex(runIndex)
    , m_lineIndex(lineIndex)
{
}

inline RunResolver::Iterator& RunResolver::Iterator::operator++()
{
    return advance();
}

inline bool RunResolver::Iterator::operator==(const Iterator& other) const
{
    ASSERT(&m_resolver == &other.m_resolver);
    return m_runIndex == other.m_runIndex;
}

inline bool RunResolver::Iterator::operator!=(const Iterator& other) const
{
    return !(*this == other);
}

inline RunResolver::Run RunResolver::Iterator::operator*() const
{
    return Run(*this);
}

inline RunResolver::Iterator& RunResolver::Iterator::advance()
{
    if (simpleRun().isEndOfLine)
        ++m_lineIndex;
    ++m_runIndex;
    return *this;
}

inline RunResolver::Iterator& RunResolver::Iterator::advanceLines(unsigned lineCount)
{
    unsigned runCount = m_resolver.m_layout.runCount();
    if (runCount == m_resolver.m_layout.lineCount()) {
        m_runIndex = std::min(runCount, m_runIndex + lineCount);
        m_lineIndex = m_runIndex;
        return *this;
    }
    unsigned target = m_lineIndex + lineCount;
    while (m_lineIndex < target && m_runIndex < runCount)
        advance();

    return *this;
}

inline const SimpleLineLayout::Run& RunResolver::Iterator::simpleRun() const
{
    return m_resolver.m_layout.runAt(m_runIndex);
}

inline RunResolver::RunResolver(const RenderBlockFlow& flow, const Layout& layout)
    : m_layout(layout)
    , m_string(toRenderText(*flow.firstChild()).text())
    , m_lineHeight(lineHeightFromFlow(flow))
    , m_baseline(baselineFromFlow(flow))
    , m_borderAndPaddingBefore(flow.borderAndPaddingBefore())
    , m_ascent(flow.style().font().fontMetrics().ascent())
    , m_descent(flow.style().font().fontMetrics().descent())
{
}

inline RunResolver::Iterator RunResolver::begin() const
{
    return Iterator(*this, 0, 0);
}

inline RunResolver::Iterator RunResolver::end() const
{
    return Iterator(*this, m_layout.runCount(), m_layout.lineCount());
}

inline unsigned RunResolver::lineIndexForHeight(LayoutUnit height) const
{
    ASSERT(m_lineHeight);
    float y = std::max<float>(height - m_borderAndPaddingBefore - m_baseline + m_ascent, 0);
    return std::min<unsigned>(y / m_lineHeight, m_layout.lineCount() - 1);
}

inline Range<RunResolver::Iterator> RunResolver::rangeForRect(const LayoutRect& rect) const
{
    if (!m_lineHeight)
        return Range<Iterator>(begin(), end());

    unsigned firstLine = lineIndexForHeight(rect.y());
    unsigned lastLine = lineIndexForHeight(rect.maxY());

    auto rangeBegin = begin().advanceLines(firstLine);
    if (rangeBegin == end())
        return Range<Iterator>(end(), end());
    auto rangeEnd = rangeBegin;
    rangeEnd.advanceLines(lastLine - firstLine + 1);
    return Range<Iterator>(rangeBegin, rangeEnd);
}

inline LineResolver::Iterator::Iterator(RunResolver::Iterator runIterator)
    : m_runIterator(runIterator)
{
}

inline LineResolver::Iterator& LineResolver::Iterator::operator++()
{
    m_runIterator.advanceLines(1);
    return *this;
}

inline bool LineResolver::Iterator::operator==(const Iterator& other) const
{
    return m_runIterator == other.m_runIterator;
}

inline bool LineResolver::Iterator::operator!=(const Iterator& other) const
{
    return m_runIterator != other.m_runIterator;
}

inline const LayoutRect LineResolver::Iterator::operator*() const
{
    unsigned currentLine = m_runIterator.lineIndex();
    auto it = m_runIterator;
    LayoutRect rect = (*it).rect();
    while (it.advance().lineIndex() == currentLine)
        rect.unite((*it).rect());

    return rect;
}

inline LineResolver::LineResolver(const RenderBlockFlow& flow, const Layout& layout)
    : m_runResolver(flow, layout)
{
}

inline LineResolver::Iterator LineResolver::begin() const
{
    return Iterator(m_runResolver.begin());
}

inline LineResolver::Iterator LineResolver::end() const
{
    return Iterator(m_runResolver.end());
}

inline Range<LineResolver::Iterator> LineResolver::rangeForRect(const LayoutRect& rect) const
{
    auto runRange = m_runResolver.rangeForRect(rect);
    return Range<Iterator>(Iterator(runRange.begin()), Iterator(runRange.end()));
}

inline RunResolver runResolver(const RenderBlockFlow& flow, const Layout& layout)
{
    return RunResolver(flow, layout);
}

inline LineResolver lineResolver(const RenderBlockFlow& flow, const Layout& layout)
{
    return LineResolver(flow, layout);
}

}
}

#endif
