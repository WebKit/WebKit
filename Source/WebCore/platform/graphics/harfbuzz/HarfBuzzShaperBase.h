/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#ifndef HarfBuzzShaperBase_h
#define HarfBuzzShaperBase_h

#include "TextRun.h"
#include <wtf/OwnArrayPtr.h>
#include <wtf/unicode/CharacterNames.h>

namespace WebCore {

class Font;

class HarfBuzzShaperBase {
public:
    enum NormalizeMode {
        DoNotNormalizeMirrorChars,
        NormalizeMirrorChars
    };

    HarfBuzzShaperBase(const Font*, const TextRun&);
    virtual ~HarfBuzzShaperBase() { }

protected:
    void setNormalizedBuffer(NormalizeMode = DoNotNormalizeMirrorChars);

    bool isWordEnd(unsigned);
    int determineWordBreakSpacing();
    // setPadding sets a number of pixels to be distributed across the TextRun.
    // WebKit uses this to justify text.
    void setPadding(int);

    // In complex text word-spacing affects each line-break, space (U+0020) and non-breaking space (U+00A0).
    static bool isCodepointSpace(UChar c) { return c == ' ' || c == noBreakSpace || c == '\n'; }

    const Font* m_font;
    OwnArrayPtr<UChar> m_normalizedBuffer;
    unsigned m_normalizedBufferLength;
    const TextRun& m_run;

    int m_wordSpacingAdjustment; // delta adjustment (pixels) for each word break.
    float m_padding; // pixels to be distributed over the line at word breaks.
    float m_padPerWordBreak; // pixels to be added to each word break.
    float m_padError; // |m_padPerWordBreak| might have a fractional component.
                      // Since we only add a whole number of padding pixels at
                      // each word break we accumulate error. This is the
                      // number of pixels that we are behind so far.
    int m_letterSpacing; // pixels to be added after each glyph.
};

} // namespace WebCore

#endif // HarfBuzzShaperBase_h
