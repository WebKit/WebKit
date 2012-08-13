/*
 * Copyright (c) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "HarfBuzzShaperBase.h"

#include "Font.h"
#include <unicode/normlzr.h>
#include <unicode/uchar.h>
#include <wtf/MathExtras.h>
#include <wtf/unicode/Unicode.h>

namespace WebCore {

HarfBuzzShaperBase::HarfBuzzShaperBase(const Font* font, const TextRun& run)
    : m_font(font)
    , m_normalizedBufferLength(0)
    , m_run(run)
    , m_wordSpacingAdjustment(font->wordSpacing())
    , m_padding(0)
    , m_padPerWordBreak(0)
    , m_padError(0)
    , m_letterSpacing(font->letterSpacing())
{
}

static void normalizeSpacesAndMirrorChars(const UChar* source, UChar* destination, int length, HarfBuzzShaperBase::NormalizeMode normalizeMode)
{
    int position = 0;
    bool error = false;
    // Iterate characters in source and mirror character if needed.
    while (position < length) {
        UChar32 character;
        int nextPosition = position;
        U16_NEXT(source, nextPosition, length, character);
        if (Font::treatAsSpace(character))
            character = ' ';
        else if (Font::treatAsZeroWidthSpace(character))
            character = zeroWidthSpace;
        else if (normalizeMode == HarfBuzzShaperBase::NormalizeMirrorChars)
            character = u_charMirror(character);
        U16_APPEND(destination, position, length, character, error);
        ASSERT_UNUSED(error, !error);
        position = nextPosition;
    }
}

void HarfBuzzShaperBase::setNormalizedBuffer(NormalizeMode normalizeMode)
{
    // Normalize the text run in three ways:
    // 1) Convert the |originalRun| to NFC normalized form if combining diacritical marks
    // (U+0300..) are used in the run. This conversion is necessary since most OpenType
    // fonts (e.g., Arial) don't have substitution rules for the diacritical marks in
    // their GSUB tables.
    //
    // Note that we don't use the icu::Normalizer::isNormalized(UNORM_NFC) API here since
    // the API returns FALSE (= not normalized) for complex runs that don't require NFC
    // normalization (e.g., Arabic text). Unless the run contains the diacritical marks,
    // HarfBuzz will do the same thing for us using the GSUB table.
    // 2) Convert spacing characters into plain spaces, as some fonts will provide glyphs
    // for characters like '\n' otherwise.
    // 3) Convert mirrored characters such as parenthesis for rtl text.

    // Convert to NFC form if the text has diacritical marks.
    icu::UnicodeString normalizedString;
    UErrorCode error = U_ZERO_ERROR;

    for (int i = 0; i < m_run.length(); ++i) {
        UChar ch = m_run[i];
        if (::ublock_getCode(ch) == UBLOCK_COMBINING_DIACRITICAL_MARKS) {
            icu::Normalizer::normalize(icu::UnicodeString(m_run.characters(),
                                       m_run.length()), UNORM_NFC, 0 /* no options */,
                                       normalizedString, error);
            if (U_FAILURE(error))
                normalizedString.remove();
            break;
        }
    }

    const UChar* sourceText;
    if (normalizedString.isEmpty()) {
        m_normalizedBufferLength = m_run.length();
        sourceText = m_run.characters();
    } else {
        m_normalizedBufferLength = normalizedString.length();
        sourceText = normalizedString.getBuffer();
    }

    m_normalizedBuffer = adoptArrayPtr(new UChar[m_normalizedBufferLength + 1]);
    normalizeSpacesAndMirrorChars(sourceText, m_normalizedBuffer.get(), m_normalizedBufferLength, normalizeMode);
}

bool HarfBuzzShaperBase::isWordEnd(unsigned index)
{
    // This could refer a high-surrogate, but should work.
    return index && isCodepointSpace(m_run[index]);
}

int HarfBuzzShaperBase::determineWordBreakSpacing()
{
    int wordBreakSpacing = m_wordSpacingAdjustment;

    if (m_padding > 0) {
        int toPad = roundf(m_padPerWordBreak + m_padError);
        m_padError += m_padPerWordBreak - toPad;

        if (m_padding < toPad)
            toPad = m_padding;
        m_padding -= toPad;
        wordBreakSpacing += toPad;
    }
    return wordBreakSpacing;
}

// setPadding sets a number of pixels to be distributed across the TextRun.
// WebKit uses this to justify text.
void HarfBuzzShaperBase::setPadding(int padding)
{
    m_padding = padding;
    m_padError = 0;
    if (!m_padding)
        return;

    // If we have padding to distribute, then we try to give an equal
    // amount to each space. The last space gets the smaller amount, if
    // any.
    unsigned numWordEnds = 0;

    for (unsigned i = 0; i < m_normalizedBufferLength; i++) {
        if (isWordEnd(i))
            numWordEnds++;
    }

    if (numWordEnds)
        m_padPerWordBreak = m_padding / numWordEnds;
    else
        m_padPerWordBreak = 0;
}

} // namespace WebCore
