/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "CSSCustomPropertySyntax.h"
#include "CSSVariableData.h"
#include "StyleRule.h"

namespace WebCore {

class MutableStyleProperties;

class StyleRuleFunction final : public StyleRuleGroup {
public:
    struct Parameter {
        AtomString name;
        CSSCustomPropertySyntax type;
        RefPtr<CSSVariableData> defaultValue;
    };

    static Ref<StyleRuleFunction> create(const AtomString& name, Vector<Parameter>&&, CSSCustomPropertySyntax&& returnType, Vector<Ref<StyleRuleBase>>&&);

    Ref<StyleRuleFunction> copy() const { return adoptRef(*new StyleRuleFunction(*this)); }

    AtomString name() const { return m_name; }
    const Vector<Parameter>& parameters() const { return m_parameters; }
    const CSSCustomPropertySyntax& returnType() const { return m_returnType; }

private:
    StyleRuleFunction(const AtomString& name, Vector<Parameter>&&, CSSCustomPropertySyntax&& returnType, Vector<Ref<StyleRuleBase>>&&);
    StyleRuleFunction(const StyleRuleFunction&);

    const AtomString m_name;
    const Vector<Parameter> m_parameters;
    const CSSCustomPropertySyntax m_returnType;
};

class StyleRuleFunctionDeclarations : public StyleRuleBase {
public:
    static Ref<StyleRuleFunctionDeclarations> create(Ref<StyleProperties>&& properties)
    {
        return adoptRef(*new StyleRuleFunctionDeclarations(WTFMove(properties)));
    }

    Ref<StyleRuleFunctionDeclarations> copy() const { return adoptRef(*new StyleRuleFunctionDeclarations(*this)); }

    // Only contains property "result" and custom properties.
    const StyleProperties& properties() const { return m_properties.get(); }
    MutableStyleProperties& mutableProperties();

private:
    StyleRuleFunctionDeclarations(Ref<StyleProperties>&&);
    StyleRuleFunctionDeclarations(const StyleRuleFunctionDeclarations&);

    Ref<StyleProperties> m_properties;
};

}

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::StyleRuleFunction)
    static bool isType(const WebCore::StyleRuleBase& rule) { return rule.type() == WebCore::StyleRuleType::Function; }
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::StyleRuleFunctionDeclarations)
    static bool isType(const WebCore::StyleRuleBase& rule) { return rule.type() == WebCore::StyleRuleType::FunctionDeclarations; }
SPECIALIZE_TYPE_TRAITS_END()
