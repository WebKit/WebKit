/*
 * Copyright (C) 2010 Alex Milowski (alex@milowski.com). All rights reserved.
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

#if ENABLE(MATHML)

#include "RenderMathMLRow.h"

#include "MathMLNames.h"
#include "RenderIterator.h"
#include "RenderMathMLOperator.h"
#include "RenderMathMLRoot.h"

namespace WebCore {

using namespace MathMLNames;

RenderMathMLRow::RenderMathMLRow(Element& element, PassRef<RenderStyle> style)
    : RenderMathMLBlock(element, std::move(style))
{
}

RenderMathMLRow::RenderMathMLRow(Document& document, PassRef<RenderStyle> style)
    : RenderMathMLBlock(document, std::move(style))
{
}

RenderPtr<RenderMathMLRow> RenderMathMLRow::createAnonymousWithParentRenderer(RenderMathMLRoot& parent)
{
    RenderPtr<RenderMathMLRow> newMRow = createRenderer<RenderMathMLRow>(parent.document(), RenderStyle::createAnonymousStyleWithDisplay(&parent.style(), FLEX));
    newMRow->initializeStyle();
    return newMRow;
}

void RenderMathMLRow::layout()
{
    int stretchLogicalHeight = 0;
    for (RenderObject* child = firstChild(); child; child = child->nextSibling()) {
        if (child->needsLayout())
            toRenderElement(child)->layout();
        // FIXME: Only skip renderMo if it is stretchy.
        if (child->isRenderMathMLBlock() && toRenderMathMLBlock(child)->unembellishedOperator())
            continue;
        if (child->isBox())
            stretchLogicalHeight = std::max<int>(stretchLogicalHeight, roundToInt(toRenderBox(child)->logicalHeight()));
    }
    if (!stretchLogicalHeight)
        stretchLogicalHeight = style().fontSize();
    
    // Set the sizes of (possibly embellished) stretchy operator children.
    for (auto& child : childrenOfType<RenderMathMLBlock>(*this)) {
        if (auto renderMo = child.unembellishedOperator()) {
            if (renderMo->stretchHeight() != stretchLogicalHeight)
                renderMo->stretchToHeight(stretchLogicalHeight);
        }
    }

    RenderMathMLBlock::layout();
}

}

#endif // ENABLE(MATHML)
