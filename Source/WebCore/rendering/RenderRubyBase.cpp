/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 * Copyright (C) 2023 Apple Inc. All right reserved.
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

#include "config.h"

#include "RenderRubyBase.h"

#include "RenderBoxModelObjectInlines.h"
#include "RenderChildIterator.h"
#include "RenderRubyRun.h"
#include "RenderRubyText.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderRubyBase);

RenderRubyBase::RenderRubyBase(Document& document, RenderStyle&& style)
    : RenderBlockFlow(Type::RubyBase, document, WTFMove(style))
    , m_initialOffset(0)
    , m_isAfterExpansion(true)
{
    setInline(false);
    ASSERT(isRenderRubyBase());
}

RenderRubyBase::~RenderRubyBase() = default;

bool RenderRubyBase::isChildAllowed(const RenderObject& child, const RenderStyle&) const
{
    return child.isInline();
}

RenderRubyRun* RenderRubyBase::rubyRun() const
{
    ASSERT(parent());
    return downcast<RenderRubyRun>(parent());
}

std::optional<TextAlignMode> RenderRubyBase::overrideTextAlignmentForLine(bool /* endsWithSoftBreak */) const
{
    return TextAlignMode::Justify;
}

void RenderRubyBase::adjustInlineDirectionLineBounds(int expansionOpportunityCount, float& logicalLeft, float& logicalWidth) const
{
    if (rubyRun()->hasOverridingLogicalWidth() && firstRootBox() && !firstRootBox()->nextRootBox()) {
        logicalLeft += m_initialOffset;
        logicalWidth -= 2 * m_initialOffset;
        return;
    }

    LayoutUnit maxPreferredLogicalWidth = rubyRun() && rubyRun()->hasOverridingLogicalWidth() ? rubyRun()->overridingLogicalWidth() : this->maxPreferredLogicalWidth();
    if (maxPreferredLogicalWidth >= logicalWidth)
        return;

    // Inset the ruby base by half the inter-ideograph expansion amount.
    float inset = (logicalWidth - maxPreferredLogicalWidth) / (expansionOpportunityCount + 1);

    logicalLeft += inset / 2;
    logicalWidth -= inset;
}

void RenderRubyBase::cachePriorCharactersIfNeeded(const CachedLineBreakIteratorFactory& lineBreakIteratorFactory)
{
    auto* run = rubyRun();
    if (run)
        run->setCachedPriorCharacters(lineBreakIteratorFactory.priorContext().lastCharacter(), lineBreakIteratorFactory.priorContext().secondToLastCharacter());
}

bool RenderRubyBase::isEmptyOrHasInFlowContent() const
{
    auto* firstChild = dynamicDowncast<RenderElement>(this->firstChild());
    if (!firstChild)
        return true;

    if (firstChild->isOutOfFlowPositioned())
        return false;

    for (auto& child : childrenOfType<RenderObject>(*firstChild)) {
        if (!child.isOutOfFlowPositioned())
            return true;
    }
    return false;
}

} // namespace WebCore
