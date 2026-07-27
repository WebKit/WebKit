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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ContainerQuery.h"

#include "CSSCustomPropertyValue.h"
#include "CSSMarkup.h"
#include "CSSPropertyParser.h"
#include "CSSTokenizer.h"
#include "CSSValue.h"
#include "CSSValueKeywords.h"
#include "ContainerQueryFeatures.h"
#include "GenericMediaQueryParser.h"
#include "GenericMediaQuerySerialization.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {
namespace CQ {

OptionSet<Axis> requiredAxesForFeature(const MQ::Feature& feature)
{
    if (feature.schema == &Features::width())
        return { Axis::Width };
    if (feature.schema == &Features::height())
        return { Axis::Height };
    if (feature.schema == &Features::inlineSize())
        return { Axis::Inline };
    if (feature.schema == &Features::blockSize())
        return { Axis::Block };
    if (feature.schema == &Features::aspectRatio() || feature.schema == &Features::orientation())
        return { Axis::Inline, Axis::Block };
    return { };
}

void collectCustomPropertyNames(const MQ::Feature& feature, HashSet<AtomString>& names)
{
    auto collectFromCustomPropertyValue = [&](const CSSCustomPropertyValue& value) {
        auto& tokens = value.tokens();

        // A bare <custom-property-name> operand is evaluated as var(--name).
        if (auto name = MQ::bareCustomPropertyName(tokens.span()); !name.isNull())
            names.add(name);

        // var() references, at any nesting depth.
        for (size_t i = 0; i < tokens.size(); ++i) {
            if (tokens[i].type() != FunctionToken || tokens[i].functionId() != CSSValueVar)
                continue;
            for (size_t j = i + 1; j < tokens.size(); ++j) {
                if (CSSTokenizer::isWhitespace(tokens[j].type()))
                    continue;
                if (tokens[j].type() == IdentToken && isCustomPropertyName(tokens[j].value()))
                    names.add(tokens[j].value().toAtomString());
                break;
            }
        }
    };

    auto collectFromValue = [&](const std::optional<MQ::Value>& value) {
        if (!value)
            return;
        if (auto* customProperty = std::get_if<Ref<CSSCustomPropertyValue>>(&*value))
            collectFromCustomPropertyValue(customProperty->get());
    };

    // The queried property of a plain/boolean feature, or the bare-name center of a range.
    if (isCustomPropertyName(feature.name))
        names.add(feature.name);

    // Range operands: a non-name center and the comparison bounds may reference further properties.
    collectFromValue(feature.subject);
    if (feature.leftComparison)
        collectFromValue(feature.leftComparison->value);
    if (feature.rightComparison)
        collectFromValue(feature.rightComparison->value);
}

void serialize(StringBuilder& builder, const ContainerQuery& query)
{
    auto name = query.name;
    if (!name.isEmpty()) {
        serializeIdentifier(builder, name);
        builder.append(' ');
    }

    serialize(builder, query.condition);
}

}
}
