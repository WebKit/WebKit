/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "RenderBlockFlow.h"

namespace WebCore {

class RenderRubyBase;
class RenderRubyText;

// RenderRubyRuns are 'inline-block/table' like objects, and wrap a single pairing of a ruby base with its ruby text(s).
// See RenderRuby.h for further comments on the structure

class RenderRubyRun final : public RenderBlockFlow {
    WTF_MAKE_ISO_ALLOCATED(RenderRubyRun);
public:
    RenderRubyRun(Document&, RenderStyle&&);
    virtual ~RenderRubyRun();

    bool hasRubyText() const;
    bool hasRubyBase() const;
    RenderRubyText* rubyText() const;
    RenderRubyBase* rubyBase() const;

    void layoutExcludedChildren(bool relayoutChildren) override;
    void layout() override;
    void layoutBlock(bool relayoutChildren, LayoutUnit pageHeight = 0_lu) override;

    bool isChildAllowed(const RenderObject&, const RenderStyle&) const override;

    RenderBlock* firstLineBlock() const override;

    void getOverhang(bool firstLine, RenderObject* startRenderer, RenderObject* endRenderer, float& startOverhang, float& endOverhang) const;

    static RenderPtr<RenderRubyRun> staticCreateRubyRun(const RenderObject* parentRuby);
    
    void updatePriorContextFromCachedBreakIterator(LazyLineBreakIterator&) const;
    void setCachedPriorCharacters(UChar last, UChar secondToLast)
    {
        m_lastCharacter = last;
        m_secondToLastCharacter = secondToLast;
    }
    bool canBreakBefore(const LazyLineBreakIterator&) const;
    
    RenderPtr<RenderRubyBase> createRubyBase() const;

private:
    bool isRubyRun() const override { return true; }
    const char* renderName() const override { return "RenderRubyRun (anonymous)"; }
    bool createsAnonymousWrapper() const override { return true; }

private:
    UChar m_lastCharacter;
    UChar m_secondToLastCharacter;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderRubyRun, isRubyRun())
