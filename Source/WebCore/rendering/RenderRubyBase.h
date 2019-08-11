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

class RenderRubyRun;

class RenderRubyBase final : public RenderBlockFlow {
    WTF_MAKE_ISO_ALLOCATED(RenderRubyBase);
public:
    RenderRubyBase(Document&, RenderStyle&&);
    virtual ~RenderRubyBase();
    
    const char* renderName() const override { return "RenderRubyBase (anonymous)"; }
    
    RenderRubyRun* rubyRun() const;

    void setIsAfterExpansion(bool isAfterExpansion) { m_isAfterExpansion = isAfterExpansion; }
    bool isAfterExpansion() { return m_isAfterExpansion; }

    void setInitialOffset(float initialOffset) { m_initialOffset = initialOffset; }

    void reset()
    {
        m_initialOffset = 0;
        m_isAfterExpansion = true;
    }
    
    void cachePriorCharactersIfNeeded(const LazyLineBreakIterator&) override;

private:
    bool isRubyBase() const override { return true; }
    bool isChildAllowed(const RenderObject&, const RenderStyle&) const override;
    Optional<TextAlignMode> overrideTextAlignmentForLine(bool endsWithSoftBreak) const override;
    void adjustInlineDirectionLineBounds(int expansionOpportunityCount, float& logicalLeft, float& logicalWidth) const override;

    float m_initialOffset;
    unsigned m_isAfterExpansion : 1;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderRubyBase, isRubyBase())
