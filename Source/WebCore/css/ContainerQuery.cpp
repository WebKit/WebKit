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

#include "CSSMarkup.h"
#include "CSSValue.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {
namespace CQ {
namespace FeatureNames {

const AtomString& width()
{
    static MainThreadNeverDestroyed<AtomString> name { "width"_s };
    return name;
}

const AtomString& height()
{
    static MainThreadNeverDestroyed<AtomString> name { "height"_s };
    return name;
}

const AtomString& inlineSize()
{
    static MainThreadNeverDestroyed<AtomString> name { "inline-size"_s };
    return name;
}

const AtomString& blockSize()
{
    static MainThreadNeverDestroyed<AtomString> name { "block-size"_s };
    return name;
}

const AtomString& aspectRatio()
{
    static MainThreadNeverDestroyed<AtomString> name { "aspect-ratio"_s };
    return name;
}

const AtomString& orientation()
{
    static MainThreadNeverDestroyed<AtomString> name { "orientation"_s };
    return name;
}

}

OptionSet<Axis> requiredAxesForFeature(const AtomString& featureName)
{
    if (featureName == FeatureNames::width())
        return { Axis::Width };
    if (featureName == FeatureNames::height())
        return { Axis::Height };
    if (featureName == FeatureNames::inlineSize())
        return { Axis::Inline };
    if (featureName == FeatureNames::blockSize())
        return { Axis::Block };
    if (featureName == FeatureNames::aspectRatio() || featureName == FeatureNames::orientation())
        return { Axis::Inline, Axis::Block };
    return { };
}

void serialize(StringBuilder&, const SizeFeature&);
template<typename ConditionType> void serialize(StringBuilder&, const ConditionType&);

static void serialize(StringBuilder& builder, const QueryInParens& query)
{
    WTF::switchOn(query, [&](auto& node) {
        builder.append('(');
        serialize(builder, node);
        builder.append(')');
    }, [&](const MQ::GeneralEnclosed& generalEnclosed) {
        builder.append(generalEnclosed.name);
        builder.append('(');
        builder.append(generalEnclosed.text);
        builder.append(')');
    });
}

void serialize(StringBuilder& builder, const SizeFeature& sizeFeature)
{
    auto serializeRangeComparisonOperator = [&](MQ::ComparisonOperator op) {
        builder.append(' ');
        switch (op) {
        case MQ::ComparisonOperator::LessThan:
            builder.append('<');
            break;
        case MQ::ComparisonOperator::LessThanOrEqual:
            builder.append("<=");
            break;
        case MQ::ComparisonOperator::Equal:
            builder.append('=');
            break;
        case MQ::ComparisonOperator::GreaterThan:
            builder.append('>');
            break;
        case MQ::ComparisonOperator::GreaterThanOrEqual:
            builder.append(">=");
            break;
        }
        builder.append(' ');
    };

    switch (sizeFeature.syntax) {
    case MQ::Syntax::Boolean:
        serializeIdentifier(sizeFeature.name, builder);
        break;

    case MQ::Syntax::Plain:
        switch (sizeFeature.rightComparison->op) {
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
        serializeIdentifier(sizeFeature.name, builder);

        builder.append(": ");
        builder.append(sizeFeature.rightComparison->value->cssText());
        break;

    case MQ::Syntax::Range:
        if (sizeFeature.leftComparison) {
            builder.append(sizeFeature.leftComparison->value->cssText());
            serializeRangeComparisonOperator(sizeFeature.leftComparison->op);
        }

        serializeIdentifier(sizeFeature.name, builder);

        if (sizeFeature.rightComparison) {
            serializeRangeComparisonOperator(sizeFeature.rightComparison->op);
            builder.append(sizeFeature.rightComparison->value->cssText());
        }
        break;
    }
}

template<typename ConditionType>
void serialize(StringBuilder& builder, const ConditionType& condition)
{
    if (condition.queries.size() == 1 && condition.logicalOperator == MQ::LogicalOperator::Not) {
        builder.append("not ");
        serialize(builder, condition.queries.first());
        return;
    }

    for (auto& query : condition.queries) {
        if (&query != &condition.queries.first())
            builder.append(condition.logicalOperator == MQ::LogicalOperator::And ? " and " : " or ");
        serialize(builder, query);
    }
}

void serialize(StringBuilder& builder, const ContainerCondition& condition)
{
    serialize<ContainerCondition>(builder, condition);
}

void serialize(StringBuilder& builder, const ContainerQuery& query)
{
    auto name = query.name;
    if (!name.isEmpty()) {
        serializeIdentifier(name, builder);
        builder.append(' ');
    }

    serialize(builder, query.condition);
}

}
}

