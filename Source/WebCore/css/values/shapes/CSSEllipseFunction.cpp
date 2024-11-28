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
#include "CSSEllipseFunction.h"

#include "CSSPrimitiveNumericTypes+Serialization.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {
namespace CSS {

static bool hasDefaultValueForEllipseRadius(Ellipse::RadialSize radius)
{
    return WTF::switchOn(radius,
        [](Ellipse::Extent extent) {
            // FIXME: The spec says that `farthest-corner` should be the default, but this does not match the tests.
            return std::holds_alternative<ClosestSide>(extent);
        },
        [](const auto&) {
            return false;
        }
    );
}

void Serialize<Ellipse>::operator()(StringBuilder& builder, const Ellipse& value)
{
    // <ellipse()> = ellipse( <radial-size>? [ at <position> ]? )

    auto lengthBefore = builder.length();

    if (!hasDefaultValueForEllipseRadius(get<0>(value.radii)) || !hasDefaultValueForEllipseRadius(get<1>(value.radii)))
        serializationForCSS(builder, value.radii);

    if (value.position) {
        // FIXME: To match other serialization of Percentage, this should not serialize if equal to the default value of 50% 50%, but this does not match the tests.
        bool wroteSomething = builder.length() != lengthBefore;
        builder.append(wroteSomething ? " at "_s : "at "_s);
        serializationForCSS(builder, *value.position);
    }
}

} // namespace CSS
} // namespace WebCore
