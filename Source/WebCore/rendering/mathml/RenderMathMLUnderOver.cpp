/*
 * Copyright (C) 2009 Alex Milowski (alex@milowski.com). All rights reserved.
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

#include "RenderMathMLUnderOver.h"

#include "MathMLElement.h"
#include "MathMLNames.h"
#include "RenderIterator.h"
#include "RenderMathMLOperator.h"

namespace WebCore {

using namespace MathMLNames;
    
RenderMathMLUnderOver::RenderMathMLUnderOver(Element& element, PassRef<RenderStyle> style)
    : RenderMathMLBlock(element, WTF::move(style))
{
    // Determine what kind of under/over expression we have by element name
    if (element.hasTagName(MathMLNames::munderTag))
        m_kind = Under;
    else if (element.hasTagName(MathMLNames::moverTag))
        m_kind = Over;
    else {
        ASSERT(element.hasTagName(MathMLNames::munderoverTag));
        m_kind = UnderOver;
    }
}

RenderMathMLOperator* RenderMathMLUnderOver::unembellishedOperator()
{
    RenderObject* base = firstChild();
    if (!base || !base->isRenderMathMLBlock())
        return 0;
    return toRenderMathMLBlock(base)->unembellishedOperator();
}

int RenderMathMLUnderOver::firstLineBaseline() const
{
    RenderBox* base = firstChildBox();
    if (!base)
        return -1;
    LayoutUnit baseline = base->firstLineBaseline();
    if (baseline != -1)
        baseline += base->logicalTop();
    return baseline;
}

void RenderMathMLUnderOver::layout()
{
    LayoutUnit stretchWidth = 0;
    for (RenderObject* child = firstChild(); child; child = child->nextSibling()) {
        if (child->needsLayout())
            toRenderElement(child)->layout();
        // Skipping the embellished op does not work for nested structures like
        // <munder><mover><mo>_</mo>...</mover> <mo>_</mo></munder>.
        if (child->isBox())
            stretchWidth = std::max<LayoutUnit>(stretchWidth, toRenderBox(child)->logicalWidth());
    }

    // Set the sizes of (possibly embellished) stretchy operator children.
    for (auto& child : childrenOfType<RenderMathMLBlock>(*this)) {
        if (auto renderOperator = child.unembellishedOperator())
            renderOperator->stretchTo(stretchWidth);
    }

    RenderMathMLBlock::layout();
}

}

#endif // ENABLE(MATHML)
