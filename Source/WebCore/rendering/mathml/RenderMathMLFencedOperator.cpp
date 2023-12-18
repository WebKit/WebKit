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
#include "RenderMathMLFencedOperator.h"

#if ENABLE(MATHML)

#include "MathMLOperatorDictionary.h"
#include "MathMLOperatorElement.h"
#include "RenderBoxInlines.h"
#include "RenderBoxModelObjectInlines.h"
#include "RenderStyleInlines.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

using namespace MathMLOperatorDictionary;

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderMathMLFencedOperator);

RenderMathMLFencedOperator::RenderMathMLFencedOperator(Document& document, RenderStyle&& style, const String& operatorString, MathMLOperatorDictionary::Form form, unsigned short flags)
    : RenderMathMLOperator(Type::MathMLFencedOperator, document, WTFMove(style))
    , m_operatorForm(form)
    , m_operatorFlags(flags)
{
    updateOperatorContent(operatorString);
    ASSERT(isRenderMathMLFencedOperator());
}

void RenderMathMLFencedOperator::updateOperatorContent(const String& operatorString)
{
    m_operatorChar = MathMLOperatorElement::parseOperatorChar(operatorString);

    // We try and read spacing and boolean properties from the operator dictionary.
    // However we preserve the Fence and Separator properties specified in the constructor.
    if (auto entry = search(m_operatorChar.character, m_operatorForm, true)) {
        m_leadingSpaceInMathUnit = entry.value().leadingSpaceInMathUnit;
        m_trailingSpaceInMathUnit = entry.value().trailingSpaceInMathUnit;
        m_operatorFlags = (m_operatorFlags & (MathMLOperatorDictionary::Fence | MathMLOperatorDictionary::Separator)) | entry.value().flags;
    } else {
        m_operatorFlags &= MathMLOperatorDictionary::Fence | MathMLOperatorDictionary::Separator; // Flags are disabled by default.
        m_leadingSpaceInMathUnit = 5; // Default spacing is thickmathspace.
        m_trailingSpaceInMathUnit = 5; // Default spacing is thickmathspace.
    }

    updateMathOperator();
}

LayoutUnit RenderMathMLFencedOperator::leadingSpace() const
{
    MathMLElement::Length leadingSpace;
    leadingSpace.type = MathMLElement::LengthType::MathUnit;
    leadingSpace.value = static_cast<float>(m_leadingSpaceInMathUnit);
    return toUserUnits(leadingSpace, style(), 0);
}

LayoutUnit RenderMathMLFencedOperator::trailingSpace() const
{
    MathMLElement::Length trailingSpace;
    trailingSpace.type = MathMLElement::LengthType::MathUnit;
    trailingSpace.value = static_cast<float>(m_trailingSpaceInMathUnit);
    return toUserUnits(trailingSpace, style(), 0);
}

}

#endif
