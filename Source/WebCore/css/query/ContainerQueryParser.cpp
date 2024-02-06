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
#include "ContainerQueryParser.h"

#include "CSSPrimitiveValue.h"
#include "CSSPropertyParserHelpers.h"
#include "ContainerQueryFeatures.h"

namespace WebCore {
namespace CQ {

std::optional<ContainerQuery> ContainerQueryParser::consumeContainerQuery(CSSParserTokenRange& range, const MediaQueryParserContext& context)
{
    auto consumeName = [&] {
        if (range.peek().type() == LeftParenthesisToken || range.peek().type() == FunctionToken)
            return nullAtom();
        auto nameValue = CSSPropertyParserHelpers::consumeSingleContainerName(range);
        if (!nameValue)
            return nullAtom();
        return AtomString { nameValue->stringValue() };
    };

    auto name = consumeName();

    auto condition = consumeCondition(range, context);
    if (!condition)
        return { };

    OptionSet<Axis> requiredAxes;
    auto containsUnknownFeature = ContainsUnknownFeature::No;

    traverseFeatures(*condition, [&](auto& feature) {
        requiredAxes.add(requiredAxesForFeature(feature));
        if (!feature.schema)
            containsUnknownFeature = ContainsUnknownFeature::Yes;
    });

    return ContainerQuery { name, *condition, requiredAxes, containsUnknownFeature };
}

Vector<const MQ::FeatureSchema*> ContainerQueryParser::featureSchemas()
{
    return {
        &Features::width(),
        &Features::height(),
        &Features::inlineSize(),
        &Features::blockSize(),
        &Features::aspectRatio(),
        &Features::orientation(),
    };
}

}
}
