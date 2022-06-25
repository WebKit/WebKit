/*
 * Copyright (C) 2004-2016 Apple Inc. All rights reserved.
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

#import <WebKitLegacy/DOMObject.h>

@class DOMCSSPrimitiveValue;
#if !TARGET_OS_IPHONE
@class NSColor;
#else
typedef struct CGColor* CGColorRef;
#endif

WEBKIT_CLASS_DEPRECATED_MAC(10_4, 10_14)
@interface DOMRGBColor : DOMObject
@property (readonly, strong) DOMCSSPrimitiveValue *red;
@property (readonly, strong) DOMCSSPrimitiveValue *green;
@property (readonly, strong) DOMCSSPrimitiveValue *blue;
@property (readonly, strong) DOMCSSPrimitiveValue *alpha;
#if !TARGET_OS_IPHONE
@property (readonly, copy) NSColor *color WEBKIT_AVAILABLE_MAC(10_5);
#else
- (CGColorRef)color;
#endif
@end
