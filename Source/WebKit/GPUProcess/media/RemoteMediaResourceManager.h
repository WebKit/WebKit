/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#if ENABLE(GPU_PROCESS)

#include "MessageReceiver.h"
#include "RemoteMediaResourceIdentifier.h"
#include <WebCore/PolicyChecker.h>
#include <wtf/HashMap.h>

namespace IPC {
class Connection;
class Decoder;
class DataReference;
}

namespace WebCore {
class ResourceRequest;
}

namespace WebKit {

class RemoteMediaResource;

class RemoteMediaResourceManager
    : public IPC::MessageReceiver {
    WTF_MAKE_FAST_ALLOCATED;
public:
    RemoteMediaResourceManager();
    ~RemoteMediaResourceManager();

    void addMediaResource(RemoteMediaResourceIdentifier, RemoteMediaResource&);
    void removeMediaResource(RemoteMediaResourceIdentifier);

    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;

private:
    void responseReceived(RemoteMediaResourceIdentifier, const WebCore::ResourceResponse&, bool, CompletionHandler<void(WebCore::PolicyChecker::ShouldContinue)>&&);
    void redirectReceived(RemoteMediaResourceIdentifier, WebCore::ResourceRequest&&, const WebCore::ResourceResponse&, CompletionHandler<void(WebCore::ResourceRequest&&)>&&);
    void dataSent(RemoteMediaResourceIdentifier, uint64_t, uint64_t);
    void dataReceived(RemoteMediaResourceIdentifier, const IPC::DataReference&);
    void accessControlCheckFailed(RemoteMediaResourceIdentifier, const WebCore::ResourceError&);
    void loadFailed(RemoteMediaResourceIdentifier, const WebCore::ResourceError&);
    void loadFinished(RemoteMediaResourceIdentifier);

    HashMap<RemoteMediaResourceIdentifier, RemoteMediaResource*> m_remoteMediaResources;
};

} // namespace WebKit

#endif
