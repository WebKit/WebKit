/*
 * Copyright (C) 2025 Igalia, S.L. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(MATHML)

namespace WebCore {

// These are the mathvariant values from the MathML3 recommendation.
// The special value none means that no explicit mathvariant value has been specified.
// Note that the numeral values are important for the computation performed in the mathVariantMapCodePoint function, do not change them!
// In MathML Core only the italic mathvariant is used to implement `text-transform: math-auto`, other values are deprecated.
enum class MathVariant {
    None = 0,
    Normal = 1,
    Bold = 2,
    Italic = 3,
    BoldItalic = 4,
    Script = 5,
    BoldScript = 6,
    Fraktur = 7,
    DoubleStruck = 8,
    BoldFraktur = 9,
    SansSerif = 10,
    BoldSansSerif = 11,
    SansSerifItalic = 12,
    SansSerifBoldItalic = 13,
    Monospace = 14,
    Initial = 15,
    Tailed = 16,
    Looped = 17,
    Stretched = 18
};

char32_t mathVariantMapCodePoint(char32_t, MathVariant);

} // namespace WebCore

#endif // ENABLE(MATHML)
