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
#include "MathMLMathElement.h"

#if ENABLE(MATHML)

#include "MathMLNames.h"
#include "RenderMathMLMath.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(MathMLMathElement);

using namespace MathMLNames;

inline MathMLMathElement::MathMLMathElement(const QualifiedName& tagName, Document& document)
    : MathMLRowElement(tagName, document)
{
    setHasCustomStyleResolveCallbacks();
}

Ref<MathMLMathElement> MathMLMathElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new MathMLMathElement(tagName, document));
}

RenderPtr<RenderElement> MathMLMathElement::createElementRenderer(RenderStyle&& style, const RenderTreePosition&)
{
    return createRenderer<RenderMathMLMath>(*this, WTFMove(style));
}

Optional<bool> MathMLMathElement::specifiedDisplayStyle()
{
    if (cachedBooleanAttribute(displaystyleAttr, m_displayStyle) == BooleanValue::Default) {
        // The default displaystyle value of the <math> depends on the display attribute, so we parse it here.
        auto& value = attributeWithoutSynchronization(displayAttr);
        if (value == "block")
            m_displayStyle = BooleanValue::True;
        else if (value == "inline")
            m_displayStyle = BooleanValue::False;
    }
    return toOptionalBool(m_displayStyle.value());
}

void MathMLMathElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    bool displayStyleAttribute = (name == displaystyleAttr || name == displayAttr);
    bool mathVariantAttribute = name == mathvariantAttr;
    if (displayStyleAttribute)
        m_displayStyle = WTF::nullopt;
    if (mathVariantAttribute)
        m_mathVariant = WTF::nullopt;
    if ((displayStyleAttribute || mathVariantAttribute) && renderer())
        MathMLStyle::resolveMathMLStyleTree(renderer());

    MathMLElement::parseAttribute(name, value);
}

void MathMLMathElement::didAttachRenderers()
{
    MathMLRowElement::didAttachRenderers();

    MathMLStyle::resolveMathMLStyleTree(renderer());
}

}

#endif // ENABLE(MATHML)
