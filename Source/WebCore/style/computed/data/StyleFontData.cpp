/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
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
#include "StyleFontData.h"

#include "StyleComputedStyle+DifferenceLogging.h"
#include "StyleComputedStyle+InitialInlines.h"
#include "StylePrimitiveKeyword+Logging.h"
#include "StylePrimitiveNumericTypes+Logging.h"

namespace WebCore {
namespace Style {

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(FontData);

FontData::FontData()
    : letterSpacing(ComputedStyle::initialLetterSpacing())
    , wordSpacing(ComputedStyle::initialWordSpacing())
{
}

FontData::FontData(const FontData& o)
    : letterSpacing(o.letterSpacing)
    , wordSpacing(o.wordSpacing)
    , fontCascade(o.fontCascade)
{
}

Ref<FontData> FontData::copy() const
{
    return adoptRef(*new FontData(*this));
}

bool FontData::operator==(const FontData& o) const
{
    return letterSpacing == o.letterSpacing
        && wordSpacing == o.wordSpacing
        && fontCascade == o.fontCascade;
}

#if !LOG_DISABLED
void FontData::dumpDifferences(TextStream& ts, const FontData& other) const
{
    if (fontCascade != other.fontCascade)
        ts << "fontCascade differs:\n  "_s << fontCascade << "\n  "_s << other.fontCascade;
}
#endif

} // namespace Style
} // namespace WebCore
