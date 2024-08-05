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
#include "CSSLayerBlockRule.h"

#include "CSSMarkup.h"
#include "CSSStyleSheet.h"
#include "StyleRule.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {

CSSLayerBlockRule::CSSLayerBlockRule(StyleRuleLayer& rule, CSSStyleSheet* parent)
    : CSSGroupingRule(rule, parent)
{
    ASSERT(!rule.isStatement());
}

Ref<CSSLayerBlockRule> CSSLayerBlockRule::create(StyleRuleLayer& rule, CSSStyleSheet* parent)
{
    return adoptRef(*new CSSLayerBlockRule(rule, parent));
}

void CSSLayerBlockRule::cssText(StringBuilder& builder) const
{
    builder.append("@layer"_s);

    auto& layer = downcast<StyleRuleLayer>(groupRule());
    if (!layer.name().isEmpty()) {
        builder.append(' ');
        serializedCascadeLayerName(builder, layer.name());
    }
    appendCSSTextForItems(builder);
}

String CSSLayerBlockRule::name() const
{
    auto& layer = downcast<StyleRuleLayer>(groupRule());

    if (layer.name().isEmpty())
        return emptyString();

    return serializedCascadeLayerName(layer.name());
}

String serializedCascadeLayerName(const CascadeLayerName& name)
{
    return makeString(interleave(name, [](auto& builder, auto& name) { serializeIdentifier(builder, name); }, '.'));
}

void serializedCascadeLayerName(StringBuilder& builder, const CascadeLayerName& name)
{
    builder.append(interleave(name, [](auto& builder, auto& name) { serializeIdentifier(builder, name); }, '.'));
}

} // namespace WebCore

