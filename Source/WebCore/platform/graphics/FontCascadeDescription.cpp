/*
 * Copyright (C) 2007 Nicholas Shanks <contact@nickshanks.com>
 * Copyright (C) 2008, 2013 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "FontCascadeDescription.h"

#include <wtf/text/StringHash.h>

namespace WebCore {

struct SameSizeAsFontCascadeDescription {
    Vector<void*> vector;
#if ENABLE(VARIATION_FONTS)
    Vector<void*> vector2;
#else
    char c;
#endif
    AtomicString string;
    int16_t fontSelectionRequest[3];
    float size;
    unsigned bitfields1;
    unsigned bitfields2 : 22;
    void* array;
    float size2;
    unsigned bitfields3 : 10;
};

COMPILE_ASSERT(sizeof(FontCascadeDescription) == sizeof(SameSizeAsFontCascadeDescription), FontCascadeDescription_should_stay_small);

FontCascadeDescription::FontCascadeDescription()
    : m_isAbsoluteSize(false)
    , m_kerning(static_cast<unsigned>(Kerning::Auto))
    , m_keywordSize(0)
    , m_fontSmoothing(static_cast<unsigned>(FontSmoothingMode::AutoSmoothing))
    , m_isSpecifiedFont(false)
{
}

#if !USE_PLATFORM_SYSTEM_FALLBACK_LIST
unsigned FontCascadeDescription::effectiveFamilyCount() const
{
    return familyCount();
}

FontFamilySpecification FontCascadeDescription::effectiveFamilyAt(unsigned i) const
{
    return familyAt(i);
}
#endif

FontSelectionValue FontCascadeDescription::lighterWeight(FontSelectionValue weight)
{
    if (weight < FontSelectionValue(100))
        return weight;
    if (weight < FontSelectionValue(550))
        return FontSelectionValue(100);
    if (weight < FontSelectionValue(750))
        return FontSelectionValue(400);
    return FontSelectionValue(700);
}

FontSelectionValue FontCascadeDescription::bolderWeight(FontSelectionValue weight)
{
    if (weight < FontSelectionValue(350))
        return FontSelectionValue(400);
    if (weight < FontSelectionValue(550))
        return FontSelectionValue(700);
    if (weight < FontSelectionValue(900))
        return FontSelectionValue(900);
    return weight;
}

#if ENABLE(TEXT_AUTOSIZING)

bool FontCascadeDescription::familiesEqualForTextAutoSizing(const FontCascadeDescription& other) const
{
    unsigned thisFamilyCount = familyCount();
    unsigned otherFamilyCount = other.familyCount();

    if (thisFamilyCount != otherFamilyCount)
        return false;

    for (unsigned i = 0; i < thisFamilyCount; ++i) {
        if (!equalIgnoringASCIICase(familyAt(i), other.familyAt(i)))
            return false;
    }

    return true;
}

#endif // ENABLE(TEXT_AUTOSIZING)

bool FontCascadeDescription::familyNamesAreEqual(const AtomicString& family1, const AtomicString& family2)
{
    // FIXME: <rdar://problem/33594253> CoreText matches dot-prefixed font names case sensitively. We should
    // always take the case insensitive patch once this radar is fixed.
    if (family1.startsWith('.'))
        return StringHash::equal(family1.string(), family2.string());
    return ASCIICaseInsensitiveHash::equal(family1, family2);
}

unsigned FontCascadeDescription::familyNameHash(const AtomicString& family)
{
    // FIXME: <rdar://problem/33594253> CoreText matches dot-prefixed font names case sensitively. We should
    // always take the case insensitive patch once this radar is fixed.
    if (family.startsWith('.'))
        return StringHash::hash(family.string());
    return ASCIICaseInsensitiveHash::hash(family);
}

String FontCascadeDescription::foldedFamilyName(const AtomicString& family)
{
    // FIXME: <rdar://problem/33594253> CoreText matches dot-prefixed font names case sensitively. We should
    // always take the case insensitive patch once this radar is fixed.
    if (family.startsWith('.'))
        return family.string();
    return family.string().foldCase();
}

} // namespace WebCore
