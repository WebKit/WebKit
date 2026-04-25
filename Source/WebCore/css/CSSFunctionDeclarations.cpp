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
#include "CSSFunctionDeclarations.h"

#include "CSSFunctionDescriptors.h"
#include "CSSSerializationContext.h"
#include "MutableStyleProperties.h"
#include "StylePropertiesInlines.h"
#include "StyleRuleFunction.h"

namespace WebCore {

CSSFunctionDeclarations::CSSFunctionDeclarations(StyleRuleFunctionDeclarations& rule, CSSStyleSheet* parent)
    : CSSRule(parent)
    , m_styleRule(rule)
{
}

CSSFunctionDeclarations::~CSSFunctionDeclarations() = default;

CSSFunctionDescriptors& CSSFunctionDeclarations::style()
{
    if (!m_descriptorsCSSOMWrapper) {
        Ref styleRule = m_styleRule;
        Ref properties = styleRule->mutableProperties();
        m_descriptorsCSSOMWrapper = CSSFunctionDescriptors::create(properties, *this);
    }
    return *m_descriptorsCSSOMWrapper;
}

String CSSFunctionDeclarations::cssText() const
{
    Ref properties = m_styleRule->properties();
    return properties->asText(CSS::defaultSerializationContext());
}

void CSSFunctionDeclarations::reattach(StyleRuleBase& rule)
{
    m_styleRule = downcast<StyleRuleFunctionDeclarations>(rule);

    if (RefPtr wrapper = m_descriptorsCSSOMWrapper) {
        Ref styleRule = m_styleRule;
        Ref properties = styleRule->mutableProperties();
        wrapper->reattach(properties);
    }
}

}
