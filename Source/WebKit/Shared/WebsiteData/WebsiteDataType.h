/*
 * Copyright (C) 2014-2020 Apple Inc. All rights reserved.
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

namespace WebKit {

enum class WebsiteDataType : uint32_t {
    Cookies = 1 << 0,
    DiskCache = 1 << 1,
    MemoryCache = 1 << 2,
    OfflineWebApplicationCache = 1 << 3,
    SessionStorage = 1 << 4,
    LocalStorage = 1 << 5,
    WebSQLDatabases = 1 << 6,
    IndexedDBDatabases = 1 << 7,
    MediaKeys = 1 << 8,
    HSTSCache = 1 << 9,
    SearchFieldRecentSearches = 1 << 10,
    ResourceLoadStatistics = 1 << 12,
    Credentials = 1 << 13,
    ServiceWorkerRegistrations = 1 << 14,
    DOMCache = 1 << 15,
    DeviceIdHashSalt = 1 << 16,
    PrivateClickMeasurements = 1 << 17,
#if HAVE(ALTERNATIVE_SERVICE)
    AlternativeServices = 1 << 18,
#endif
    FileSystem = 1 << 19,
    BackgroundFetchStorage = 1 << 20,
};

} // namespace WebKit
