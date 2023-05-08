/*
 * Copyright (C) 2023 Sony Interactive Entertainment Inc.
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

#pragma once

#include <WebKit/WKBase.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    kWKWebsiteDataTypesCookies = 1 << 0,
    kWKWebsiteDataTypesDiskCache = 1 << 1,
    kWKWebsiteDataTypesMemoryCache = 1 << 2,
    kWKWebsiteDataTypesOfflineWebApplicationCache = 1 << 3,
    kWKWebsiteDataTypesSessionStorage = 1 << 4,
    kWKWebsiteDataTypesLocalStorage = 1 << 5,
    kWKWebsiteDataTypesWebSQLDatabases = 1 << 6,
    kWKWebsiteDataTypesIndexedDBDatabases = 1 << 7,
    kWKWebsiteDataTypesMediaKeys = 1 << 8,
    kWKWebsiteDataTypesHSTSCache = 1 << 9,
    kWKWebsiteDataTypesSearchFieldRecentSearches = 1 << 10,
    kWKWebsiteDataTypesResourceLoadStatistics = 1 << 12,
    kWKWebsiteDataTypesCredentials = 1 << 13,
    kWKWebsiteDataTypesServiceWorkerRegistrations = 1 << 14,
    kWKWebsiteDataTypesDOMCache = 1 << 15,
    kWKWebsiteDataTypesDeviceIdHashSalt = 1 << 16,
    kWKWebsiteDataTypesPrivateClickMeasurements = 1 << 17,
    kWKWebsiteDataTypesAlternativeServices = 1 << 18,
    kWKWebsiteDataTypesFileSystem = 1 << 19,
    kWKWebsiteDataTypesBackgroundFetchStorage = 1 << 20
};
typedef uint64_t WKWebsiteDataTypes;

WK_EXPORT WKTypeID WKWebsiteDataRecordGetTypeID();

WK_EXPORT WKStringRef WKWebsiteDataRecordGetDisplayName(WKWebsiteDataRecordRef dataRecordRef);
WK_EXPORT WKWebsiteDataTypes WKWebsiteDataRecordGetDataTypes(WKWebsiteDataRecordRef dataRecordRef);

#ifdef __cplusplus
}
#endif
