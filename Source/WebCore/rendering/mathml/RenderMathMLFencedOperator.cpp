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

#include "RenderMathMLFencedOperator.h"

#include "MathMLOperatorDictionary.h"
#include "MathMLOperatorElement.h"

namespace WebCore {

using namespace MathMLOperatorDictionary;

RenderMathMLFencedOperator::RenderMathMLFencedOperator(Document& document, RenderStyle&& style, const String& operatorString, MathMLOperatorDictionary::Form form, unsigned short flags)
    : RenderMathMLOperator(document, WTFMove(style), flags)
    , m_operatorForm(form)
{
    updateOperatorContent(operatorString);

    // maxsize always has the default value "infinity" value for mfenced operators.
    m_maxSize = intMaxForLayoutUnit;
}

void RenderMathMLFencedOperator::updateOperatorContent(const String& operatorString)
{
    m_textContent = MathMLOperatorElement::parseOperatorText(operatorString);
    rebuildTokenContent();
}

void RenderMathMLFencedOperator::setOperatorProperties()
{
    // We determine the stretch direction (default is vertical).
    m_isVertical = MathMLOperatorDictionary::isVertical(m_textContent);

    // Resets all but the Fence and Separator properties.
    m_operatorFlags &= MathMLOperatorDictionary::Fence | MathMLOperatorDictionary::Separator;

    // minsize always has the default value "1em" value for mfenced operators.
    m_minSize = style().fontCascade().size();

    // Default spacing is thickmathspace.
    MathMLElement::Length leadingSpace;
    leadingSpace.type = MathMLElement::LengthType::MathUnit;
    leadingSpace.value = 5;
    MathMLElement::Length trailingSpace = leadingSpace;

    if (auto entry = search(m_textContent, m_operatorForm, true)) {
        // We use the space specified in the operator dictionary.
        leadingSpace.value = static_cast<float>(entry.value().lspace);
        trailingSpace.value = static_cast<float>(entry.value().rspace);

        // We use the dictionary but preserve the Fence and Separator properties.
        m_operatorFlags = (m_operatorFlags & (MathMLOperatorDictionary::Fence | MathMLOperatorDictionary::Separator)) | entry.value().flags;
    }

    // Resolve the leading and trailing spaces.
    m_leadingSpace = toUserUnits(leadingSpace, style(), 0);
    m_trailingSpace = toUserUnits(trailingSpace, style(), 0);
}

}

#endif
