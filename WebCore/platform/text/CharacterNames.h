/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef CharacterNames_h
#define CharacterNames_h

#include <wtf/unicode/Unicode.h>

namespace WebCore {

    // Names here are taken from the Unicode standard.

    // Note, these are UChar constants, not UChar32, which makes them
    // more convenient for WebCore code that mostly uses UTF-16.

    const UChar blackSquare = 0x25A0;
    const UChar bullet = 0x2022;
    const UChar hebrewPunctuationGershayim = 0x05F4;
    const UChar horizontalEllipsis = 0x2026;
    const UChar ideographicSpace = 0x3000;
    const UChar ideographicComma = 0x3001;
    const UChar ideographicFullStop = 0x3002;
    const UChar leftToRightMark = 0x200E;
    const UChar leftToRightEmbed = 0x202A;
    const UChar leftToRightOverride = 0x202D;
    const UChar newlineCharacter = 0x000A;
    const UChar noBreakSpace = 0x00A0;
    const UChar objectReplacementCharacter = 0xFFFC;
    const UChar popDirectionalFormatting = 0x202C;
    const UChar replacementCharacter = 0xFFFD;
    const UChar rightSingleQuotationMark = 0x2019;
    const UChar rightToLeftMark = 0x200F;
    const UChar rightToLeftEmbed = 0x202B;
    const UChar rightToLeftOverride = 0x202E;
    const UChar softHyphen = 0x00AD;
    const UChar whiteBullet = 0x25E6;
    const UChar zeroWidthSpace = 0x200B;

}

#endif // CharacterNames_h
