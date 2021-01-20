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
 *
 */

#include "config.h"
#include "MathMLUnderOverElement.h"

#if ENABLE(MATHML)

#include "RenderMathMLUnderOver.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(MathMLUnderOverElement);

using namespace MathMLNames;

inline MathMLUnderOverElement::MathMLUnderOverElement(const QualifiedName& tagName, Document& document)
    : MathMLScriptsElement(tagName, document)
{
}

Ref<MathMLUnderOverElement> MathMLUnderOverElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new MathMLUnderOverElement(tagName, document));
}

const MathMLElement::BooleanValue& MathMLUnderOverElement::accent()
{
    return cachedBooleanAttribute(accentAttr, m_accent);
}

const MathMLElement::BooleanValue& MathMLUnderOverElement::accentUnder()
{
    return cachedBooleanAttribute(accentunderAttr, m_accentUnder);
}

void MathMLUnderOverElement::parseAttribute(const QualifiedName& name, const AtomString& value)
{
    if (name == accentAttr)
        m_accent = WTF::nullopt;
    else if (name == accentunderAttr)
        m_accentUnder = WTF::nullopt;

    MathMLElement::parseAttribute(name, value);
}

RenderPtr<RenderElement> MathMLUnderOverElement::createElementRenderer(RenderStyle&& style, const RenderTreePosition&)
{
    ASSERT(hasTagName(munderTag) || hasTagName(moverTag) || hasTagName(munderoverTag));
    return createRenderer<RenderMathMLUnderOver>(*this, WTFMove(style));
}

}

#endif // ENABLE(MATHML)
