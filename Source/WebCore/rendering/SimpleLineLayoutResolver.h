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

    class Run {
    public:
        Run(const Resolver&, unsigned lineIndex);

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

        Run operator*() const;

    private:
        const Resolver& m_resolver;
        unsigned m_lineIndex;
    };

    Resolver(const RenderBlockFlow&, const Layout&);

    unsigned size() const;

    Iterator begin() const;
    Iterator end() const;
    Iterator last() const;
    Iterator operator[](unsigned) const;

private:
    const Layout& m_layout;
    const String m_string;
    const LayoutUnit m_lineHeight;
    const LayoutUnit m_baseline;
    const float m_ascent;
    const float m_descent;
    const LayoutPoint m_contentOffset;
};

Resolver runResolver(const RenderBlockFlow&, const Layout&);
Resolver lineResolver(const RenderBlockFlow&, const Layout&);

inline Resolver::Run::Run(const Resolver& resolver, unsigned lineIndex)
    : m_resolver(resolver)
    , m_lineIndex(lineIndex)
{
}

inline LayoutRect Resolver::Run::rect() const
{
    auto& run = m_resolver.m_layout.runs[m_lineIndex];

    LayoutPoint linePosition(run.left, m_resolver.m_lineHeight * m_lineIndex + m_resolver.m_baseline - m_resolver.m_ascent);
    LayoutSize lineSize(run.width, m_resolver.m_ascent + m_resolver.m_descent);
    return LayoutRect(linePosition + m_resolver.m_contentOffset, lineSize);
}

inline LayoutPoint Resolver::Run::baseline() const
{
    auto& run = m_resolver.m_layout.runs[m_lineIndex];

    float baselineY = m_resolver.m_lineHeight * m_lineIndex + m_resolver.m_baseline;
    return LayoutPoint(run.left, baselineY) + m_resolver.m_contentOffset;
}

inline String Resolver::Run::text() const
{
    auto& run = m_resolver.m_layout.runs[m_lineIndex];
    return m_resolver.m_string.substringSharingImpl(run.textOffset, run.textLength);
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

inline Resolver::Run Resolver::Iterator::operator*() const
{
    return Run(m_resolver, m_lineIndex);
}

inline Resolver::Resolver(const RenderBlockFlow& flow, const Layout& layout)
    : m_layout(layout)
    , m_string(toRenderText(*flow.firstChild()).text())
    , m_lineHeight(lineHeightFromFlow(flow))
    , m_baseline(baselineFromFlow(flow))
    , m_ascent(flow.style().font().fontMetrics().ascent())
    , m_descent(flow.style().font().fontMetrics().descent())
    , m_contentOffset(flow.borderLeft() + flow.paddingLeft(), flow.borderTop() + flow.paddingTop())
{
}

inline unsigned Resolver::size() const
{
    return m_layout.runs.size();
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

inline Resolver runResolver(const RenderBlockFlow& flow, const Layout& layout)
{
    return Resolver(flow, layout);
}

inline Resolver lineResolver(const RenderBlockFlow& flow, const Layout& layout)
{
    return Resolver(flow, layout);
}

}
}

#endif
