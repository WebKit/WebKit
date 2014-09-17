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

#ifndef CTFontDescriptorSPI_h
#define CTFontDescriptorSPI_h

#include <CoreText/CoreText.h>

#if USE(APPLE_INTERNAL_SDK)
#include <CoreText/CTFontDescriptorPriv.h>
#endif

#if PLATFORM(IOS) || __MAC_OS_X_VERSION_MIN_REQUIRED >= 10100
EXTERN_C const CFStringRef kCTUIFontTextStyleShortHeadline;
EXTERN_C const CFStringRef kCTUIFontTextStyleShortBody;
EXTERN_C const CFStringRef kCTUIFontTextStyleShortSubhead;
EXTERN_C const CFStringRef kCTUIFontTextStyleShortFootnote;
EXTERN_C const CFStringRef kCTUIFontTextStyleShortCaption1;
EXTERN_C const CFStringRef kCTUIFontTextStyleTallBody;

EXTERN_C const CFStringRef kCTUIFontTextStyleHeadline;
EXTERN_C const CFStringRef kCTUIFontTextStyleBody;
EXTERN_C const CFStringRef kCTUIFontTextStyleSubhead;
EXTERN_C const CFStringRef kCTUIFontTextStyleFootnote;
EXTERN_C const CFStringRef kCTUIFontTextStyleCaption1;
EXTERN_C const CFStringRef kCTUIFontTextStyleCaption2;

EXTERN_C const CFStringRef kCTFontDescriptorTextStyleEmphasized;
#endif

EXTERN_C CTFontDescriptorRef CTFontDescriptorCreateForUIType(CTFontUIFontType, CGFloat size, CFStringRef language);
EXTERN_C CTFontDescriptorRef CTFontDescriptorCreateWithTextStyle(CFStringRef style, CFStringRef size, CFStringRef language);
EXTERN_C bool CTFontDescriptorIsSystemUIFont(CTFontDescriptorRef);

#endif // CTFontDescriptorSPI_h
