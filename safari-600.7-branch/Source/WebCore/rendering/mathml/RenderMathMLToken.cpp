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
#include "RenderElement.h"
#include "RenderIterator.h"

namespace WebCore {

using namespace MathMLNames;

RenderMathMLToken::RenderMathMLToken(Element& element, PassRef<RenderStyle> style)
    : RenderMathMLBlock(element, WTF::move(style))
    , m_containsElement(false)
{
}

RenderMathMLToken::RenderMathMLToken(Document& document, PassRef<RenderStyle> style)
    : RenderMathMLBlock(document, WTF::move(style))
    , m_containsElement(false)
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
        RenderMathMLBlock::addChild(wrapper.leakPtr());
    }
}

void RenderMathMLToken::updateTokenContent()
{
    m_containsElement = false;
    if (!isEmpty()) {
        // The renderers corresponding to the children of the token element are wrapped inside an anonymous RenderMathMLBlock.
        // When one of these renderers is a RenderElement, we handle the RenderMathMLToken differently.
        // For some reason, an additional anonymous RenderBlock is created as a child of the RenderMathMLToken and the renderers are actually inserted into that RenderBlock so we need to dig down one additional level here.
        const auto& wrapper = toRenderElement(firstChild());
        if (const auto& block = toRenderElement(wrapper->firstChild()))
            m_containsElement = childrenOfType<RenderElement>(*block).first();
        updateStyle();
    }
    setNeedsLayoutAndPrefWidthsRecalc();
}

void RenderMathMLToken::updateStyle()
{
    const auto& tokenElement = element();

    const auto& wrapper = toRenderElement(firstChild());
    auto newStyle = RenderStyle::createAnonymousStyleWithDisplay(&style(), FLEX);

    if (tokenElement.hasTagName(MathMLNames::miTag)) {
        // This tries to emulate the default mathvariant value on <mi> using the CSS font-style property.
        // FIXME: This should be revised when mathvariant is implemented (http://wkbug/85735) and when fonts with Mathematical Alphanumeric Symbols characters are more popular.
        FontDescription fontDescription(newStyle.get().fontDescription());
        FontSelector* fontSelector = newStyle.get().font().fontSelector();
        if (!m_containsElement && element().textContent().stripWhiteSpace().simplifyWhiteSpace().length() == 1 && !tokenElement.hasAttribute(mathvariantAttr))
            fontDescription.setItalic(true);
        if (newStyle.get().setFontDescription(fontDescription))
            newStyle.get().font().update(fontSelector);
    }

    wrapper->setStyle(WTF::move(newStyle));
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
