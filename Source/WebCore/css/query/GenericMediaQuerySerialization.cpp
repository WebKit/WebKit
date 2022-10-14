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
#include "GenericMediaQuerySerialization.h"

#include "CSSMarkup.h"
#include "CSSValue.h"

namespace WebCore {
namespace MQ {

static void serialize(StringBuilder& builder, const QueryInParens& queryInParens)
{
    WTF::switchOn(queryInParens, [&](auto& node) {
        builder.append('(');
        serialize(builder, node);
        builder.append(')');
    }, [&](const GeneralEnclosed& generalEnclosed) {
        builder.append(generalEnclosed.name);
        builder.append('(');
        builder.append(generalEnclosed.text);
        builder.append(')');
    });
}

void serialize(StringBuilder& builder, const Condition& condition)
{
    if (condition.queries.size() == 1 && condition.logicalOperator == LogicalOperator::Not) {
        builder.append("not ");
        serialize(builder, condition.queries.first());
        return;
    }

    for (auto& query : condition.queries) {
        if (&query != &condition.queries.first())
            builder.append(condition.logicalOperator == LogicalOperator::And ? " and " : " or ");
        serialize(builder, query);
    }
}

void serialize(StringBuilder& builder, const Feature& feature)
{
    auto serializeRangeComparisonOperator = [&](ComparisonOperator op) {
        builder.append(' ');
        switch (op) {
        case ComparisonOperator::LessThan:
            builder.append('<');
            break;
        case ComparisonOperator::LessThanOrEqual:
            builder.append("<=");
            break;
        case ComparisonOperator::Equal:
            builder.append('=');
            break;
        case ComparisonOperator::GreaterThan:
            builder.append('>');
            break;
        case ComparisonOperator::GreaterThanOrEqual:
            builder.append(">=");
            break;
        }
        builder.append(' ');
    };

    switch (feature.syntax) {
    case Syntax::Boolean:
        serializeIdentifier(feature.name, builder);
        break;

    case Syntax::Plain:
        switch (feature.rightComparison->op) {
        case MQ::ComparisonOperator::LessThanOrEqual:
            builder.append("max-");
            break;
        case MQ::ComparisonOperator::Equal:
            break;
        case MQ::ComparisonOperator::GreaterThanOrEqual:
            builder.append("min-");
            break;
        case MQ::ComparisonOperator::LessThan:
        case MQ::ComparisonOperator::GreaterThan:
            ASSERT_NOT_REACHED();
            break;
        }
        serializeIdentifier(feature.name, builder);

        builder.append(": ");
        builder.append(feature.rightComparison->value->cssText());
        break;

    case Syntax::Range:
        if (feature.leftComparison) {
            builder.append(feature.leftComparison->value->cssText());
            serializeRangeComparisonOperator(feature.leftComparison->op);
        }

        serializeIdentifier(feature.name, builder);

        if (feature.rightComparison) {
            serializeRangeComparisonOperator(feature.rightComparison->op);
            builder.append(feature.rightComparison->value->cssText());
        }
        break;
    }
}

}
}
