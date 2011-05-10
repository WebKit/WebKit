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

#include "config.h"
#include "WebStorageQuotaCallbacksImpl.h"

#if ENABLE(QUOTA)

#include "DOMCoreException.h"
#include "ExceptionCode.h"
#include "StorageInfoErrorCallback.h"
#include "StorageInfoQuotaCallback.h"
#include "StorageInfoUsageCallback.h"

using namespace WebCore;

namespace WebKit {

WebStorageQuotaCallbacksImpl::WebStorageQuotaCallbacksImpl(PassRefPtr<WebCore::StorageInfoUsageCallback> usageCallback, PassRefPtr<WebCore::StorageInfoErrorCallback> errorCallback)
    : m_usageCallback(usageCallback)
    , m_errorCallback(errorCallback)
{
}

WebStorageQuotaCallbacksImpl::WebStorageQuotaCallbacksImpl(PassRefPtr<WebCore::StorageInfoQuotaCallback> quotaCallback, PassRefPtr<WebCore::StorageInfoErrorCallback> errorCallback)
    : m_quotaCallback(quotaCallback)
    , m_errorCallback(errorCallback)
{
}

WebStorageQuotaCallbacksImpl::~WebStorageQuotaCallbacksImpl()
{
}

void WebStorageQuotaCallbacksImpl::didQueryStorageUsageAndQuota(unsigned long long usageInBytes, unsigned long long quotaInBytes)
{
    if (m_usageCallback)
        m_usageCallback->handleEvent(usageInBytes, quotaInBytes);
    delete this;
}

void WebStorageQuotaCallbacksImpl::didGrantStorageQuota(unsigned long long grantedQuotaInBytes)
{
    if (m_quotaCallback)
        m_quotaCallback->handleEvent(grantedQuotaInBytes);
    delete this;
}

void WebStorageQuotaCallbacksImpl::didFail(WebStorageQuotaError error)
{
    if (m_errorCallback) {
        ExceptionCodeDescription description;
        getExceptionCodeDescription(static_cast<ExceptionCode>(error), description);
        m_errorCallback->handleEvent(DOMCoreException::create(description).get());
    }
    delete this;
}

} // namespace WebKit

#endif // ENABLE(QUOTA)
