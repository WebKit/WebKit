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
        Iterator(const RunResolver&, unsigned lineIndex);

        Iterator& operator++();
        bool operator==(const Iterator&) const;
        bool operator!=(const Iterator&) const;

        Run operator*() const;

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

private:
    const Layout& m_layout;
    const String m_string;
    const LayoutUnit m_lineHeight;
    const LayoutUnit m_baseline;
    const float m_ascent;
    const float m_descent;
    const LayoutPoint m_contentOffset;
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

    LayoutPoint linePosition(floor(run.left), resolver.m_lineHeight * m_iterator.lineIndex() + resolver.m_baseline - resolver.m_ascent);
    LayoutSize lineSize(ceil(run.right) - floor(run.left), resolver.m_ascent + resolver.m_descent);
    return LayoutRect(linePosition + resolver.m_contentOffset, lineSize);
}

inline FloatPoint RunResolver::Run::baseline() const
{
    auto& resolver = m_iterator.resolver();
    auto& run = m_iterator.simpleRun();

    float baselineY = resolver.m_lineHeight * m_iterator.lineIndex() + resolver.m_baseline;
    return FloatPoint(run.left, baselineY) + resolver.m_contentOffset;
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

inline RunResolver::Iterator::Iterator(const RunResolver& resolver, unsigned runIndex)
    : m_resolver(resolver)
    , m_runIndex(runIndex)
    , m_lineIndex(0)
{
}

inline RunResolver::Iterator& RunResolver::Iterator::operator++()
{
    if (simpleRun().isEndOfLine)
        ++m_lineIndex;
    ++m_runIndex;
    return *this;
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

inline const SimpleLineLayout::Run& RunResolver::Iterator::simpleRun() const
{
    return m_resolver.m_layout.runAt(m_runIndex);
}

inline RunResolver::RunResolver(const RenderBlockFlow& flow, const Layout& layout)
    : m_layout(layout)
    , m_string(toRenderText(*flow.firstChild()).text())
    , m_lineHeight(lineHeightFromFlow(flow))
    , m_baseline(baselineFromFlow(flow))
    , m_ascent(flow.style().font().fontMetrics().ascent())
    , m_descent(flow.style().font().fontMetrics().descent())
    , m_contentOffset(flow.borderLeft() + flow.paddingLeft(), flow.borderTop() + flow.paddingTop())
{
}

inline RunResolver::Iterator RunResolver::begin() const
{
    return Iterator(*this, 0);
}

inline RunResolver::Iterator RunResolver::end() const
{
    return Iterator(*this, m_layout.runCount());
}

inline LineResolver::Iterator::Iterator(RunResolver::Iterator runIterator)
    : m_runIterator(runIterator)
{
}

inline LineResolver::Iterator& LineResolver::Iterator::operator++()
{
    unsigned previousLine = m_runIterator.lineIndex();
    while ((++m_runIterator).lineIndex() == previousLine) { }

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
    while ((++it).lineIndex() == currentLine)
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
