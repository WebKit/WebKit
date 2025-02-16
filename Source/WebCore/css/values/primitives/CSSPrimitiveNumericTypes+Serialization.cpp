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
#include "CSSPrimitiveNumericTypes+Serialization.h"

#include <limits>
#include <wtf/text/MakeString.h>

namespace WebCore {
namespace CSS {

static NEVER_INLINE ASCIILiteral formatNonfiniteCSSNumberValuePrefix(double number)
{
    if (number == std::numeric_limits<double>::infinity())
        return "infinity"_s;
    if (number == -std::numeric_limits<double>::infinity())
        return "-infinity"_s;
    ASSERT(std::isnan(number));
    return "NaN"_s;
}

NEVER_INLINE void formatNonfiniteCSSNumberValue(StringBuilder& builder, const SerializableNumber& number)
{
    return builder.append(formatNonfiniteCSSNumberValuePrefix(number.value), number.suffix.isEmpty() ? ""_s : " * 1"_s, number.suffix);
}

NEVER_INLINE String formatNonfiniteCSSNumberValue(const SerializableNumber& number)
{
    return makeString(formatNonfiniteCSSNumberValuePrefix(number.value), number.suffix.isEmpty() ? ""_s : " * 1"_s, number.suffix);
}

NEVER_INLINE void formatCSSNumberValue(StringBuilder& builder, const SerializableNumber& number)
{
    if (!std::isfinite(number.value))
        return formatNonfiniteCSSNumberValue(builder, number);
    return builder.append(FormattedCSSNumber::create(number.value), number.suffix);
}

NEVER_INLINE String formatCSSNumberValue(const SerializableNumber& number)
{
    if (!std::isfinite(number.value))
        return formatNonfiniteCSSNumberValue(number);
    return makeString(FormattedCSSNumber::create(number.value), number.suffix);
}

void Serialize<SerializableNumber>::operator()(StringBuilder& builder, const SerializationContext&, const SerializableNumber& number)
{
    formatCSSNumberValue(builder, number);
}

} // namespace CSS
} // namespace WebCore
