/*
 * Copyright (C) 2016 Igalia, S.L.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
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

#include "AccessibilityRenderObject.h"
#include "RenderMathMLBlock.h"
#include "RenderMathMLFraction.h"
#include "RenderMathMLMath.h"
#include "RenderMathMLOperator.h"
#include "RenderMathMLRoot.h"

namespace WebCore {

class AccessibilityMathMLElement : public AccessibilityRenderObject {

public:
    static Ref<AccessibilityMathMLElement> create(RenderObject*, bool isAnonymousOperator);
    virtual ~AccessibilityMathMLElement();

protected:
    explicit AccessibilityMathMLElement(RenderObject*, bool isAnonymousOperator);

private:
    AccessibilityRole determineAccessibilityRole() final;
    String textUnderElement(AccessibilityTextUnderElementMode = AccessibilityTextUnderElementMode()) const override;
    String stringValue() const override;
    bool isIgnoredElementWithinMathTree() const final;

    bool isMathElement() const final { return true; }

    bool isMathFraction() const override;
    bool isMathFenced() const override;
    bool isMathSubscriptSuperscript() const override;
    bool isMathRow() const override;
    bool isMathUnderOver() const override;
    bool isMathRoot() const override;
    bool isMathSquareRoot() const override;
    bool isMathText() const override;
    bool isMathNumber() const override;
    bool isMathOperator() const override;
    bool isMathFenceOperator() const override;
    bool isMathSeparatorOperator() const override;
    bool isMathIdentifier() const override;
    bool isMathTable() const override;
    bool isMathTableRow() const override;
    bool isMathTableCell() const override;
    bool isMathMultiscript() const override;
    bool isMathToken() const override;
    bool isMathScriptObject(AccessibilityMathScriptObjectType) const override;
    bool isMathMultiscriptObject(AccessibilityMathMultiscriptObjectType) const override;

    // Generic components.
    AccessibilityObject* mathBaseObject() override;

    // Root components.
    AccessibilityObject* mathRadicandObject() override;
    AccessibilityObject* mathRootIndexObject() override;

    // Fraction components.
    AccessibilityObject* mathNumeratorObject() override;
    AccessibilityObject* mathDenominatorObject() override;

    // Under over components.
    AccessibilityObject* mathUnderObject() override;
    AccessibilityObject* mathOverObject() override;

    // Subscript/superscript components.
    AccessibilityObject* mathSubscriptObject() override;
    AccessibilityObject* mathSuperscriptObject() override;

    // Fenced components.
    String mathFencedOpenString() const override;
    String mathFencedCloseString() const override;
    int mathLineThickness() const override;
    bool isAnonymousMathOperator() const override;

    // Multiscripts components.
    void mathPrescripts(AccessibilityMathMultiscriptPairs&) override;
    void mathPostscripts(AccessibilityMathMultiscriptPairs&) override;

    bool m_isAnonymousOperator;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_ACCESSIBILITY(AccessibilityMathMLElement, isMathElement())

#endif // ENABLE(MATHML)
