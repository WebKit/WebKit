/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"
#include "CSSLayerStatementRule.h"

#include "CSSLayerBlockRule.h"
#include "CSSStyleSheet.h"
#include "StyleRule.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {

CSSLayerStatementRule::CSSLayerStatementRule(StyleRuleLayer& rule, CSSStyleSheet* parent)
    : CSSRule(parent)
    , m_layerRule(rule)
{
    ASSERT(rule.isStatement());
}

Ref<CSSLayerStatementRule> CSSLayerStatementRule::create(StyleRuleLayer& rule, CSSStyleSheet* parent)
{
    return adoptRef(*new CSSLayerStatementRule(rule, parent));
}

CSSLayerStatementRule::~CSSLayerStatementRule() = default;

String CSSLayerStatementRule::cssText() const
{
    StringBuilder result;

    result.append("@layer ");

    auto nameList = this->nameList();
    for (auto& name : nameList) {
        result.append(name);
        if (&name != &nameList.last())
            result.append(", ");
    }
    result.append(';');

    return result.toString();
}

Vector<String> CSSLayerStatementRule::nameList() const
{
    Vector<String> result;

    for (auto& name : m_layerRule.get().nameList())
        result.append(stringFromCascadeLayerName(name));

    return result;
}

void CSSLayerStatementRule::reattach(StyleRuleBase& rule)
{
    m_layerRule = downcast<StyleRuleLayer>(rule);
}

} // namespace WebCore

