/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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
#include "StorageQuota.h"

#if ENABLE(QUOTA)

#include "Document.h"
#include "ExceptionCode.h"
#include "ScriptExecutionContext.h"
#include "StorageErrorCallback.h"
#include "StorageQuotaCallback.h"
#include "StorageUsageCallback.h"
#include "WebFrameClient.h"
#include "WebFrameImpl.h"
#include "WebStorageQuotaCallbacksImpl.h"
#include "WebStorageQuotaType.h"

using namespace WebKit;

namespace WebCore {

void StorageQuota::queryUsageAndQuota(ScriptExecutionContext* scriptExecutionContext, PassRefPtr<StorageUsageCallback> successCallback, PassRefPtr<StorageErrorCallback> errorCallback)
{
    ASSERT(scriptExecutionContext);
    WebStorageQuotaType storageType = static_cast<WebStorageQuotaType>(m_type);
    if (storageType != WebStorageQuotaTypeTemporary && storageType != WebStorageQuotaTypePersistent) {
        // Unknown storage type is requested.
        scriptExecutionContext->postTask(StorageErrorCallback::CallbackTask::create(errorCallback, NOT_SUPPORTED_ERR));
        return;
    }
    if (scriptExecutionContext->isDocument()) {
        Document* document = static_cast<Document*>(scriptExecutionContext);
        WebFrameImpl* webFrame = WebFrameImpl::fromFrame(document->frame());
        webFrame->client()->queryStorageUsageAndQuota(webFrame, storageType, new WebStorageQuotaCallbacksImpl(successCallback, errorCallback));
    } else {
        // FIXME: calling this on worker is not yet supported.
        scriptExecutionContext->postTask(StorageErrorCallback::CallbackTask::create(errorCallback, NOT_SUPPORTED_ERR));
    }
}

void StorageQuota::requestQuota(ScriptExecutionContext* scriptExecutionContext, unsigned long long newQuotaInBytes, PassRefPtr<StorageQuotaCallback> successCallback, PassRefPtr<StorageErrorCallback> errorCallback)
{
    ASSERT(scriptExecutionContext);
    WebStorageQuotaType storageType = static_cast<WebStorageQuotaType>(m_type);
    if (storageType != WebStorageQuotaTypeTemporary && storageType != WebStorageQuotaTypePersistent) {
        // Unknown storage type is requested.
        scriptExecutionContext->postTask(StorageErrorCallback::CallbackTask::create(errorCallback, NOT_SUPPORTED_ERR));
        return;
    }
    if (scriptExecutionContext->isDocument()) {
        Document* document = static_cast<Document*>(scriptExecutionContext);
        WebFrameImpl* webFrame = WebFrameImpl::fromFrame(document->frame());
        webFrame->client()->requestStorageQuota(webFrame, storageType, newQuotaInBytes, new WebStorageQuotaCallbacksImpl(successCallback, errorCallback));
    } else {
        // FIXME: calling this on worker is not yet supported.
        scriptExecutionContext->postTask(StorageErrorCallback::CallbackTask::create(errorCallback, NOT_SUPPORTED_ERR));
    }
}

} // namespace WebCore

#endif // ENABLE(QUOTA)
