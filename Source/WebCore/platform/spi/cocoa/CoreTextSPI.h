/*
 * Copyright (C) 2014 Apple Inc.  All rights reserved.
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

#ifndef CoreTextSPI_h
#define CoreTextSPI_h

#include <CoreText/CoreText.h>

#if USE(APPLE_INTERNAL_SDK)
#include <CoreText/CTFontDescriptorPriv.h>
#include <CoreText/CTFontPriv.h>
#include <CoreText/CTLinePriv.h>
#include <CoreText/CTRunPriv.h>
#include <CoreText/CTTypesetterPriv.h>
#endif

extern "C" {

typedef const UniChar* (*CTUniCharProviderCallback)(CFIndex stringIndex, CFIndex* charCount, CFDictionaryRef* attributes, void* refCon);
typedef void (*CTUniCharDisposeCallback)(const UniChar* chars, void* refCon);

#if PLATFORM(IOS) || (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED > 1080)
#if !USE(APPLE_INTERNAL_SDK)
typedef CF_OPTIONS(uint32_t, CTFontTransformOptions)
{
    kCTFontTransformApplyShaping = (1 << 0),
    kCTFontTransformApplyPositioning = (1 << 1)
};
#endif
bool CTFontTransformGlyphs(CTFontRef, CGGlyph glyphs[], CGSize advances[], CFIndex count, CTFontTransformOptions);
#endif

CGSize CTRunGetInitialAdvance(CTRunRef run);
CTLineRef CTLineCreateWithUniCharProvider(CTUniCharProviderCallback provide, CTUniCharDisposeCallback dispose, void* refCon);
CTTypesetterRef CTTypesetterCreateWithUniCharProviderAndOptions(CTUniCharProviderCallback provide, CTUniCharDisposeCallback dispose, void* refCon, CFDictionaryRef options);
bool CTFontGetVerticalGlyphsForCharacters(CTFontRef, const UniChar characters[], CGGlyph glyphs[], CFIndex count);
bool CTFontDescriptorIsSystemUIFont(CTFontDescriptorRef);

}

#endif
