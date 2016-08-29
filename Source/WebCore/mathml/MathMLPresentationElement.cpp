/*
 * Copyright (C) 2009 Alex Milowski (alex@milowski.com). All rights reserved.
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2016 Igalia S.L.
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
#include "MathMLPresentationElement.h"

#if ENABLE(MATHML)

#include "MathMLNames.h"
#include "RenderMathMLBlock.h"

namespace WebCore {

using namespace MathMLNames;

MathMLPresentationElement::MathMLPresentationElement(const QualifiedName& tagName, Document& document)
    : MathMLElement(tagName, document)
{
}

Ref<MathMLPresentationElement> MathMLPresentationElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new MathMLPresentationElement(tagName, document));
}

RenderPtr<RenderElement> MathMLPresentationElement::createElementRenderer(RenderStyle&& style, const RenderTreePosition& insertionPosition)
{
    if (hasTagName(mtableTag))
        return createRenderer<RenderMathMLTable>(*this, WTFMove(style));

    return MathMLElement::createElementRenderer(WTFMove(style), insertionPosition);
}

bool MathMLPresentationElement::acceptsDisplayStyleAttribute()
{
    return hasTagName(mtableTag);
}

void MathMLPresentationElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    bool displayStyleAttribute = name == displaystyleAttr && acceptsDisplayStyleAttribute();
    bool mathVariantAttribute = name == mathvariantAttr && acceptsMathVariantAttribute();
    if (displayStyleAttribute)
        m_displayStyle = Nullopt;
    if (mathVariantAttribute)
        m_mathVariant = Nullopt;
    if ((displayStyleAttribute || mathVariantAttribute) && renderer())
        MathMLStyle::resolveMathMLStyleTree(renderer());

    MathMLElement::parseAttribute(name, value);
}

}

#endif // ENABLE(MATHML)
