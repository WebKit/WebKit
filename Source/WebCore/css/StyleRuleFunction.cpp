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

#include "config.h"
#include "StyleRuleFunction.h"

#include "MutableStyleProperties.h"
#include "StylePropertiesInlines.h"

namespace WebCore {

Ref<StyleRuleFunction> StyleRuleFunction::create(const AtomString& name, Vector<Parameter>&& parameters, CSSCustomPropertySyntax&& returnType, Vector<Ref<StyleRuleBase>>&& rules)
{
    return adoptRef(*new StyleRuleFunction(name, WTFMove(parameters), WTFMove(returnType), WTFMove(rules)));
}

StyleRuleFunction::StyleRuleFunction(const AtomString& name, Vector<Parameter>&& parameters, CSSCustomPropertySyntax&& returnType, Vector<Ref<StyleRuleBase>>&& rules)
    : StyleRuleGroup(StyleRuleType::Function, WTFMove(rules))
    , m_name(name)
    , m_parameters(WTFMove(parameters))
    , m_returnType(WTFMove(returnType))
{
}

StyleRuleFunction::StyleRuleFunction(const StyleRuleFunction&) = default;

StyleRuleFunctionDeclarations::StyleRuleFunctionDeclarations(Ref<StyleProperties>&& properties)
    : StyleRuleBase(StyleRuleType::FunctionDeclarations)
    , m_properties(WTFMove(properties))
{
}

StyleRuleFunctionDeclarations::StyleRuleFunctionDeclarations(const StyleRuleFunctionDeclarations&) = default;

MutableStyleProperties& StyleRuleFunctionDeclarations::mutableProperties()
{
    Ref properties = m_properties;

    if (!is<MutableStyleProperties>(properties))
        m_properties = properties->mutableCopy();

    return downcast<MutableStyleProperties>(m_properties.get());
}

}
