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
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CSSFontVariantAlternatesValue.h"

#include <wtf/text/TextStream.h>

namespace WebCore {

CSSFontVariantAlternatesValue::CSSFontVariantAlternatesValue(FontVariantAlternates&& alternates)
    : CSSValue(FontVariantAlternatesClass)
    , m_value(alternates)
{
}

void CSSFontVariantAlternatesValue::customCSSText(StringBuilder& builder) const
{
    if (m_value.isNormal()) {
        builder.append("normal"_s);
        return;
    }

    auto initialLengthOfBuilder = builder.length();

    auto values = m_value.values();
    auto append = [&]<typename ...Ts>(Ts&& ...args) {
        // Separate elements with a space.
        builder.append(initialLengthOfBuilder == builder.length() ? ""_s: " "_s, std::forward<Ts>(args)...);
    };
    // FIXME: These strings needs to be escaped.
    if (!values.stylistic.isNull())
        append("stylistic("_s, values.stylistic, ')');
    if (values.historicalForms)
        append("historical-forms"_s);
    if (!values.styleset.isEmpty())
        append("styleset("_s, interleave(values.styleset, ", "_s), ')');
    if (!values.characterVariant.isEmpty())
        append("character-variant("_s, interleave(values.characterVariant, ", "_s), ')');
    if (!values.swash.isNull())
        append("swash("_s, values.swash, ')');
    if (!values.ornaments.isNull())
        append("ornaments("_s, values.ornaments, ')');
    if (!values.annotation.isNull())
        append("annotation("_s, values.annotation, ')');
}

bool CSSFontVariantAlternatesValue::equals(const CSSFontVariantAlternatesValue& other) const
{
    return value() == other.value();
}

}
