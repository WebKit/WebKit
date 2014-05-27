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

#include "config.h"
#include "BlobRegistryProxy.h"

#if ENABLE(NETWORK_PROCESS)

#include "NetworkConnectionToWebProcessMessages.h"
#include "NetworkProcessConnection.h"
#include "WebCoreArgumentCoders.h"
#include "WebProcess.h"
#include <WebCore/BlobDataFileReference.h>

using namespace WebCore;

namespace WebKit {

void BlobRegistryProxy::registerFileBlobURL(const WebCore::URL& url, PassRefPtr<BlobDataFileReference> file, const String& contentType)
{
    ASSERT(WebProcess::shared().usesNetworkProcess());

    SandboxExtension::Handle extensionHandle;

    // File path can be empty when submitting a form file input without a file, see bug 111778.
    if (!file->path().isEmpty())
        SandboxExtension::createHandle(file->path(), SandboxExtension::ReadOnly, extensionHandle);

    WebProcess::shared().networkConnection()->connection()->send(Messages::NetworkConnectionToWebProcess::RegisterFileBlobURL(url, file->path(), extensionHandle, contentType), 0);
}

void BlobRegistryProxy::registerBlobURL(const URL& url, Vector<BlobPart> blobParts, const String& contentType)
{
    ASSERT(WebProcess::shared().usesNetworkProcess());

    WebProcess::shared().networkConnection()->connection()->send(Messages::NetworkConnectionToWebProcess::RegisterBlobURL(url, blobParts, contentType), 0);
}

void BlobRegistryProxy::registerBlobURL(const URL& url, const URL& srcURL)
{
    ASSERT(WebProcess::shared().usesNetworkProcess());

    WebProcess::shared().networkConnection()->connection()->send(Messages::NetworkConnectionToWebProcess::RegisterBlobURLFromURL(url, srcURL), 0);
}

void BlobRegistryProxy::unregisterBlobURL(const URL& url)
{
    ASSERT(WebProcess::shared().usesNetworkProcess());

    WebProcess::shared().networkConnection()->connection()->send(Messages::NetworkConnectionToWebProcess::UnregisterBlobURL(url), 0);
}

void BlobRegistryProxy::registerBlobURLForSlice(const URL& url, const URL& srcURL, long long start, long long end)
{
    ASSERT(WebProcess::shared().usesNetworkProcess());

    WebProcess::shared().networkConnection()->connection()->send(Messages::NetworkConnectionToWebProcess::RegisterBlobURLForSlice(url, srcURL, start, end), 0);
}

unsigned long long BlobRegistryProxy::blobSize(const URL& url)
{
    ASSERT(WebProcess::shared().usesNetworkProcess());

    uint64_t resultSize;
    if (!WebProcess::shared().networkConnection()->connection()->sendSync(Messages::NetworkConnectionToWebProcess::BlobSize(url), Messages::NetworkConnectionToWebProcess::BlobSize::Reply(resultSize), 0))
        return 0;
    return resultSize;
}

}

#endif // ENABLE(NETWORK_PROCESS)
