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
    
RenderMathMLUnderOver::RenderMathMLUnderOver(Element& element, Ref<RenderStyle>&& style)
    : RenderMathMLBlock(element, WTFMove(style))
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
    if (!is<RenderMathMLBlock>(base))
        return nullptr;
    return downcast<RenderMathMLBlock>(*base).unembellishedOperator();
}

Optional<int> RenderMathMLUnderOver::firstLineBaseline() const
{
    RenderBox* base = firstChildBox();
    if (!base)
        return Optional<int>();
    Optional<int> baseline = base->firstLineBaseline();
    if (baseline)
        baseline.value() += static_cast<int>(base->logicalTop());
    return baseline;
}

void RenderMathMLUnderOver::layout()
{
    LayoutUnit stretchWidth = 0;
    Vector<RenderMathMLOperator*, 2> renderOperators;

    for (RenderObject* child = firstChild(); child; child = child->nextSibling()) {
        if (child->needsLayout()) {
            if (is<RenderMathMLBlock>(child)) {
                if (auto renderOperator = downcast<RenderMathMLBlock>(*child).unembellishedOperator()) {
                    if (!renderOperator->isVertical()) {
                        renderOperator->resetStretchSize();
                        renderOperators.append(renderOperator);
                    }
                }
            }

            downcast<RenderElement>(*child).layout();
        }

        // Skipping the embellished op does not work for nested structures like
        // <munder><mover><mo>_</mo>...</mover> <mo>_</mo></munder>.
        if (is<RenderBox>(*child))
            stretchWidth = std::max<LayoutUnit>(stretchWidth, downcast<RenderBox>(*child).logicalWidth());
    }

    // Set the sizes of (possibly embellished) stretchy operator children.
    for (auto& renderOperator : renderOperators)
        renderOperator->stretchTo(stretchWidth);

    RenderMathMLBlock::layout();
}

}

#endif // ENABLE(MATHML)
