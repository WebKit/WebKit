/*
 * Copyright (C) 2013-2016 Apple Inc. All rights reserved.
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

#include "config.h"
#include "BlobRegistryProxy.h"

#include "NetworkConnectionToWebProcessMessages.h"
#include "NetworkProcessConnection.h"
#include "WebCoreArgumentCoders.h"
#include "WebProcess.h"
#include <WebCore/BlobDataFileReference.h>
#include <WebCore/CrossOriginOpenerPolicy.h>
#include <WebCore/SWContextManager.h>

namespace WebKit {
using namespace WebCore;

void BlobRegistryProxy::registerInternalFileBlobURL(const URL& url, Ref<BlobDataFileReference>&& file, const String& path, const String& contentType)
{
    SandboxExtension::Handle extensionHandle;

    // File path can be empty when submitting a form file input without a file, see bug 111778.
    if (!file->path().isEmpty()) {
        if (auto handle = SandboxExtension::createHandle(file->path(), SandboxExtension::Type::ReadOnly))
            extensionHandle = WTFMove(*handle);
    }

    String replacementPath = path == file->path() ? nullString() : file->path();
    WebProcess::singleton().ensureNetworkProcessConnection().connection().send(Messages::NetworkConnectionToWebProcess::RegisterInternalFileBlobURL(url, path, replacementPath, extensionHandle, contentType), 0);
}

void BlobRegistryProxy::registerInternalBlobURL(const URL& url, Vector<BlobPart>&& blobParts, const String& contentType)
{
    WebProcess::singleton().ensureNetworkProcessConnection().connection().send(Messages::NetworkConnectionToWebProcess::RegisterInternalBlobURL(url, blobParts, contentType), 0);
}

void BlobRegistryProxy::registerBlobURL(const URL& url, const URL& srcURL, const PolicyContainer& policyContainer, const std::optional<SecurityOriginData>& topOrigin)
{
    WebProcess::singleton().ensureNetworkProcessConnection().connection().send(Messages::NetworkConnectionToWebProcess::RegisterBlobURL { url, srcURL, policyContainer, topOrigin }, 0);
}

void BlobRegistryProxy::registerInternalBlobURLOptionallyFileBacked(const URL& url, const URL& srcURL, RefPtr<WebCore::BlobDataFileReference>&& file, const String& contentType)
{
    ASSERT(file);
    WebProcess::singleton().ensureNetworkProcessConnection().connection().send(Messages::NetworkConnectionToWebProcess::RegisterInternalBlobURLOptionallyFileBacked(url, srcURL, file->path(), contentType), 0);
}

void BlobRegistryProxy::unregisterBlobURL(const URL& url, const std::optional<SecurityOriginData>& topOrigin)
{
    WebProcess::singleton().ensureNetworkProcessConnection().connection().send(Messages::NetworkConnectionToWebProcess::UnregisterBlobURL(url, topOrigin), 0);
}

void BlobRegistryProxy::registerInternalBlobURLForSlice(const URL& url, const URL& srcURL, long long start, long long end, const String& contentType)
{
    WebProcess::singleton().ensureNetworkProcessConnection().connection().send(Messages::NetworkConnectionToWebProcess::RegisterInternalBlobURLForSlice(url, srcURL, start, end, contentType), 0);
}

void BlobRegistryProxy::registerBlobURLHandle(const URL& url, const std::optional<SecurityOriginData>& topOrigin)
{
    WebProcess::singleton().ensureNetworkProcessConnection().connection().send(Messages::NetworkConnectionToWebProcess::RegisterBlobURLHandle(url, topOrigin), 0);
}

void BlobRegistryProxy::unregisterBlobURLHandle(const URL& url, const std::optional<SecurityOriginData>& topOrigin)
{
    WebProcess::singleton().ensureNetworkProcessConnection().connection().send(Messages::NetworkConnectionToWebProcess::UnregisterBlobURLHandle(url, topOrigin), 0);
}

unsigned long long BlobRegistryProxy::blobSize(const URL& url)
{
    auto sendResult = WebProcess::singleton().ensureNetworkProcessConnection().connection().sendSync(Messages::NetworkConnectionToWebProcess::BlobSize(url), 0);
    auto [resultSize] = sendResult.takeReplyOr(0);
    return resultSize;
}

void BlobRegistryProxy::writeBlobsToTemporaryFilesForIndexedDB(const Vector<String>& blobURLs, CompletionHandler<void(Vector<String>&& filePaths)>&& completionHandler)
{
    WebProcess::singleton().ensureNetworkProcessConnection().writeBlobsToTemporaryFilesForIndexedDB(blobURLs, WTFMove(completionHandler));
}

}
