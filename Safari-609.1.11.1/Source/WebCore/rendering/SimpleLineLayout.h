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

#pragma once

#include "SimpleLineLayoutCoverage.h"
#include "TextFlags.h"
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

#if COMPILER(MSVC)
#pragma warning(push)
#pragma warning(disable: 4200) // Disable "zero-sized array in struct/union" warning
#endif

namespace WebCore {

class RenderBlockFlow;

namespace SimpleLineLayout {

class RunResolver;

bool canUseFor(const RenderBlockFlow&);
AvoidanceReasonFlags canUseForWithReason(const RenderBlockFlow&, IncludeReasons);

struct Run {
#if COMPILER(MSVC)
    Run() { }
#endif
    Run(unsigned start, unsigned end, float logicalLeft, float logicalRight, bool isEndOfLine, bool hasHyphen, bool isLineBreak)
        : end(end)
        , start(start)
        , isEndOfLine(isEndOfLine)
        , hasHyphen(hasHyphen)
        , isLineBreak(isLineBreak)
        , logicalLeft(logicalLeft)
        , logicalRight(logicalRight)
    { }

    unsigned end;
    unsigned start : 29;
    unsigned isEndOfLine : 1;
    unsigned hasHyphen : 1;
    unsigned isLineBreak : 1;
    float logicalLeft;
    float logicalRight;
    // TODO: Move these optional items out of SimpleLineLayout::Run to a supplementary structure.
    float expansion { 0 };
    ExpansionBehavior expansionBehavior { ForbidLeadingExpansion | ForbidTrailingExpansion };
};

struct SimpleLineStrut {
    unsigned lineBreak;
    float offset;
};

class Layout : public RefCounted<Layout> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    using RunVector = Vector<Run, 10>;
    using SimpleLineStruts = Vector<SimpleLineStrut, 4>;
    static Ref<Layout> create(const RunVector&, unsigned lineCount, const RenderBlockFlow&);

    ~Layout();

    unsigned lineCount() const { return m_lineCount; }

    unsigned runCount() const { return m_runCount; }
    const Run& runAt(unsigned i) const { return m_runs[i]; }

    void setIsPaginated() { m_isPaginated = true; }
    bool isPaginated() const { return m_isPaginated; }
    bool hasLineStruts() const { return !m_lineStruts.isEmpty(); }
    void setLineStruts(SimpleLineStruts&& lineStruts) { m_lineStruts = lineStruts; }
    const SimpleLineStruts& struts() const { return m_lineStruts; }
    const RunResolver& runResolver() const;

private:
    Layout(const RunVector&, unsigned lineCount, const RenderBlockFlow&);

    unsigned m_lineCount;
    unsigned m_runCount;
    bool m_isPaginated { false };
    SimpleLineStruts m_lineStruts;
    const RenderBlockFlow& m_blockFlowRenderer;
    mutable std::unique_ptr<RunResolver> m_runResolver;
    Run m_runs[0];
};

Ref<Layout> create(RenderBlockFlow&);

}
}

#if COMPILER(MSVC)
#pragma warning(pop)
#endif
