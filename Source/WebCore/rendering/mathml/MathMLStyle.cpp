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

#if ENABLE(MATHML)
#include "MathMLStyle.h"

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

void MathMLStyle::setDisplayStyle(RenderObject* renderer)
{
    if (!renderer)
        return;

    // FIXME: Should we make RenderMathMLTable derive from RenderMathMLBlock in order to simplify this?
    if (is<RenderMathMLTable>(renderer))
        m_displayStyle = downcast<RenderMathMLTable>(renderer)->mathMLStyle()->displayStyle();
    else if (is<RenderMathMLBlock>(renderer))
        m_displayStyle = downcast<RenderMathMLBlock>(renderer)->mathMLStyle()->displayStyle();
}

void MathMLStyle::resolveMathMLStyleTree(RenderObject* renderer)
{
    for (auto* child = renderer; child; child = child->nextInPreOrder(renderer)) {
        // FIXME: Should we make RenderMathMLTable derive from RenderMathMLBlock in order to simplify this?
        if (is<RenderMathMLTable>(child))
            downcast<RenderMathMLTable>(child)->mathMLStyle()->resolveMathMLStyle(child);
        else if (is<RenderMathMLBlock>(child))
            downcast<RenderMathMLBlock>(child)->mathMLStyle()->resolveMathMLStyle(child);
    }
}

RenderObject* MathMLStyle::getMathMLParentNode(RenderObject* renderer)
{
    auto* parentRenderer = renderer->parent();

    while (parentRenderer && !(is<RenderMathMLTable>(parentRenderer) || is<RenderMathMLBlock>(parentRenderer)))
        parentRenderer = parentRenderer->parent();

    return parentRenderer;
}

void MathMLStyle::updateStyleIfNeeded(RenderObject* renderer, bool oldDisplayStyle)
{
    if (oldDisplayStyle != m_displayStyle) {
        renderer->setPreferredLogicalWidthsDirty(true);
        if (is<RenderMathMLToken>(renderer))
            downcast<RenderMathMLToken>(renderer)->updateTokenContent();
        else if (is<RenderMathMLRoot>(renderer))
            downcast<RenderMathMLRoot>(renderer)->updateStyle();
    }
}

void MathMLStyle::resolveMathMLStyle(RenderObject* renderer)
{
    ASSERT(renderer);

    bool oldDisplayStyle = m_displayStyle;

    // For anonymous renderers, we just inherit the style from our parent.
    if (renderer->isAnonymous()) {
        setDisplayStyle(getMathMLParentNode(renderer));
        updateStyleIfNeeded(renderer, oldDisplayStyle);
        return;
    }

    if (is<RenderMathMLMath>(renderer))
        m_displayStyle = downcast<RenderElement>(renderer)->element()->fastGetAttribute(displayAttr) == "block"; // The default displaystyle of the <math> element depends on its display attribute.
    else if (is<RenderMathMLTable>(renderer))
        m_displayStyle = false; // The default displaystyle of <mtable> is false.
    else if (auto* parentRenderer = getMathMLParentNode(renderer)) {
        setDisplayStyle(parentRenderer); // The default displaystyle is inherited from our parent.
        if (is<RenderMathMLFraction>(parentRenderer))
            m_displayStyle = false; // <mfrac> sets displaystyle to false within its numerator and denominator.
        else if ((is<RenderMathMLRoot>(parentRenderer) && !parentRenderer->isRenderMathMLSquareRoot()) || is<RenderMathMLScripts>(parentRenderer) || is<RenderMathMLUnderOver>(parentRenderer)) {
            // <mroot>, <msub>, <msup>, <msubsup>, <mmultiscripts>, <munder>, <mover> and <munderover> elements set displaystyle to false within their scripts.
            auto* base = downcast<RenderBox>(parentRenderer)->firstChildBox();
            if (renderer != base)
                m_displayStyle = false;
        }
    }

    // The displaystyle attribute on the <math>, <mtable> or <mstyle> elements override the default behavior.
    const auto* element = downcast<RenderElement>(renderer)->element();
    const QualifiedName& tagName = element->tagQName();
    if (tagName == mathTag || tagName == mtableTag || tagName == mstyleTag) {
        // We only modify the value of displaystyle if there is an explicit and valid attribute.
        const AtomicString& attributeValue = element->fastGetAttribute(displaystyleAttr);
        if (attributeValue == "true")
            m_displayStyle = true;
        else if (attributeValue == "false")
            m_displayStyle = false;
    }

    updateStyleIfNeeded(renderer, oldDisplayStyle);
}

}

#endif // ENABLE(MATHML)
