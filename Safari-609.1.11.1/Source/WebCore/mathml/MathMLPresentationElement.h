/*
 * Copyright (C) 2009 Alex Milowski (alex@milowski.com). All rights reserved.
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(MATHML)

#include "MathMLElement.h"

namespace WebCore {

class MathMLPresentationElement : public MathMLElement {
    WTF_MAKE_ISO_ALLOCATED(MathMLPresentationElement);
public:
    static Ref<MathMLPresentationElement> create(const QualifiedName& tagName, Document&);

protected:
    MathMLPresentationElement(const QualifiedName& tagName, Document&);
    void parseAttribute(const QualifiedName&, const AtomString&) override;

    static bool isPhrasingContent(const Node&);
    static bool isFlowContent(const Node&);

    static Optional<bool> toOptionalBool(const BooleanValue& value) { return value == BooleanValue::Default ? WTF::nullopt : Optional<bool>(value == BooleanValue::True); }
    const BooleanValue& cachedBooleanAttribute(const QualifiedName&, Optional<BooleanValue>&);

    static Length parseMathMLLength(const String&);
    const Length& cachedMathMLLength(const QualifiedName&, Optional<Length>&);

    virtual bool acceptsDisplayStyleAttribute();
    Optional<bool> specifiedDisplayStyle() override;

    virtual bool acceptsMathVariantAttribute() { return false; }
    Optional<MathVariant> specifiedMathVariant() final;

    Optional<BooleanValue> m_displayStyle;
    Optional<MathVariant> m_mathVariant;

private:
    RenderPtr<RenderElement> createElementRenderer(RenderStyle&&, const RenderTreePosition&) override;
    bool isPresentationMathML() const final { return true; }

    static Length parseNumberAndUnit(const StringView&);
    static Length parseNamedSpace(const StringView&);
    static MathVariant parseMathVariantAttribute(const AtomString& attributeValue);
};

}

#endif // ENABLE(MATHML)
