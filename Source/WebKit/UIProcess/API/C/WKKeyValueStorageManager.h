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

#ifndef WKKeyValueStorageManager_h
#define WKKeyValueStorageManager_h

#include <WebKit/WKBase.h>
#include <WebKit/WKDeprecated.h>

#ifdef __cplusplus
extern "C" {
#endif

WK_EXPORT WKTypeID WKKeyValueStorageManagerGetTypeID() WK_C_API_DEPRECATED;

/* Value type: WKSecurityOriginRef */
WK_EXPORT WKStringRef WKKeyValueStorageManagerGetOriginKey() WK_C_API_DEPRECATED;

/* Value type: WKDoubleRef, seconds since January 1st, 1970 UTC */
WK_EXPORT WKStringRef WKKeyValueStorageManagerGetCreationTimeKey() WK_C_API_DEPRECATED;

/* Value type: WKDoubleRef, seconds since January 1st, 1970 UTC */
WK_EXPORT WKStringRef WKKeyValueStorageManagerGetModificationTimeKey() WK_C_API_DEPRECATED;

typedef void (*WKKeyValueStorageManagerGetKeyValueStorageOriginsFunction)(WKArrayRef, WKErrorRef, void*);
WK_EXPORT void WKKeyValueStorageManagerGetKeyValueStorageOrigins(WKKeyValueStorageManagerRef keyValueStorageManager, void* context, WKKeyValueStorageManagerGetKeyValueStorageOriginsFunction function) WK_C_API_DEPRECATED;

typedef void (*WKKeyValueStorageManagerGetStorageDetailsByOriginFunction)(WKArrayRef, WKErrorRef, void*);
WK_EXPORT void WKKeyValueStorageManagerGetStorageDetailsByOrigin(WKKeyValueStorageManagerRef keyValueStorageManager, void* context, WKKeyValueStorageManagerGetStorageDetailsByOriginFunction function) WK_C_API_DEPRECATED;

WK_EXPORT void WKKeyValueStorageManagerDeleteEntriesForOrigin(WKKeyValueStorageManagerRef keyValueStorageManager, WKSecurityOriginRef origin) WK_C_API_DEPRECATED;
WK_EXPORT void WKKeyValueStorageManagerDeleteAllEntries(WKKeyValueStorageManagerRef keyValueStorageManager) WK_C_API_DEPRECATED;

#ifdef __cplusplus
}
#endif

#endif // WKKeyValueStorageManager_h
