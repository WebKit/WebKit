/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "CSSPropertyRule.h"

#include "CSSMarkup.h"
#include "CSSParserTokenRange.h"
#include "CSSStyleSheet.h"
#include "StyleRule.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {

CSSPropertyRule::CSSPropertyRule(StyleRuleProperty& rule, CSSStyleSheet* parent)
    : CSSRule(parent)
    , m_propertyRule(rule)
{
}

Ref<CSSPropertyRule> CSSPropertyRule::create(StyleRuleProperty& rule, CSSStyleSheet* parent)
{
    return adoptRef(*new CSSPropertyRule(rule, parent));
}

CSSPropertyRule::~CSSPropertyRule() = default;

String CSSPropertyRule::name() const
{
    return m_propertyRule->descriptor().name;
}

String CSSPropertyRule::syntax() const
{
    return m_propertyRule->descriptor().syntax;
}

bool CSSPropertyRule::inherits() const
{
    return m_propertyRule->descriptor().inherits.value_or(false);
}

String CSSPropertyRule::initialValue() const
{
    if (!m_propertyRule->descriptor().initialValue)
        return nullString();

    return m_propertyRule->descriptor().initialValue->tokenRange().serialize();
}

String CSSPropertyRule::cssText() const
{
    StringBuilder builder;

    auto& descriptor = m_propertyRule->descriptor();

    builder.append("@property ");
    serializeIdentifier(descriptor.name, builder);
    builder.append(" { ");

    if (!descriptor.syntax.isNull()) {
        builder.append("syntax: ");
        serializeString(syntax(), builder);
        builder.append("; ");
    }

    if (descriptor.inherits)
        builder.append("inherits: ", *descriptor.inherits ? "true" : "false", "; ");

    if (descriptor.initialValue)
        builder.append("initial-value: ", initialValue(), "; ");

    builder.append("}");

    return builder.toString();
}

void CSSPropertyRule::reattach(StyleRuleBase& rule)
{
    m_propertyRule = downcast<StyleRuleProperty>(rule);
}

} // namespace WebCore
