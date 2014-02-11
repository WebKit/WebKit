/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "MutableDictionary.h"
#include "WKArray.h"
#include "WKMutableDictionary.h"
#include "WKSharedAPICast.h"

#if PLATFORM(MAC) && !PLATFORM(IOS)
#include "WKContextPrivateMac.h"
#endif

// Deprecated functions that should be removed from the framework once nobody uses them.

using namespace WebKit;

extern "C" {
WK_EXPORT bool WKArrayIsMutable(WKArrayRef array);

WK_EXPORT void WKPageSetVisibilityState(WKPageRef, WKPageVisibilityState, bool);

WK_EXPORT bool WKDictionaryAddItem(WKMutableDictionaryRef dictionary, WKStringRef key, WKTypeRef item);
WK_EXPORT bool WKDictionaryIsMutable(WKDictionaryRef dictionary);
WK_EXPORT void WKDictionaryRemoveItem(WKMutableDictionaryRef dictionary, WKStringRef key);

#if PLATFORM(MAC) && !PLATFORM(IOS)
WK_EXPORT CGContextRef WKGraphicsContextGetCGContext(WKGraphicsContextRef graphicsContext);
#endif
}

bool WKArrayIsMutable(WKArrayRef)
{
    return false;
}

void WKPageSetVisibilityState(WKPageRef, WKPageVisibilityState, bool)
{
}

bool WKDictionaryIsMutable(WKDictionaryRef dictionaryRef)
{
    return toImpl(dictionaryRef)->isMutable();
}

bool WKDictionaryAddItem(WKMutableDictionaryRef dictionaryRef, WKStringRef keyRef, WKTypeRef itemRef)
{
    return toImpl(dictionaryRef)->add(toImpl(keyRef)->string(), toImpl(itemRef));
}

void WKDictionaryRemoveItem(WKMutableDictionaryRef dictionaryRef, WKStringRef keyRef)
{
    toImpl(dictionaryRef)->remove(toImpl(keyRef)->string());
}

#if PLATFORM(MAC) && !PLATFORM(IOS)
CGContextRef WKGraphicsContextGetCGContext(WKGraphicsContextRef graphicsContext)
{
    return nullptr;
}

bool WKContextGetProcessSuppressionEnabled(WKContextRef)
{
    return true;
}

void WKContextSetProcessSuppressionEnabled(WKContextRef, bool)
{
}
#endif
