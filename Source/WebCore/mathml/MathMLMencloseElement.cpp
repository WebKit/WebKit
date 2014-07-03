/*
 * Copyright (C) 2014 Gurpreet Kaur (k.gurpreet@samsung.com). All rights reserved.
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
#include "RenderElement.h"
#include "RenderMathMLMenclose.h"
#include "RenderObject.h"
#include "TextRun.h"
#include <wtf/Ref.h>
#include <wtf/text/StringBuilder.h>


namespace WebCore {

MathMLMencloseElement::MathMLMencloseElement(const QualifiedName& tagName, Document& document)
    : MathMLInlineContainerElement(tagName, document)
    , m_isRadicalValue(false)
{
}

PassRefPtr<MathMLMencloseElement> MathMLMencloseElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(new MathMLMencloseElement(tagName, document));
}

RenderPtr<RenderElement> MathMLMencloseElement::createElementRenderer(PassRef<RenderStyle> style)
{    
    return createRenderer<RenderMathMLMenclose>(*this, WTF::move(style));
}

bool MathMLMencloseElement::isPresentationAttribute(const QualifiedName& name) const
{
    if (name == MathMLNames::notationAttr)
        return true;
    return MathMLElement::isPresentationAttribute(name);
}

void MathMLMencloseElement::finishParsingChildren()
{
    MathMLInlineContainerElement::finishParsingChildren();
    // When notation value is a radical and menclose does not have any child 
    // then we add anonymous squareroot child to menclose so that square root
    // symbol can be rendered.
    if (m_isRadicalValue && !firstElementChild())
        renderer()->addChild(nullptr, nullptr);
}

void MathMLMencloseElement::collectStyleForPresentationAttribute(const QualifiedName& name, const AtomicString& value, MutableStyleProperties& style)
{
    String val = value;
    if (val.isEmpty())
        return;
    if (name == MathMLNames::notationAttr) {
        val.split(" ", m_notationValues);
        size_t notationValueSize = m_notationValues.size();
        for (size_t i = 0; i < notationValueSize; i++) {
            if (m_notationValues[i] == "top" || m_notationValues[i] == "longdiv") {
                if (m_notationValues[i] == "longdiv")
                    addPropertyToPresentationAttributeStyle(style, CSSPropertyPaddingLeft, longDivLeftPadding());
                addPropertyToPresentationAttributeStyle(style, CSSPropertyBorderTopStyle, "solid");
                addPropertyToPresentationAttributeStyle(style, CSSPropertyBorderTopWidth, "thin");
                addPropertyToPresentationAttributeStyle(style, CSSPropertyPaddingTop, ".3ex");
            } else if (m_notationValues[i] == "bottom") {
                addPropertyToPresentationAttributeStyle(style, CSSPropertyBorderBottomStyle, "solid");
                addPropertyToPresentationAttributeStyle(style, CSSPropertyBorderBottomWidth, "thin");
                addPropertyToPresentationAttributeStyle(style, CSSPropertyPaddingBottom, ".3ex");
            } else if (m_notationValues[i] == "left") {
                addPropertyToPresentationAttributeStyle(style, CSSPropertyBorderLeftStyle, "solid");
                addPropertyToPresentationAttributeStyle(style, CSSPropertyBorderLeftWidth, "thin");
                addPropertyToPresentationAttributeStyle(style, CSSPropertyPaddingLeft, ".3ex");
            } else if (m_notationValues[i] == "right") {
                addPropertyToPresentationAttributeStyle(style, CSSPropertyBorderRightStyle, "solid");
                addPropertyToPresentationAttributeStyle(style, CSSPropertyBorderRightWidth, "thin");
                addPropertyToPresentationAttributeStyle(style, CSSPropertyPaddingRight, ".3ex");
            } else if (m_notationValues[i] == "box" || m_notationValues[i] == "roundedbox") {
                addPropertyToPresentationAttributeStyle(style, CSSPropertyBorderStyle, "solid");
                addPropertyToPresentationAttributeStyle(style, CSSPropertyBorderWidth, "thin");
                addPropertyToPresentationAttributeStyle(style, CSSPropertyPadding, ".3ex");
                if (m_notationValues[i] == "roundedbox")
                    addPropertyToPresentationAttributeStyle(style, CSSPropertyBorderRadius, ASCIILiteral("5px"));
            } else if (m_notationValues[i] == "actuarial" || m_notationValues[i] == "madruwb") {
                addPropertyToPresentationAttributeStyle(style, CSSPropertyBorderRightStyle, "solid");
                addPropertyToPresentationAttributeStyle(style, CSSPropertyBorderRightWidth, "thin");
                addPropertyToPresentationAttributeStyle(style, CSSPropertyPaddingRight, ".3ex");
                if (m_notationValues[i] == "actuarial") {
                    addPropertyToPresentationAttributeStyle(style, CSSPropertyBorderTopStyle, "solid");
                    addPropertyToPresentationAttributeStyle(style, CSSPropertyBorderTopWidth, "thin");
                    addPropertyToPresentationAttributeStyle(style, CSSPropertyPaddingTop, ".3ex");
                } else if (m_notationValues[i] == "madruwb") {
                    addPropertyToPresentationAttributeStyle(style, CSSPropertyBorderBottomStyle, "solid");
                    addPropertyToPresentationAttributeStyle(style, CSSPropertyBorderBottomWidth, "thin");
                    addPropertyToPresentationAttributeStyle(style, CSSPropertyPaddingBottom, ".3ex");
                }
            } else if (m_notationValues[i] == "radical")
                m_isRadicalValue = true;
        }
    } else
        MathMLInlineContainerElement::collectStyleForPresentationAttribute(name, value, style);
}


String MathMLMencloseElement::longDivLeftPadding() const
{
    StringBuilder padding;
    float fontSize = 0;
    String closingBrace = ")";
    TextRun run(closingBrace.impl(), closingBrace.length());
    Node* node = parentNode();
    if (node && node->renderer()) {
        const Font& font = node->renderer()->style().font();
        fontSize = font.width(run);
        padding.append(String::number(fontSize));
        padding.append("px");
    }
    return padding.toString();
}

}
#endif // ENABLE(MATHML)
