/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#if ENABLE(SERVICE_WORKER)

#include "SecurityOriginData.h"
#include "URL.h"

namespace WebCore {

struct ServiceWorkerRegistrationKey {
    URL clientCreationURL;
    SecurityOriginData topOrigin;

    static ServiceWorkerRegistrationKey emptyKey();
    unsigned hash() const;

    bool operator==(const ServiceWorkerRegistrationKey&) const;
};

} // namespace WebCore

namespace WTF {

struct ServiceWorkerRegistrationKeyHash {
    static unsigned hash(const WebCore::ServiceWorkerRegistrationKey& key) { return key.hash(); }
    static bool equal(const WebCore::ServiceWorkerRegistrationKey& a, const WebCore::ServiceWorkerRegistrationKey& b) { return a == b; }
    static const bool safeToCompareToEmptyOrDeleted = false;
};

template<> struct HashTraits<WebCore::ServiceWorkerRegistrationKey> : GenericHashTraits<WebCore::ServiceWorkerRegistrationKey> {
    static WebCore::ServiceWorkerRegistrationKey emptyValue() { return WebCore::ServiceWorkerRegistrationKey::emptyKey(); }

    static void constructDeletedValue(WebCore::ServiceWorkerRegistrationKey& slot) { slot.clientCreationURL = WebCore::URL(HashTableDeletedValue); }
    static bool isDeletedValue(const WebCore::ServiceWorkerRegistrationKey& slot) { return slot.clientCreationURL.isHashTableDeletedValue(); }
};

template<> struct DefaultHash<WebCore::ServiceWorkerRegistrationKey> {
    typedef ServiceWorkerRegistrationKeyHash Hash;
};

} // namespace WTF

#endif // ENABLE(SERVICE_WORKER)
