/*
 * Copyright (C) 2016 Igalia S.L. All rights reserved.
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
#include "MathMLStyle.h"

#if ENABLE(MATHML)

#include "MathMLElement.h"
#include "MathMLNames.h"
#include "RenderMathMLBlock.h"
#include "RenderMathMLFraction.h"
#include "RenderMathMLMath.h"
#include "RenderMathMLRoot.h"
#include "RenderMathMLScripts.h"
#include "RenderMathMLToken.h"
#include "RenderMathMLUnderOver.h"

namespace WebCore {

using namespace MathMLNames;

Ref<MathMLStyle> MathMLStyle::create()
{
    return adoptRef(*new MathMLStyle());
}

const MathMLStyle* MathMLStyle::getMathMLStyle(RenderObject* renderer)
{
    // FIXME: Should we make RenderMathMLTable derive from RenderMathMLBlock in order to simplify this?
    if (is<RenderMathMLTable>(renderer))
        return &downcast<RenderMathMLTable>(*renderer).mathMLStyle();
    if (is<RenderMathMLBlock>(renderer))
        return &downcast<RenderMathMLBlock>(*renderer).mathMLStyle();
    return nullptr;
}

void MathMLStyle::resolveMathMLStyleTree(RenderObject* renderer)
{
    for (auto* child = renderer; child; child = child->nextInPreOrder(renderer)) {
        // FIXME: Should we make RenderMathMLTable derive from RenderMathMLBlock in order to simplify this?
        if (is<RenderMathMLTable>(child))
            downcast<RenderMathMLTable>(*child).mathMLStyle().resolveMathMLStyle(child);
        else if (is<RenderMathMLBlock>(child))
            downcast<RenderMathMLBlock>(*child).mathMLStyle().resolveMathMLStyle(child);
    }
}

RenderObject* MathMLStyle::getMathMLParentNode(RenderObject* renderer)
{
    auto* parentRenderer = renderer->parent();

    while (parentRenderer && !(is<RenderMathMLTable>(parentRenderer) || is<RenderMathMLBlock>(parentRenderer)))
        parentRenderer = parentRenderer->parent();

    return parentRenderer;
}

void MathMLStyle::updateStyleIfNeeded(RenderObject* renderer, MathMLElement::MathVariant oldMathVariant)
{
    // RenderMathMLFencedOperator does not support mathvariant transforms.
    // See https://bugs.webkit.org/show_bug.cgi?id=160509#c1.
    bool isNonAnonymousTokenElement = is<RenderMathMLToken>(renderer) && !renderer->isAnonymous();

    if (oldMathVariant != m_mathVariant) {
        if (isNonAnonymousTokenElement)
            downcast<RenderMathMLToken>(renderer)->updateTokenContent();
    }
}

void MathMLStyle::resolveMathMLStyle(RenderObject* renderer)
{
    ASSERT(renderer);

    MathMLElement::MathVariant oldMathVariant = m_mathVariant;
    auto* parentRenderer = getMathMLParentNode(renderer);
    const MathMLStyle* parentStyle = getMathMLStyle(parentRenderer);

    // By default, we just inherit the style from our parent.
    m_mathVariant = MathMLElement::MathVariant::None;
    if (parentStyle) {
        setMathVariant(parentStyle->mathVariant());
    }

    // Early return for anonymous renderers.
    if (renderer->isAnonymous()) {
        updateStyleIfNeeded(renderer, oldMathVariant);
        return;
    }

    // The mathvariant attributes override the default behavior.
    auto* element = downcast<RenderElement>(renderer)->element();
    if (is<MathMLElement>(element)) {
        Optional<MathMLElement::MathVariant> mathVariant = downcast<MathMLElement>(element)->specifiedMathVariant();
        if (mathVariant)
            m_mathVariant = mathVariant.value();
    }
    updateStyleIfNeeded(renderer, oldMathVariant);
}

}

#endif // ENABLE(MATHML)
