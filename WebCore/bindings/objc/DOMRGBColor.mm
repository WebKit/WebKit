/*
 * Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
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

#import "config.h"
#import "DOMRGBColor.h"

#import "CSSPrimitiveValue.h"
#import "Color.h"
#import "ColorMac.h"
#import "DOMCSSPrimitiveValue.h"
#import "DOMInternal.h"
#import "WebCoreObjCExtras.h"
#import <wtf/GetPtr.h>

namespace WebCore {

static CFMutableDictionaryRef wrapperCache = NULL;

id getWrapperForRGB(WebCore::RGBA32 value)
{
    if (!wrapperCache)
        return nil;
    return (id)CFDictionaryGetValue(wrapperCache, reinterpret_cast<const void*>(value));
}

void setWrapperForRGB(id wrapper, WebCore::RGBA32 value)
{
    if (!wrapperCache)
        // No need to retain/free either impl key, or id value.  Items will be removed
        // from the cache in dealloc methods.
        wrapperCache = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
    CFDictionarySetValue(wrapperCache, reinterpret_cast<const void*>(value), wrapper);
}

void removeWrapperForRGB(WebCore::RGBA32 value)
{
    if (!wrapperCache)
        return;
    CFDictionaryRemoveValue(wrapperCache, reinterpret_cast<const void*>(value));
}

} // namespace WebCore


@implementation DOMRGBColor

#ifndef BUILDING_ON_TIGER
+ (void)initialize
{
    WebCoreObjCFinalizeOnMainThread(self);
}
#endif

- (void)dealloc
{
    WebCore::removeWrapperForRGB(reinterpret_cast<uintptr_t>(_internal));
    _internal = 0;
    [super dealloc];
}

- (void)finalize
{
    WebCore::removeWrapperForRGB(reinterpret_cast<uintptr_t>(_internal));
    [super finalize];
}

- (DOMCSSPrimitiveValue *)red
{
    WebCore::RGBA32 rgb = reinterpret_cast<uintptr_t>(_internal);
    int value = (rgb >> 16) & 0xFF;
    return [DOMCSSPrimitiveValue _wrapCSSPrimitiveValue:new WebCore::CSSPrimitiveValue(value, WebCore::CSSPrimitiveValue::CSS_NUMBER)];
}

- (DOMCSSPrimitiveValue *)green
{
    WebCore::RGBA32 rgb = reinterpret_cast<uintptr_t>(_internal);
    int value = (rgb >> 8) & 0xFF;
    return [DOMCSSPrimitiveValue _wrapCSSPrimitiveValue:new WebCore::CSSPrimitiveValue(value, WebCore::CSSPrimitiveValue::CSS_NUMBER)];
}

- (DOMCSSPrimitiveValue *)blue
{
    WebCore::RGBA32 rgb = reinterpret_cast<uintptr_t>(_internal);
    int value = rgb & 0xFF;
    return [DOMCSSPrimitiveValue _wrapCSSPrimitiveValue:new WebCore::CSSPrimitiveValue(value, WebCore::CSSPrimitiveValue::CSS_NUMBER)];
}

- (DOMCSSPrimitiveValue *)alpha
{
    WebCore::RGBA32 rgb = reinterpret_cast<uintptr_t>(_internal);
    float value = static_cast<float>(WebCore::Color(rgb).alpha()) / 0xFF;
    return [DOMCSSPrimitiveValue _wrapCSSPrimitiveValue:new WebCore::CSSPrimitiveValue(value, WebCore::CSSPrimitiveValue::CSS_NUMBER)];
    
}

- (NSColor *)color
{
    WebCore::RGBA32 rgb = reinterpret_cast<uintptr_t>(_internal);
    return WebCore::nsColor(WebCore::Color(rgb));
}

@end

@implementation DOMRGBColor (WebPrivate)

// FIXME: this should be removed as soon as all internal Apple uses of it have been replaced with
// calls to the public method - (NSColor *)color.
- (NSColor *)_color
{
    return [self color];
}

@end

@implementation DOMRGBColor (WebCoreInternal)

- (WebCore::RGBA32)_RGBColor
{
     return static_cast<WebCore::RGBA32>(reinterpret_cast<uintptr_t>(_internal));
}

- (id)_initWithRGB:(WebCore::RGBA32)value
{
    [super _init];
    _internal = reinterpret_cast<DOMObjectInternal*>(value);
    WebCore::setWrapperForRGB(self, value);
    return self;
}

+ (DOMRGBColor *)_wrapRGBColor:(WebCore::RGBA32)value
{
    id cachedInstance;
    cachedInstance = WebCore::getWrapperForRGB(value);
    if (cachedInstance)
        return [[cachedInstance retain] autorelease];
    return [[[self alloc] _initWithRGB:value] autorelease];
}

@end
