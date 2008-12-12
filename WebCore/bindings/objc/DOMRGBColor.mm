/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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
#import <runtime/InitializeThreading.h>
#import <wtf/GetPtr.h>

namespace WebCore {

static NSMapTable* RGBColorWrapperCache;

id getWrapperForRGB(WebCore::RGBA32 value)
{
    if (!RGBColorWrapperCache)
        return nil;
    return static_cast<id>(NSMapGet(RGBColorWrapperCache, reinterpret_cast<const void*>(value)));
}

void setWrapperForRGB(id wrapper, WebCore::RGBA32 value)
{
    if (!RGBColorWrapperCache)
        // No need to retain/free either impl key, or id value.  Items will be removed
        // from the cache in dealloc methods.
        RGBColorWrapperCache = createWrapperCacheWithIntegerKeys();
    NSMapInsert(RGBColorWrapperCache, reinterpret_cast<const void*>(value), wrapper);
}

void removeWrapperForRGB(WebCore::RGBA32 value)
{
    if (!RGBColorWrapperCache)
        return;
    NSMapRemove(RGBColorWrapperCache, reinterpret_cast<const void*>(value));
}

} // namespace WebCore


@implementation DOMRGBColor

+ (void)initialize
{
    JSC::initializeThreading();
#ifndef BUILDING_ON_TIGER
    WebCoreObjCFinalizeOnMainThread(self);
#endif
}

- (void)dealloc
{
    if (WebCoreObjCScheduleDeallocateOnMainThread([DOMRGBColor class], self))
        return;
    
    WebCore::removeWrapperForRGB(reinterpret_cast<uintptr_t>(_internal));
    _internal = 0;
    [super dealloc];
}

- (DOMCSSPrimitiveValue *)red
{
    WebCore::RGBA32 rgb = reinterpret_cast<uintptr_t>(_internal);
    int value = (rgb >> 16) & 0xFF;
    return [DOMCSSPrimitiveValue _wrapCSSPrimitiveValue:WebCore::CSSPrimitiveValue::create(value, WebCore::CSSPrimitiveValue::CSS_NUMBER).get()];
}

- (DOMCSSPrimitiveValue *)green
{
    WebCore::RGBA32 rgb = reinterpret_cast<uintptr_t>(_internal);
    int value = (rgb >> 8) & 0xFF;
    return [DOMCSSPrimitiveValue _wrapCSSPrimitiveValue:WebCore::CSSPrimitiveValue::create(value, WebCore::CSSPrimitiveValue::CSS_NUMBER).get()];
}

- (DOMCSSPrimitiveValue *)blue
{
    WebCore::RGBA32 rgb = reinterpret_cast<uintptr_t>(_internal);
    int value = rgb & 0xFF;
    return [DOMCSSPrimitiveValue _wrapCSSPrimitiveValue:WebCore::CSSPrimitiveValue::create(value, WebCore::CSSPrimitiveValue::CSS_NUMBER).get()];
}

- (DOMCSSPrimitiveValue *)alpha
{
    WebCore::RGBA32 rgb = reinterpret_cast<uintptr_t>(_internal);
    float value = static_cast<float>(WebCore::Color(rgb).alpha()) / 0xFF;
    return [DOMCSSPrimitiveValue _wrapCSSPrimitiveValue:WebCore::CSSPrimitiveValue::create(value, WebCore::CSSPrimitiveValue::CSS_NUMBER).get()];
    
}

- (NSColor *)color
{
    WebCore::RGBA32 rgb = reinterpret_cast<uintptr_t>(_internal);
    return WebCore::nsColor(WebCore::Color(rgb));
}

@end

@implementation DOMRGBColor (WebPrivate)

// FIXME: this should be removed once all internal Apple uses of it have been replaced with
// calls to the public method, color without the leading underscore.
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
