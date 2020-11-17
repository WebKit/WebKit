/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#if ENABLE(GPU_PROCESS) && ENABLE(MEDIA_SOURCE)

#include "GPUConnectionToWebProcess.h"
#include "MessageReceiver.h"
#include "RemoteSourceBufferIdentifier.h"
#include <WebCore/SourceBufferPrivate.h>
#include <WebCore/SourceBufferPrivateClient.h>
#include <wtf/Ref.h>
#include <wtf/text/AtomString.h>

namespace IPC {
class Connection;
class Decoder;
class DataReference;
}

namespace WebCore {
class MediaSample;
}

namespace WebKit {

class RemoteSourceBufferProxy final
    : public RefCounted<RemoteSourceBufferProxy>
    , public WebCore::SourceBufferPrivateClient
    , private IPC::MessageReceiver {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<RemoteSourceBufferProxy> create(RemoteSourceBufferIdentifier, GPUConnectionToWebProcess&, Ref<WebCore::SourceBufferPrivate>&&);
    virtual ~RemoteSourceBufferProxy();

    void sourceBufferPrivateDidReceiveInitializationSegment(const InitializationSegment&) final;
    void sourceBufferPrivateDidReceiveSample(WebCore::MediaSample&) final;
    bool sourceBufferPrivateHasAudio() const final;
    bool sourceBufferPrivateHasVideo() const final;

    void sourceBufferPrivateReenqueSamples(const AtomString& trackID) final;
    void sourceBufferPrivateDidBecomeReadyForMoreSamples(const AtomString& trackID) final;

    MediaTime sourceBufferPrivateFastSeekTimeForMediaTime(const MediaTime&, const MediaTime&, const MediaTime&) final;

    void sourceBufferPrivateAppendComplete(WebCore::SourceBufferPrivateClient::AppendResult) final;
    void sourceBufferPrivateDidReceiveRenderingError(int errorCode) final;

private:
    RemoteSourceBufferProxy(RemoteSourceBufferIdentifier, GPUConnectionToWebProcess&, Ref<WebCore::SourceBufferPrivate>&&);

    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;
    // void didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, std::unique_ptr<IPC::Encoder>&) final;

    void append(const IPC::DataReference&);

    RemoteSourceBufferIdentifier m_identifier;
    GPUConnectionToWebProcess& m_connectionToWebProcess;
    Ref<WebCore::SourceBufferPrivate> m_sourceBufferPrivate;
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS) && ENABLE(MEDIA_SOURCE)
