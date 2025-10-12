/*
 * Copyright (C) 2015-2021 Apple Inc. All rights reserved.
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <WebCore/CSSValue.h>
#include <WebCore/CSSVariableData.h>
#include <WebCore/CSSVariableReferenceValue.h>
#include <WebCore/CSSWideKeyword.h>

namespace WebCore {

class CSSParserToken;

class CSSCustomPropertyValue final : public CSSValue {
public:
    using VariantValue = Variant<
        Ref<CSSVariableReferenceValue>,
        Ref<CSSVariableData>,
        CSSWideKeyword
    >;

    static Ref<CSSCustomPropertyValue> createEmpty(const AtomString& name);
    static Ref<CSSCustomPropertyValue> createUnresolved(const AtomString& name, Ref<CSSVariableReferenceValue>&&);
    static Ref<CSSCustomPropertyValue> createSyntaxAll(const AtomString& name, Ref<CSSVariableData>&&);
    static Ref<CSSCustomPropertyValue> createWithCSSWideKeyword(const AtomString& name, CSSWideKeyword);

    const AtomString& name() const { return m_name; }
    const VariantValue& value() const { return m_value; }

    Ref<const CSSVariableData> asVariableData() const;

    bool isCurrentColor() const;

    bool isVariableReference() const;
    bool isVariableData() const;
    bool isCSSWideKeyword() const;

    std::optional<CSSWideKeyword> tryCSSWideKeyword() const;

    String customCSSText(const CSS::SerializationContext&) const;
    bool equals(const CSSCustomPropertyValue&) const;
    IterationStatus customVisitChildren(NOESCAPE const Function<IterationStatus(CSSValue&)>&) const;

private:
    CSSCustomPropertyValue(const AtomString& name, VariantValue&& value)
        : CSSValue(ClassType::CustomProperty)
        , m_name(name)
        , m_value(WTFMove(value))
    {
    }

    const Vector<CSSParserToken>& tokens() const;

    const AtomString m_name;
    const VariantValue m_value;
    mutable String m_cachedCSSText;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CSS_VALUE(CSSCustomPropertyValue, isCustomPropertyValue())
