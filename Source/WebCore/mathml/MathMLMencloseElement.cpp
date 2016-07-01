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

#if ENABLE(MATHML)
#include "MathMLMencloseElement.h"

#include "MathMLNames.h"
#include "RenderMathMLMenclose.h"

namespace WebCore {

MathMLMencloseElement::MathMLMencloseElement(const QualifiedName& tagName, Document& document)
    : MathMLInlineContainerElement(tagName, document)
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

void MathMLMencloseElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (name == MathMLNames::notationAttr) {
        clearNotations();
        if (!hasAttribute(name)) {
            addNotation(LongDiv); // default value is longdiv
            return;
        }
        Vector<String> notationsList;
        String(value).split(' ', notationsList);
        for (auto& notation : notationsList) {
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
        return;
    }

    MathMLInlineContainerElement::parseAttribute(name, value);
}

}
#endif // ENABLE(MATHML)
