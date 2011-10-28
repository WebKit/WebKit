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
#include "StorageInfo.h"

#if ENABLE(QUOTA)

#include "DOMCoreException.h"
#include "Document.h"
#include "ExceptionCode.h"
#include "ScriptExecutionContext.h"
#include "StorageInfoErrorCallback.h"
#include "StorageInfoQuotaCallback.h"
#include "StorageInfoUsageCallback.h"
#include "WebFrameClient.h"
#include "WebFrameImpl.h"
#include "WebStorageQuotaCallbacksImpl.h"
#include "WebStorageQuotaType.h"

using namespace WebKit;

namespace WebCore {

namespace {
void fireStorageInfoErrorCallback(PassRefPtr<StorageInfoErrorCallback> errorCallback, ExceptionCode ec)
{
    if (!errorCallback)
        return;
    ExceptionCodeDescription description(ec);
    errorCallback->handleEvent(DOMCoreException::create(description).get());
}
}

void StorageInfo::queryUsageAndQuota(ScriptExecutionContext* context, int storageType, PassRefPtr<StorageInfoUsageCallback> successCallback, PassRefPtr<StorageInfoErrorCallback> errorCallback)
{
    ASSERT(context);
    if (storageType != WebStorageQuotaTypeTemporary && storageType != WebStorageQuotaTypePersistent) {
        // Unknown storage type is requested.
        fireStorageInfoErrorCallback(errorCallback, NOT_SUPPORTED_ERR);
        return;
    }
    if (context->isDocument()) {
        Document* document = static_cast<Document*>(context);
        WebFrameImpl* webFrame = WebFrameImpl::fromFrame(document->frame());
        webFrame->client()->queryStorageUsageAndQuota(webFrame, static_cast<WebStorageQuotaType>(storageType), new WebStorageQuotaCallbacksImpl(successCallback, errorCallback));
    } else {
        // FIXME: calling this on worker is not yet supported.
        fireStorageInfoErrorCallback(errorCallback, NOT_SUPPORTED_ERR);
    }
}

void StorageInfo::requestQuota(ScriptExecutionContext* context, int storageType, unsigned long long newQuotaInBytes, PassRefPtr<StorageInfoQuotaCallback> successCallback, PassRefPtr<StorageInfoErrorCallback> errorCallback)
{
    ASSERT(context);
    if (storageType != WebStorageQuotaTypeTemporary && storageType != WebStorageQuotaTypePersistent) {
        // Unknown storage type is requested.
        fireStorageInfoErrorCallback(errorCallback, NOT_SUPPORTED_ERR);
        return;
    }
    if (context->isDocument()) {
        Document* document = static_cast<Document*>(context);
        WebFrameImpl* webFrame = WebFrameImpl::fromFrame(document->frame());
        webFrame->client()->requestStorageQuota(webFrame, static_cast<WebStorageQuotaType>(storageType), newQuotaInBytes, new WebStorageQuotaCallbacksImpl(successCallback, errorCallback));
    } else {
        // FIXME: calling this on worker is not yet supported.
        fireStorageInfoErrorCallback(errorCallback, NOT_SUPPORTED_ERR);
    }
}

} // namespace WebCore

#endif // ENABLE(QUOTA)
