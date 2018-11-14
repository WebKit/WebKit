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

#include "FormattingState.h"
#include "InlineItem.h"
#include "InlineRun.h"
#include <wtf/IsoMalloc.h>
#include <wtf/OptionSet.h>

namespace WebCore {
namespace Layout {

// InlineFormattingState holds the state for a particular inline formatting context tree.
class InlineFormattingState : public FormattingState {
    WTF_MAKE_ISO_ALLOCATED(InlineFormattingState);
public:
    InlineFormattingState(Ref<FloatingState>&&, LayoutState&);
    virtual ~InlineFormattingState();

    std::unique_ptr<FormattingContext> formattingContext(const Box& formattingContextRoot) override;

    InlineContent& inlineContent() { return m_inlineContent; }
    InlineItem* lastInlineItem() const { return m_inlineContent.isEmpty() ? nullptr : m_inlineContent.last().get(); }

    // DetachingRule indicates whether the inline element needs to be wrapped in a dediceted run or break from previous/next runs.
    // <span>start</span><span style="position: relative;"> middle </span><span>end</span>
    // input to line breaking -> <start middle end>
    // output of line breaking (considering infinite constraint) -> <start middle end>
    // due to the in-flow positioning, the final runs are: <start>< middle ><end>
    // "start" -> n/a
    // " middle " -> BreakAtStart and BreakAtEnd
    // "end" -> n/a
    //
    // <span>parent </span><span style="padding: 10px;">start<span> middle </span>end</span><span> parent</span>
    // input to line breaking -> <parent start middle end parent>
    // output of line breaking (considering infinite constraint) -> <parent start middle end parent>
    // due to padding, final runs -> <parent><start middle end><parent>
    // "parent" -> n/a
    // "start" -> BreakAtStart
    // " middle " -> n/a
    // "end" -> BreakAtEnd
    // "parent" -> n/a
    enum class DetachingRule {
        BreakAtStart = 1 << 0,
        BreakAtEnd = 1 << 1
    };
    using DetachingRules = OptionSet<DetachingRule>;
    std::optional<DetachingRules> detachingRules(const Box& layoutBox) const;
    void addDetachingRule(const Box& layoutBox, DetachingRules detachingRules) { m_detachingRules.set(&layoutBox, detachingRules); }

    // Temp
    InlineRuns& inlineRuns() { return m_inlineRuns; }
    void appendInlineRun(InlineRun inlineRun) { m_inlineRuns.append(inlineRun); }

private:
    using DetachingRulesForInlineItems = HashMap<const Box*, DetachingRules>;

    InlineContent m_inlineContent;
    InlineRuns m_inlineRuns;
    DetachingRulesForInlineItems m_detachingRules;
};

}
}

SPECIALIZE_TYPE_TRAITS_LAYOUT_FORMATTING_STATE(InlineFormattingState, isInlineFormattingState())

#endif
