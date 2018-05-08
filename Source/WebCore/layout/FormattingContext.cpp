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
#include "FormattingContext.h"

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "LayoutBox.h"
#include "LayoutContainer.h"
#include "LayoutContext.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {
namespace Layout {

WTF_MAKE_ISO_ALLOCATED_IMPL(FormattingContext);

FormattingContext::FormattingContext(const Box& formattingContextRoot)
    : m_root(makeWeakPtr(const_cast<Box&>(formattingContextRoot)))
{
}

FormattingContext::~FormattingContext()
{
}

void FormattingContext::computeStaticPosition(LayoutContext&, const Box&, Display::Box&) const
{
}

void FormattingContext::computeInFlowPositionedPosition(const Box&, Display::Box&) const
{
}

void FormattingContext::computeOutOfFlowPosition(const Box&, Display::Box&) const
{
}

void FormattingContext::computeWidth(const Box& layoutBox, Display::Box& displayBox) const
{
    if (layoutBox.isOutOfFlowPositioned())
        return computeOutOfFlowWidth(layoutBox, displayBox);
    if (layoutBox.isFloatingPositioned())
        return computeFloatingWidth(layoutBox, displayBox);
    return computeInFlowWidth(layoutBox, displayBox);
}

void FormattingContext::computeHeight(const Box& layoutBox, Display::Box& displayBox) const
{
    if (layoutBox.isOutOfFlowPositioned())
        return computeOutOfFlowHeight(layoutBox, displayBox);
    if (layoutBox.isFloatingPositioned())
        return computeFloatingHeight(layoutBox, displayBox);
    return computeInFlowHeight(layoutBox, displayBox);
}

void FormattingContext::computeOutOfFlowWidth(const Box&, Display::Box&) const
{
}

void FormattingContext::computeFloatingWidth(const Box&, Display::Box&) const
{
}

void FormattingContext::computeOutOfFlowHeight(const Box&, Display::Box&) const
{
}

void FormattingContext::computeFloatingHeight(const Box&, Display::Box&) const
{
}

LayoutUnit FormattingContext::marginTop(const Box&) const
{
    return 0;
}

LayoutUnit FormattingContext::marginLeft(const Box&) const
{
    return 0;
}

LayoutUnit FormattingContext::marginBottom(const Box&) const
{
    return 0;
}

LayoutUnit FormattingContext::marginRight(const Box&) const
{
    return 0;
}

void FormattingContext::placeInFlowPositionedChildren(const Container&) const
{
}

void FormattingContext::layoutOutOfFlowDescendants(LayoutContext& layoutContext) const
{
    if (!is<Container>(m_root.get()))
        return;
    for (auto& outOfFlowBox : downcast<Container>(*m_root).outOfFlowDescendants()) {
        auto& layoutBox = *outOfFlowBox;
        auto& displayBox = layoutContext.createDisplayBox(layoutBox);

        computeOutOfFlowPosition(layoutBox, displayBox);
        computeOutOfFlowWidth(layoutBox, displayBox);

        ASSERT(layoutBox.establishesFormattingContext());
        auto formattingContext = layoutContext.formattingContext(layoutBox);
        formattingContext->layout(layoutContext, layoutContext.establishedFormattingState(layoutBox, *formattingContext));

        computeOutOfFlowHeight(layoutBox, displayBox);
    }
}

}
}
#endif
