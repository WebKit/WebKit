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
#include "CSSContainerRule.h"

#include "CSSMarkup.h"
#include "CSSStyleSheet.h"
#include "GenericMediaQuerySerialization.h"
#include "StyleRule.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {

CSSContainerRule::CSSContainerRule(StyleRuleContainer& rule, CSSStyleSheet* parent)
    : CSSConditionRule(rule, parent)
{
}

Ref<CSSContainerRule> CSSContainerRule::create(StyleRuleContainer& rule, CSSStyleSheet* parent)
{
    return adoptRef(*new CSSContainerRule(rule, parent));
}

const StyleRuleContainer& CSSContainerRule::styleRuleContainer() const
{
    return downcast<StyleRuleContainer>(groupRule());
}

String CSSContainerRule::cssText() const
{
    StringBuilder builder;

    builder.append("@container ");

    CQ::serialize(builder, styleRuleContainer().containerQuery());

    builder.append(" {\n");
    appendCSSTextForItems(builder);
    builder.append('}');

    return builder.toString();
}

String CSSContainerRule::conditionText() const
{
    StringBuilder builder;
    MQ::serialize(builder, styleRuleContainer().containerQuery().condition);
    return builder.toString();
}

String CSSContainerRule::nameText() const
{
    StringBuilder builder;
    
    auto name = styleRuleContainer().containerQuery().name;
    if (!name.isEmpty())
        serializeIdentifier(name, builder);

    return builder.toString();
}

} // namespace WebCore

