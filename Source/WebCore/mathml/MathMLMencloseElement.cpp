/*
 * Copyright (C) 2014 Gurpreet Kaur (k.gurpreet@samsung.com). All rights reserved.
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
#include "MathMLMencloseElement.h"

#if ENABLE(MATHML)

#include "HTMLParserIdioms.h"
#include "MathMLNames.h"
#include "RenderMathMLMenclose.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(MathMLMencloseElement);

using namespace MathMLNames;

MathMLMencloseElement::MathMLMencloseElement(const QualifiedName& tagName, Document& document)
    : MathMLRowElement(tagName, document)
{
    // By default we draw a longdiv.
    clearNotations();
    addNotation(LongDiv);
}

Ref<MathMLMencloseElement> MathMLMencloseElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new MathMLMencloseElement(tagName, document));
}

RenderPtr<RenderElement> MathMLMencloseElement::createElementRenderer(RenderStyle&& style, const RenderTreePosition&)
{
    return createRenderer<RenderMathMLMenclose>(*this, WTFMove(style));
}

void MathMLMencloseElement::addNotationFlags(StringView notation)
{
    ASSERT(m_notationFlags);
    if (notation == "longdiv") {
        addNotation(LongDiv);
    } else if (notation == "roundedbox") {
        addNotation(RoundedBox);
    } else if (notation == "circle") {
        addNotation(Circle);
    } else if (notation == "left") {
        addNotation(Left);
    } else if (notation == "right") {
        addNotation(Right);
    } else if (notation == "top") {
        addNotation(Top);
    } else if (notation == "bottom") {
        addNotation(Bottom);
    } else if (notation == "updiagonalstrike") {
        addNotation(UpDiagonalStrike);
    } else if (notation == "downdiagonalstrike") {
        addNotation(DownDiagonalStrike);
    } else if (notation == "verticalstrike") {
        addNotation(VerticalStrike);
    } else if (notation == "horizontalstrike") {
        addNotation(HorizontalStrike);
    } else if (notation == "updiagonalarrow") {
        addNotation(UpDiagonalArrow);
    } else if (notation == "phasorangle") {
        addNotation(PhasorAngle);
    } else if (notation == "box") {
        addNotation(Left);
        addNotation(Right);
        addNotation(Top);
        addNotation(Bottom);
    } else if (notation == "actuarial") {
        addNotation(Right);
        addNotation(Top);
    } else if (notation == "madruwb") {
        addNotation(Right);
        addNotation(Bottom);
    }
}

void MathMLMencloseElement::parseNotationAttribute()
{
    clearNotations();
    if (!hasAttribute(notationAttr)) {
        addNotation(LongDiv); // The default value is longdiv.
        return;
    }
    // We parse the list of whitespace-separated notation names.
    StringView value = attributeWithoutSynchronization(notationAttr).string();
    unsigned length = value.length();
    unsigned start = 0;
    while (start < length) {
        if (isHTMLSpace(value[start])) {
            start++;
            continue;
        }
        unsigned end = start + 1;
        while (end < length && !isHTMLSpace(value[end]))
            end++;
        addNotationFlags(value.substring(start, end - start));
        start = end;
    }
}

bool MathMLMencloseElement::hasNotation(MencloseNotationFlag notationFlag)
{
    if (!m_notationFlags)
        parseNotationAttribute();
    return m_notationFlags.value() & notationFlag;
}

void MathMLMencloseElement::parseAttribute(const QualifiedName& name, const AtomString& value)
{
    if (name == notationAttr)
        m_notationFlags = WTF::nullopt;

    MathMLRowElement::parseAttribute(name, value);
}

}
#endif // ENABLE(MATHML)
