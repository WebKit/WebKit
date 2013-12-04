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

#ifndef WKOriginDataManager_h
#define WKOriginDataManager_h

#include <WebKit2/WKBase.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    kWKApplicationCacheOriginData = 1 << 0,
    kWKCookieOriginData = 1 << 1,
    kWKDatabaseOriginData = 1 << 2,
    kWKKeyValueStorageOriginData = 1 << 3,
    kWKMediaCacheOriginData = 1 << 4,
    kWKPluginDataOriginData = 1 << 5,
    kWKResourceCacheOriginData = 1 << 6,

    kWKAllOriginData = (1 << 7) - 1
};
typedef uint32_t WKOriginDataTypes;

WK_EXPORT WKTypeID WKOriginDataManagerGetTypeID();

typedef void (*WKOriginDataManagerGetOriginsFunction)(WKArrayRef, WKErrorRef, void*);
WK_EXPORT void WKOriginDataManagerGetOrigins(WKOriginDataManagerRef originDataManager, WKOriginDataTypes types, void* context, WKOriginDataManagerGetOriginsFunction function);

WK_EXPORT void WKOriginDataManagerDeleteEntriesForOrigin(WKOriginDataManagerRef originDataManager, WKOriginDataTypes types, WKSecurityOriginRef origin);
WK_EXPORT void WKOriginDataManagerDeleteAllEntries(WKOriginDataManagerRef originDataManager, WKOriginDataTypes types);

// OriginDataManager Client
typedef void (*WKOriginDataManagerChangeCallback)(WKOriginDataManagerRef originDataManager, const void *clientInfo);

typedef struct WKOriginDataManagerChangeClientBase {
    const void *                                                        clientInfo;
    int                                                                 version;
} WKOriginDataManagerChangeClientBase;

typedef struct WKOriginDataManagerChangeClientV0 {
    WKOriginDataManagerChangeClientBase                                 base;

    // Version 0.
    WKOriginDataManagerChangeCallback                                   didChange;
} WKOriginDataManagerChangeClientV0;

enum { kWKOriginDataManagerChangeClientVersion = 0 };
typedef struct WKOriginDataManagerChangeClient {
    int                                                                 version;
    const void *                                                        clientInfo;

    // Version 0.
    WKOriginDataManagerChangeCallback                                   didChange;
} WKOriginDataManagerChangeClient WK_DEPRECATED("Use an explicit versioned struct instead");

WK_EXPORT void WKOriginDataManagerStartObservingChanges(WKOriginDataManagerRef originDataManager, WKOriginDataTypes types);
WK_EXPORT void WKOriginDataManagerStopObservingChanges(WKOriginDataManagerRef originDataManager, WKOriginDataTypes types);
WK_EXPORT void WKOriginDataManagerSetChangeClient(WKOriginDataManagerRef originDataManger, const WKOriginDataManagerChangeClientBase* client);
#ifdef __cplusplus
}
#endif

#endif // WKOriginDataManager_h
