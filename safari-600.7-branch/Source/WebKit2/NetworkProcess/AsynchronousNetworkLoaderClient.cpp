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
#include "AsynchronousNetworkLoaderClient.h"

#include "DataReference.h"
#include "NetworkResourceLoader.h"
#include "WebCoreArgumentCoders.h"
#include "WebResourceLoaderMessages.h"
#include <WebCore/CertificateInfo.h>
#include <WebCore/ResourceError.h>
#include <WebCore/SharedBuffer.h>
#include <wtf/CurrentTime.h>

#if ENABLE(NETWORK_PROCESS)

using namespace WebCore;

namespace WebKit {

AsynchronousNetworkLoaderClient::AsynchronousNetworkLoaderClient()
{
}

void AsynchronousNetworkLoaderClient::willSendRequest(NetworkResourceLoader* loader, ResourceRequest& request, const ResourceResponse& redirectResponse)
{
    loader->sendAbortingOnFailure(Messages::WebResourceLoader::WillSendRequest(request, redirectResponse));
}

#if USE(PROTECTION_SPACE_AUTH_CALLBACK)
void AsynchronousNetworkLoaderClient::canAuthenticateAgainstProtectionSpace(NetworkResourceLoader* loader, const ProtectionSpace& protectionSpace)
{
    loader->sendAbortingOnFailure(Messages::WebResourceLoader::CanAuthenticateAgainstProtectionSpace(protectionSpace));
}
#endif

void AsynchronousNetworkLoaderClient::didReceiveResponse(NetworkResourceLoader* loader, const ResourceResponse& response)
{
    loader->sendAbortingOnFailure(Messages::WebResourceLoader::DidReceiveResponseWithCertificateInfo(response, CertificateInfo(response), loader->isLoadingMainResource()));
}

void AsynchronousNetworkLoaderClient::didReceiveBuffer(NetworkResourceLoader* loader, SharedBuffer* buffer, int encodedDataLength)
{
#if PLATFORM(IOS) || (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 1090)
    ShareableResource::Handle shareableResourceHandle;
    NetworkResourceLoader::tryGetShareableHandleFromSharedBuffer(shareableResourceHandle, buffer);
    if (!shareableResourceHandle.isNull()) {
        // Since we're delivering this resource by ourselves all at once and don't need anymore data or callbacks from the network layer, abort the loader.
        loader->abort();
        loader->send(Messages::WebResourceLoader::DidReceiveResource(shareableResourceHandle, currentTime()));
        return;
    }
#endif // __MAC_OS_X_VERSION_MIN_REQUIRED >= 1090

    IPC::SharedBufferDataReference dataReference(buffer);
    loader->sendAbortingOnFailure(Messages::WebResourceLoader::DidReceiveData(dataReference, encodedDataLength));
}

void AsynchronousNetworkLoaderClient::didSendData(NetworkResourceLoader* loader, unsigned long long bytesSent, unsigned long long totalBytesToBeSent)
{
    loader->send(Messages::WebResourceLoader::DidSendData(bytesSent, totalBytesToBeSent));
}

void AsynchronousNetworkLoaderClient::didFinishLoading(NetworkResourceLoader* loader, double finishTime)
{
    loader->send(Messages::WebResourceLoader::DidFinishResourceLoad(finishTime));
}

void AsynchronousNetworkLoaderClient::didFail(NetworkResourceLoader* loader, const ResourceError& error)
{
    loader->send(Messages::WebResourceLoader::DidFailResourceLoad(error));
}

} // namespace WebKit

#endif // ENABLE(NETWORK_PROCESS)
