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
#include "ContainerQueryFeatures.h"
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

