/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#ifndef CTFontSPI_h
#define CTFontSPI_h

#include <CoreText/CoreText.h>

#if USE(APPLE_INTERNAL_SDK)
#include <CoreText/CTFontPriv.h>
#else
enum {
    kCTFontUIFontSystemItalic = 27,
    kCTFontUIFontSystemThin = 102,
    kCTFontUIFontSystemLight = 103,
    kCTFontUIFontSystemUltraLight = 104,
};
#endif

EXTERN_C CTFontRef CTFontCreatePhysicalFontForCharactersWithLanguage(CTFontRef, const UTF16Char* characters, CFIndex length, CFStringRef language, CFIndex* coveredLength);
EXTERN_C bool CTFontIsAppleColorEmoji(CTFontRef);
EXTERN_C bool CTFontDescriptorIsSystemUIFont(CTFontDescriptorRef);
EXTERN_C CTFontRef CTFontCreateForCSS(CFStringRef name, uint16_t weight, CTFontSymbolicTraits, CGFloat size);

#endif // CTFontSPI_h
