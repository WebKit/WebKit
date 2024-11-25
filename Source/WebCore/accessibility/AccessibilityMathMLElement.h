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
    static Ref<AccessibilityMathMLElement> create(AXID, RenderObject&, bool isAnonymousOperator);
    virtual ~AccessibilityMathMLElement();

protected:
    explicit AccessibilityMathMLElement(AXID, RenderObject&, bool isAnonymousOperator);

private:
    AccessibilityRole determineAccessibilityRole() final;
    void addChildren() final;
    String textUnderElement(TextUnderElementMode = TextUnderElementMode()) const final;
    String stringValue() const final;
    bool isIgnoredElementWithinMathTree() const final;

    bool isMathElement() const final { return true; }

    bool isMathFraction() const final;
    bool isMathFenced() const final;
    bool isMathSubscriptSuperscript() const final;
    bool isMathRow() const final;
    bool isMathUnderOver() const final;
    bool isMathRoot() const final;
    bool isMathSquareRoot() const final;
    bool isMathText() const final;
    bool isMathNumber() const final;
    bool isMathOperator() const final;
    bool isMathFenceOperator() const final;
    bool isMathSeparatorOperator() const final;
    bool isMathIdentifier() const final;
    bool isMathTable() const final;
    bool isMathTableRow() const final;
    bool isMathTableCell() const final;
    bool isMathMultiscript() const final;
    bool isMathToken() const final;
    bool isMathScriptObject(AccessibilityMathScriptObjectType) const final;
    bool isMathMultiscriptObject(AccessibilityMathMultiscriptObjectType) const final;

    // Generic components.
    AXCoreObject* mathBaseObject() final;

    // Root components.
    std::optional<AccessibilityChildrenVector> mathRadicand() final;
    AXCoreObject* mathRootIndexObject() final;

    // Fraction components.
    AXCoreObject* mathNumeratorObject() final;
    AXCoreObject* mathDenominatorObject() final;

    // Under over components.
    AXCoreObject* mathUnderObject() final;
    AXCoreObject* mathOverObject() final;

    // Subscript/superscript components.
    AXCoreObject* mathSubscriptObject() final;
    AXCoreObject* mathSuperscriptObject() final;

    // Fenced components.
    String mathFencedOpenString() const final;
    String mathFencedCloseString() const final;
    int mathLineThickness() const final;
    bool isAnonymousMathOperator() const final;

    // Multiscripts components.
    void mathPrescripts(AccessibilityMathMultiscriptPairs&) final;
    void mathPostscripts(AccessibilityMathMultiscriptPairs&) final;

    bool m_isAnonymousOperator;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_ACCESSIBILITY(AccessibilityMathMLElement, isMathElement())

#endif // ENABLE(MATHML)
