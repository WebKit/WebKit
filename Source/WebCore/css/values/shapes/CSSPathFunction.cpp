/*
 * Copyright (C) 2024 Samuel Weinig <sam@webkit.org>
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
#include "CSSPathFunction.h"

#include "CSSMarkup.h"
#include "CSSPrimitiveNumericTypes+CSSValueVisitation.h"
#include "CSSPrimitiveNumericTypes+ComputedStyleDependencies.h"
#include "CSSPrimitiveNumericTypes+Serialization.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {
namespace CSS {

void Serialize<Path>::operator()(StringBuilder& builder, const Path& value)
{
    // <path()> = path( <'fill-rule'>? , <string> )

    if (value.fillRule && !std::holds_alternative<Keyword::Nonzero>(*value.fillRule)) {
        serializationForCSS(builder, *value.fillRule);
        builder.append(", "_s);
    }

    // FIXME: Add version of `buildStringFromByteStream` that takes a `StringBuilder`.
    String pathString;
    buildStringFromByteStream(value.data.byteStream, pathString, UnalteredParsing);
    serializeString(pathString, builder);
}

void ComputedStyleDependenciesCollector<Path::Data>::operator()(ComputedStyleDependencies&, const Path::Data&)
{
}

IterationStatus CSSValueChildrenVisitor<Path::Data>::operator()(const Function<IterationStatus(CSSValue&)>&, const Path::Data&)
{
    return IterationStatus::Continue;
}

} // namespace CSS
} // namespace WebCore
