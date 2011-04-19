/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebStorageQuotaCallbacksImpl_h
#define WebStorageQuotaCallbacksImpl_h

#include "WebStorageQuotaCallbacks.h"
#include <wtf/PassOwnPtr.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>

namespace WebCore {
class StorageInfoErrorCallback;
class StorageInfoQuotaCallback;
class StorageInfoUsageCallback;
}

namespace WebKit {

class WebStorageQuotaCallbacksImpl : public WebStorageQuotaCallbacks {
public:
    // The class is self-destructed and thus we have bare constructors.
    WebStorageQuotaCallbacksImpl(PassRefPtr<WebCore::StorageInfoUsageCallback>, PassRefPtr<WebCore::StorageInfoErrorCallback>);
    WebStorageQuotaCallbacksImpl(PassRefPtr<WebCore::StorageInfoQuotaCallback>, PassRefPtr<WebCore::StorageInfoErrorCallback>);

    virtual ~WebStorageQuotaCallbacksImpl();

    virtual void didQueryStorageUsageAndQuota(unsigned long long usageInBytes, unsigned long long quotaInBytes);
    virtual void didGrantStorageQuota(unsigned long long grantedQuotaInBytes);
    virtual void didFail(WebStorageQuotaError);

private:
    RefPtr<WebCore::StorageInfoUsageCallback> m_usageCallback;
    RefPtr<WebCore::StorageInfoQuotaCallback> m_quotaCallback;
    RefPtr<WebCore::StorageInfoErrorCallback> m_errorCallback;
};

} // namespace WebKit

#endif // WebStorageQuotaCallbacksImpl_h
