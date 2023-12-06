/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#include "CSSScopeRule.h"

#include "StyleRule.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {

CSSScopeRule::CSSScopeRule(StyleRuleScope& rule, CSSStyleSheet* parent)
    : CSSGroupingRule(rule, parent)
{
}

Ref<CSSScopeRule> CSSScopeRule::create(StyleRuleScope& rule, CSSStyleSheet* parent)
{
    return adoptRef(*new CSSScopeRule(rule, parent));
}

const StyleRuleScope& CSSScopeRule::styleRuleScope() const
{
    return downcast<StyleRuleScope>(groupRule());
}

String CSSScopeRule::cssText() const
{
    StringBuilder builder;
    builder.append("@scope");
    auto start = this->start();
    if (!start.isEmpty())
        builder.append(" (", start, ')');
    auto end = this->end();
    if (!end.isEmpty())
        builder.append(" to ", '(', end, ')');
    appendCSSTextForItems(builder);
    return builder.toString();
}

String CSSScopeRule::start() const
{
    auto& scope = styleRuleScope().originalScopeStart();
    if (scope.isEmpty())
        return { };

    return scope.selectorsText();
}

String CSSScopeRule::end() const
{
    auto& scope = styleRuleScope().originalScopeEnd();
    if (scope.isEmpty())
        return { };

    return scope.selectorsText();
}

} // namespace WebCore
