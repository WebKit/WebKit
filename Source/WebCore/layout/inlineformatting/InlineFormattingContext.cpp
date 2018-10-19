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

#include "config.h"
#include "InlineFormattingContext.h"

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "FloatingState.h"
#include "InlineFormattingState.h"
#include "LayoutBox.h"
#include "LayoutContainer.h"
#include "LayoutContext.h"
#include "LayoutInlineBox.h"
#include "LayoutInlineContainer.h"
#include "Logging.h"
#include "SimpleLineBreaker.h"
#include "TextContentProvider.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace Layout {

WTF_MAKE_ISO_ALLOCATED_IMPL(InlineFormattingContext);

InlineFormattingContext::InlineFormattingContext(const Box& formattingContextRoot)
    : FormattingContext(formattingContextRoot)
{
}

void InlineFormattingContext::layout(LayoutContext& layoutContext, FormattingState& inlineFormattingState) const
{
    if (!is<Container>(root()))
        return;

    LOG_WITH_STREAM(FormattingContextLayout, stream << "[Start] -> inline formatting context -> layout context(" << &layoutContext << ") formatting root(" << &root() << ")");

    TextContentProvider textContentProvider;
    auto& formattingRoot = downcast<Container>(root());
    auto* layoutBox = formattingRoot.firstInFlowOrFloatingChild();
    // Casually walk through the block's descendants and place the inline boxes one after the other as much as we can (yeah, I am looking at you floats).
    while (layoutBox) {
        if (is<Container>(layoutBox)) {
            ASSERT(is<InlineContainer>(layoutBox));
            layoutBox = downcast<Container>(*layoutBox).firstInFlowOrFloatingChild();
            continue;
        }
        auto& inlineBox = downcast<InlineBox>(*layoutBox);
        // Only text content at this point.
        if (inlineBox.hasTextContent())
            textContentProvider.appendText(inlineBox.textContent(), inlineBox.style(), true);

        for (; layoutBox; layoutBox = layoutBox->containingBlock()) {
            if (layoutBox == &formattingRoot) {
                layoutBox = nullptr;
                break;
            }
            if (auto* nextSibling = layoutBox->nextInFlowOrFloatingSibling()) {
                layoutBox = nextSibling;
                break;
            }
        }
        ASSERT(!layoutBox || layoutBox->isDescendantOf(formattingRoot));
    }

    auto& formattingRootDisplayBox = layoutContext.displayBoxForLayoutBox(formattingRoot);
    auto lineLeft = formattingRootDisplayBox.contentBoxLeft();
    auto lineRight = formattingRootDisplayBox.contentBoxRight();

    SimpleLineBreaker::LineConstraintList constraints;
    constraints.append({ { }, lineLeft, lineRight });
    auto textRunList = textContentProvider.textRuns();
    SimpleLineBreaker simpleLineBreaker(textRunList, textContentProvider, WTFMove(constraints), formattingRoot.style());

    // Since we don't yet have a display tree context for inline boxes, let's just cache the runs on the state so that they can be verified against the sll/inline tree runs later.
    ASSERT(is<InlineFormattingState>(inlineFormattingState));
    downcast<InlineFormattingState>(inlineFormattingState).addLayoutRuns(simpleLineBreaker.runs());

    LOG_WITH_STREAM(FormattingContextLayout, stream << "[End] -> inline formatting context -> layout context(" << &layoutContext << ") formatting root(" << &root() << ")");
}

void InlineFormattingContext::computeStaticPosition(const LayoutContext&, const Box&) const
{
}

void InlineFormattingContext::computeInFlowPositionedPosition(const LayoutContext&, const Box&) const
{
}

FormattingContext::InstrinsicWidthConstraints InlineFormattingContext::instrinsicWidthConstraints(LayoutContext&, const Box&) const
{
    return { };
}

}
}

#endif
