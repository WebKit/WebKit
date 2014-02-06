/*
 * Copyright (C) 2014 Frédéric Wang (fred.wang@free.fr). All rights reserved.
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
#include "RenderMathMLToken.h"

#if ENABLE(MATHML)

#include "MathMLNames.h"

namespace WebCore {

using namespace MathMLNames;

RenderMathMLToken::RenderMathMLToken(Element& element, PassRef<RenderStyle> style)
    : RenderMathMLBlock(element, std::move(style))
{
}

RenderMathMLToken::RenderMathMLToken(Document& document, PassRef<RenderStyle> style)
    : RenderMathMLBlock(document, std::move(style))
{
}

void RenderMathMLToken::addChild(RenderObject* newChild, RenderObject* beforeChild)
{
    createWrapperIfNeeded();
    toRenderElement(firstChild())->addChild(newChild, beforeChild);
}

void RenderMathMLToken::createWrapperIfNeeded()
{
    if (!firstChild()) {
        RenderPtr<RenderMathMLBlock> wrapper = createAnonymousMathMLBlock();
        // This container doesn't offer any useful information to accessibility.
        wrapper->setIgnoreInAccessibilityTree(true);
        RenderMathMLBlock::addChild(wrapper.leakPtr());
    }
}

void RenderMathMLToken::updateTokenContent()
{
    if (!isEmpty())
        updateStyle();
    setNeedsLayoutAndPrefWidthsRecalc();
}

void RenderMathMLToken::updateStyle()
{
    const auto& tokenElement = element();

    // This tries to emulate the default mathvariant value on <mi> using the CSS font-style property.
    // FIXME: This should be revised when mathvariant is implemented (http://wkbug/85735) and when fonts with Mathematical Alphanumeric Symbols characters are more popular.
    const auto& wrapper = toRenderElement(firstChild());
    auto newStyle = RenderStyle::createAnonymousStyleWithDisplay(&style(), FLEX);
    FontDescription fontDescription(newStyle.get().fontDescription());
    FontSelector* fontSelector = newStyle.get().font().fontSelector();
    if (element().textContent().stripWhiteSpace().simplifyWhiteSpace().length() == 1 && !tokenElement.hasAttribute(mathvariantAttr))
        fontDescription.setItalic(true);
    if (newStyle.get().setFontDescription(fontDescription))
        newStyle.get().font().update(fontSelector);
    wrapper->setStyle(std::move(newStyle));
    wrapper->setNeedsLayoutAndPrefWidthsRecalc();
}

void RenderMathMLToken::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderMathMLBlock::styleDidChange(diff, oldStyle);
    if (!isEmpty())
        updateStyle();
}

void RenderMathMLToken::updateFromElement()
{
    RenderMathMLBlock::updateFromElement();
    if (!isEmpty())
        updateStyle();
}

}

#endif // ENABLE(MATHML)
