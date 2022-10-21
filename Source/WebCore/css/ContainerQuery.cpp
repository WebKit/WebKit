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
#include "GenericMediaQuerySerialization.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {
namespace CQ {
namespace FeatureSchemas {

using namespace MQ;

const FeatureSchema& width()
{
    static MainThreadNeverDestroyed<FeatureSchema> schema { FeatureSchema { "width"_s, FeatureSchema::Type::Range, { FeatureSchema::ValueType::Length }, { } } };
    return schema;
}

const FeatureSchema& height()
{
    static MainThreadNeverDestroyed<FeatureSchema> schema { FeatureSchema { "height"_s, FeatureSchema::Type::Range, { FeatureSchema::ValueType::Length }, { } } };
    return schema;
}

const FeatureSchema& inlineSize()
{
    static MainThreadNeverDestroyed<FeatureSchema> schema { FeatureSchema { "inline-size"_s, FeatureSchema::Type::Range, { FeatureSchema::ValueType::Length }, { } } };
    return schema;
}

const FeatureSchema& blockSize()
{
    static MainThreadNeverDestroyed<FeatureSchema> schema { FeatureSchema { "block-size"_s, FeatureSchema::Type::Range, { FeatureSchema::ValueType::Length }, { } } };
    return schema;
}

const FeatureSchema& aspectRatio()
{
    static MainThreadNeverDestroyed<FeatureSchema> schema { FeatureSchema { "aspect-ratio"_s, FeatureSchema::Type::Range, { FeatureSchema::ValueType::Ratio }, { } } };
    return schema;
}

const FeatureSchema& orientation()
{
    static MainThreadNeverDestroyed<FeatureSchema> schema { FeatureSchema { "orientation"_s, FeatureSchema::Type::Discrete, { }, { CSSValuePortrait, CSSValueLandscape } } };
    return schema;
}

}

OptionSet<Axis> requiredAxesForFeature(const MQ::Feature& feature)
{
    if (feature.schema == &FeatureSchemas::width())
        return { Axis::Width };
    if (feature.schema == &FeatureSchemas::height())
        return { Axis::Height };
    if (feature.schema == &FeatureSchemas::inlineSize())
        return { Axis::Inline };
    if (feature.schema == &FeatureSchemas::blockSize())
        return { Axis::Block };
    if (feature.schema == &FeatureSchemas::aspectRatio() || feature.schema == &FeatureSchemas::orientation())
        return { Axis::Inline, Axis::Block };
    return { };
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

