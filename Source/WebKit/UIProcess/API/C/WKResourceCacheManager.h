/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#ifndef WKResourceCacheManager_h
#define WKResourceCacheManager_h

#include <WebKit/WKBase.h>
#include <WebKit/WKDeprecated.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    WKResourceCachesToClearAll = 0,
    WKResourceCachesToClearInMemoryOnly = 1
};
typedef uint32_t WKResourceCachesToClear;

WK_EXPORT WKTypeID WKResourceCacheManagerGetTypeID() WK_C_API_DEPRECATED;

typedef void (*WKResourceCacheManagerGetCacheOriginsFunction)(WKArrayRef, WKErrorRef, void*);
WK_EXPORT void WKResourceCacheManagerGetCacheOrigins(WKResourceCacheManagerRef contextRef, void* context, WKResourceCacheManagerGetCacheOriginsFunction function) WK_C_API_DEPRECATED;

WK_EXPORT void WKResourceCacheManagerClearCacheForOrigin(WKResourceCacheManagerRef cacheManger, WKSecurityOriginRef origin, WKResourceCachesToClear cachesToClear) WK_C_API_DEPRECATED;
WK_EXPORT void WKResourceCacheManagerClearCacheForAllOrigins(WKResourceCacheManagerRef cacheManager, WKResourceCachesToClear cachesToClear) WK_C_API_DEPRECATED;

#ifdef __cplusplus
}
#endif

#endif // WKResourceCacheManager_h
