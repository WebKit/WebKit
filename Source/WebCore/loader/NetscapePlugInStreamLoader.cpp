/*
 * Copyright (C) 2006 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "NetscapePlugInStreamLoader.h"

#include "DocumentLoader.h"
#include "FrameLoader.h"
#include "FrameLoaderClient.h"
#include <wtf/Ref.h>

#if ENABLE(CONTENT_EXTENSIONS)
#include "ResourceLoadInfo.h"
#endif

namespace WebCore {

// FIXME: Skip Content Security Policy check when associated plugin element is in a user agent shadow tree.
// See <https://bugs.webkit.org/show_bug.cgi?id=146663>.
NetscapePlugInStreamLoader::NetscapePlugInStreamLoader(Frame* frame, NetscapePlugInStreamLoaderClient* client)
    : ResourceLoader(frame, ResourceLoaderOptions(SendCallbacks, SniffContent, DoNotBufferData, AllowStoredCredentials, AskClientForAllCredentials, ClientRequestedCredentials, SkipSecurityCheck, UseDefaultOriginRestrictionsForType, DoNotIncludeCertificateInfo, ContentSecurityPolicyImposition::DoPolicyCheck, DefersLoadingPolicy::AllowDefersLoading, CachingPolicy::AllowCaching))
    , m_client(client)
{
#if ENABLE(CONTENT_EXTENSIONS)
    m_resourceType = ResourceType::PlugInStream;
#endif
}

NetscapePlugInStreamLoader::~NetscapePlugInStreamLoader()
{
}

PassRefPtr<NetscapePlugInStreamLoader> NetscapePlugInStreamLoader::create(Frame* frame, NetscapePlugInStreamLoaderClient* client, const ResourceRequest& request)
{
    RefPtr<NetscapePlugInStreamLoader> loader(adoptRef(new NetscapePlugInStreamLoader(frame, client)));
    if (!loader->init(request))
        return nullptr;

    return loader.release();
}

bool NetscapePlugInStreamLoader::isDone() const
{
    return !m_client;
}

void NetscapePlugInStreamLoader::releaseResources()
{
    m_client = nullptr;
    ResourceLoader::releaseResources();
}

bool NetscapePlugInStreamLoader::init(const ResourceRequest& request)
{
    if (!ResourceLoader::init(request))
        return false;

    ASSERT(!reachedTerminalState());

    m_documentLoader->addPlugInStreamLoader(*this);
    m_isInitialized = true;

    return true;
}

void NetscapePlugInStreamLoader::willSendRequest(ResourceRequest&& request, const ResourceResponse& redirectResponse, std::function<void(ResourceRequest&&)>&& callback)
{
    RefPtr<NetscapePlugInStreamLoader> protect(this);

    m_client->willSendRequest(this, WTFMove(request), redirectResponse, [protect, redirectResponse, callback](ResourceRequest request) {
        if (!request.isNull())
            protect->willSendRequestInternal(request, redirectResponse);

        callback(WTFMove(request));
    });
}

void NetscapePlugInStreamLoader::didReceiveResponse(const ResourceResponse& response)
{
    Ref<NetscapePlugInStreamLoader> protect(*this);

    m_client->didReceiveResponse(this, response);

    // Don't continue if the stream is cancelled
    if (!m_client)
        return;

    ResourceLoader::didReceiveResponse(response);

    // Don't continue if the stream is cancelled
    if (!m_client)
        return;

    if (!response.isHTTP())
        return;
    
    if (m_client->wantsAllStreams())
        return;

    // Status code can be null when serving from a Web archive.
    if (response.httpStatusCode() && (response.httpStatusCode() < 100 || response.httpStatusCode() >= 400))
        cancel(frameLoader()->client().fileDoesNotExistError(response));
}

void NetscapePlugInStreamLoader::didReceiveData(const char* data, unsigned length, long long encodedDataLength, DataPayloadType dataPayloadType)
{
    didReceiveDataOrBuffer(data, length, 0, encodedDataLength, dataPayloadType);
}

void NetscapePlugInStreamLoader::didReceiveBuffer(PassRefPtr<SharedBuffer> buffer, long long encodedDataLength, DataPayloadType dataPayloadType)
{
    didReceiveDataOrBuffer(0, 0, buffer, encodedDataLength, dataPayloadType);
}

void NetscapePlugInStreamLoader::didReceiveDataOrBuffer(const char* data, int length, PassRefPtr<SharedBuffer> buffer, long long encodedDataLength, DataPayloadType dataPayloadType)
{
    Ref<NetscapePlugInStreamLoader> protect(*this);
    
    m_client->didReceiveData(this, buffer ? buffer->data() : data, buffer ? buffer->size() : length);

    ResourceLoader::didReceiveDataOrBuffer(data, length, buffer, encodedDataLength, dataPayloadType);
}

void NetscapePlugInStreamLoader::didFinishLoading(double finishTime)
{
    Ref<NetscapePlugInStreamLoader> protect(*this);

    notifyDone();

    m_client->didFinishLoading(this);
    ResourceLoader::didFinishLoading(finishTime);
}

void NetscapePlugInStreamLoader::didFail(const ResourceError& error)
{
    Ref<NetscapePlugInStreamLoader> protect(*this);

    notifyDone();

    m_client->didFail(this, error);
    ResourceLoader::didFail(error);
}

void NetscapePlugInStreamLoader::willCancel(const ResourceError& error)
{
    m_client->didFail(this, error);
}

void NetscapePlugInStreamLoader::didCancel(const ResourceError&)
{
    notifyDone();
}

void NetscapePlugInStreamLoader::notifyDone()
{
    if (!m_isInitialized)
        return;

    m_documentLoader->removePlugInStreamLoader(*this);
}


}
