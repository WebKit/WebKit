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

#pragma once

#if ENABLE(MATHML)

#include "MathMLOperatorDictionary.h"
#include "MathMLOperatorElement.h"
#include "RenderMathMLOperator.h"

namespace WebCore {

class RenderMathMLFencedOperator final : public RenderMathMLOperator {
    WTF_MAKE_ISO_ALLOCATED(RenderMathMLFencedOperator);
public:
    RenderMathMLFencedOperator(Document&, RenderStyle&&, const String& operatorString, MathMLOperatorDictionary::Form, unsigned short flags = 0);
    void updateOperatorContent(const String&);

private:
    bool isRenderMathMLFencedOperator() const final { return true; }
    bool isVertical() const final { return m_operatorChar.isVertical; }
    UChar32 textContent() const final { return m_operatorChar.character; }
    LayoutUnit leadingSpace() const final;
    LayoutUnit trailingSpace() const final;

    // minsize always has the default value "1em".
    LayoutUnit minSize() const final { return LayoutUnit(style().fontCascade().size()); }

    // maxsize always has the default value "infinity".
    LayoutUnit maxSize() const final { return intMaxForLayoutUnit; }

    bool hasOperatorFlag(MathMLOperatorDictionary::Flag flag) const final { return m_operatorFlags & flag; }

    // We always use the MathOperator class for anonymous mfenced operators, since they do not have text content in the DOM.
    bool useMathOperator() const final { return true; }

    MathMLOperatorElement::OperatorChar m_operatorChar;
    unsigned short m_leadingSpaceInMathUnit;
    unsigned short m_trailingSpaceInMathUnit;
    MathMLOperatorDictionary::Form m_operatorForm;
    unsigned short m_operatorFlags;
};

}; // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderMathMLFencedOperator, isRenderMathMLFencedOperator())

#endif // ENABLE(MATHML)
