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

    static std::optional<bool> toOptionalBool(const BooleanValue& value) { return value == BooleanValue::Default ? std::nullopt : std::optional<bool>(value == BooleanValue::True); }
    const BooleanValue& cachedBooleanAttribute(const QualifiedName&, std::optional<BooleanValue>&);

    static Length parseMathMLLength(const String&, bool acceptLegacyMathMLLengths);
    const Length& cachedMathMLLength(const QualifiedName&, std::optional<Length>&);

    virtual bool acceptsMathVariantAttribute() { return false; }
    std::optional<MathVariant> specifiedMathVariant() final;

    std::optional<MathVariant> m_mathVariant;

private:
    RenderPtr<RenderElement> createElementRenderer(RenderStyle&&, const RenderTreePosition&) override;
    bool isPresentationMathML() const final { return true; }

    static Length parseNumberAndUnit(StringView, bool acceptLegacyMathMLLengths);
    static Length parseNamedSpace(StringView);
    static MathVariant parseMathVariantAttribute(const AtomString& attributeValue);
};

}

#endif // ENABLE(MATHML)
