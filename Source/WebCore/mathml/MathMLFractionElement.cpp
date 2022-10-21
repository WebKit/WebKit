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
#include "MathMLFractionElement.h"

#if ENABLE(MATHML)

#include "ElementInlines.h"
#include "RenderMathMLFraction.h"
#include "Settings.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(MathMLFractionElement);

using namespace MathMLNames;

inline MathMLFractionElement::MathMLFractionElement(const QualifiedName& tagName, Document& document)
    : MathMLPresentationElement(tagName, document)
{
}

Ref<MathMLFractionElement> MathMLFractionElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new MathMLFractionElement(tagName, document));
}

const MathMLElement::Length& MathMLFractionElement::lineThickness()
{
    if (m_lineThickness)
        return m_lineThickness.value();

    auto& thickness = attributeWithoutSynchronization(linethicknessAttr);
    if (document().settings().coreMathMLEnabled()) {
        m_lineThickness = parseMathMLLength(thickness, false);
        return m_lineThickness.value();
    }

    // The MathML3 recommendation states that "medium" is the default thickness.
    // However, it only states that "thin" and "thick" are respectively thiner and thicker.
    // The MathML in HTML5 implementation note suggests 50% and 200% and these values are also used in Gecko.
    m_lineThickness = Length();
    if (equalLettersIgnoringASCIICase(thickness, "thin"_s)) {
        m_lineThickness.value().type = LengthType::UnitLess;
        m_lineThickness.value().value = .5;
    } else if (equalLettersIgnoringASCIICase(thickness, "medium"_s)) {
        m_lineThickness.value().type = LengthType::UnitLess;
        m_lineThickness.value().value = 1;
    } else if (equalLettersIgnoringASCIICase(thickness, "thick"_s)) {
        m_lineThickness.value().type = LengthType::UnitLess;
        m_lineThickness.value().value = 2;
    } else
        m_lineThickness = parseMathMLLength(thickness, true);
    return m_lineThickness.value();
}

MathMLFractionElement::FractionAlignment MathMLFractionElement::cachedFractionAlignment(const QualifiedName& name, std::optional<FractionAlignment>& alignment)
{
    if (alignment)
        return alignment.value();

    if (document().settings().coreMathMLEnabled()) {
        alignment = FractionAlignmentCenter;
        return alignment.value();
    }

    auto& value = attributeWithoutSynchronization(name);
    if (equalLettersIgnoringASCIICase(value, "left"_s))
        alignment = FractionAlignmentLeft;
    else if (equalLettersIgnoringASCIICase(value, "right"_s))
        alignment = FractionAlignmentRight;
    else
        alignment = FractionAlignmentCenter;
    return alignment.value();
}

MathMLFractionElement::FractionAlignment MathMLFractionElement::numeratorAlignment()
{
    return cachedFractionAlignment(numalignAttr, m_numeratorAlignment);
}

MathMLFractionElement::FractionAlignment MathMLFractionElement::denominatorAlignment()
{
    return cachedFractionAlignment(denomalignAttr, m_denominatorAlignment);
}

void MathMLFractionElement::parseAttribute(const QualifiedName& name, const AtomString& value)
{
    if (name == linethicknessAttr)
        m_lineThickness = std::nullopt;
    else if (name == numalignAttr)
        m_numeratorAlignment = std::nullopt;
    else if (name == denomalignAttr)
        m_denominatorAlignment = std::nullopt;

    MathMLElement::parseAttribute(name, value);
}

RenderPtr<RenderElement> MathMLFractionElement::createElementRenderer(RenderStyle&& style, const RenderTreePosition&)
{
    ASSERT(hasTagName(MathMLNames::mfracTag));
    return createRenderer<RenderMathMLFraction>(*this, WTFMove(style));
}

}

#endif // ENABLE(MATHML)
