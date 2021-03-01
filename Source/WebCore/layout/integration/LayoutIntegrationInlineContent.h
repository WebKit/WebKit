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

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "LayoutIntegrationLine.h"
#include "LayoutIntegrationRun.h"
#include <wtf/IteratorRange.h>
#include <wtf/Vector.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class RenderBlockFlow;
class RenderObject;

namespace Layout {
class Box;
}

namespace LayoutIntegration {

class LineLayout;

struct InlineContent : public RefCounted<InlineContent> {
    static Ref<InlineContent> create(const LineLayout& lineLayout) { return adoptRef(*new InlineContent(lineLayout)); }
    ~InlineContent();

    using Runs = Vector<Run>;
    using Lines = Vector<Line>;

    Runs runs;
    Lines lines;
    Vector<NonRootInlineBox> nonRootInlineBoxes;

    float clearGapAfterLastLine { 0 };

    const Line& lineForRun(const Run& run) const { return lines[run.lineIndex()]; }
    WTF::IteratorRange<const Run*> runsForRect(const LayoutRect&) const;
    void shrinkToFit();

    const LineLayout& lineLayout() const;
    const RenderObject& rendererForLayoutBox(const Layout::Box&) const;
    const RenderBlockFlow& containingBlock() const;

private:
    InlineContent(const LineLayout&);

    WeakPtr<const LineLayout> m_lineLayout;
};

inline void InlineContent::shrinkToFit()
{
    runs.shrinkToFit();
    lines.shrinkToFit();
}

}
}

#endif
