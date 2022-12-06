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

#include "AudioMediaStreamTrackRendererInternalUnitIdentifier.h"
#include "Connection.h"
#include "MediaOverridesForTesting.h"
#include "MessageReceiverMap.h"
#include "SharedMemory.h"
#include <WebCore/AudioSession.h>
#include <WebCore/PlatformMediaSession.h>
#include <wtf/RefCounted.h>
#include <wtf/WeakHashSet.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/WTFString.h>

namespace WebCore {
class CAAudioStreamDescription;
}

namespace IPC {
class Semaphore;
}

namespace WebKit {

class RemoteAudioSourceProviderManager;
class RemoteMediaPlayerManager;
class SampleBufferDisplayLayerManager;
class WebPage;
struct GPUProcessConnectionInfo;
struct OverrideScreenDataForTesting;
struct WebPageCreationParameters;

#if ENABLE(VIDEO)
class RemoteVideoFrameObjectHeapProxy;
#endif

class GPUProcessConnection : public RefCounted<GPUProcessConnection>, public IPC::Connection::Client {
public:
    static RefPtr<GPUProcessConnection> create(IPC::Connection& parentConnection);
    ~GPUProcessConnection();
    
    IPC::Connection& connection() { return m_connection.get(); }
    IPC::MessageReceiverMap& messageReceiverMap() { return m_messageReceiverMap; }

#if HAVE(AUDIT_TOKEN)
    std::optional<audit_token_t> auditToken();
#endif
#if PLATFORM(COCOA) && ENABLE(MEDIA_STREAM)
    SampleBufferDisplayLayerManager& sampleBufferDisplayLayerManager();
    void resetAudioMediaStreamTrackRendererInternalUnit(AudioMediaStreamTrackRendererInternalUnitIdentifier);
#endif
#if ENABLE(VIDEO)
    RemoteVideoFrameObjectHeapProxy& videoFrameObjectHeapProxy();
#endif

    RemoteMediaPlayerManager& mediaPlayerManager();

#if PLATFORM(COCOA) && ENABLE(WEB_AUDIO)
    RemoteAudioSourceProviderManager& audioSourceProviderManager();
#endif

    void updateMediaConfiguration();

#if ENABLE(VP9)
    void enableVP9Decoders(bool enableVP8Decoder, bool enableVP9Decoder, bool enableVP9SWDecoder);

    bool isVP8DecoderEnabled() const { return m_enableVP8Decoder; }
    bool isVP9DecoderEnabled() const { return m_enableVP9Decoder; }
    bool isVPSWDecoderEnabled() const { return m_enableVP9SWDecoder; }
    bool hasVP9HardwareDecoder();
    bool hasVP9ExtensionSupport();
#endif

#if HAVE(VISIBILITY_PROPAGATION_VIEW)
    void createVisibilityPropagationContextForPage(WebPage&);
    void destroyVisibilityPropagationContextForPage(WebPage&);
#endif

    void configureLoggingChannel(const String&, WTFLogChannelState, WTFLogLevel);

    class Client : public CanMakeWeakPtr<Client> {
    public:
        virtual ~Client() = default;

        virtual void gpuProcessConnectionDidClose(GPUProcessConnection&) { }
    };
    void addClient(const Client& client) { m_clients.add(client); }
    void removeClient(const Client& client) { m_clients.remove(client); }

    static constexpr Seconds defaultTimeout = 3_s;
private:
    GPUProcessConnection(IPC::Connection::Identifier&&);
    bool waitForDidInitialize();
    void invalidate();

    // IPC::Connection::Client
    void didClose(IPC::Connection&) override;
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;
    bool didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, UniqueRef<IPC::Encoder>&) final;
    void didReceiveInvalidMessage(IPC::Connection&, IPC::MessageName) override;

    bool dispatchMessage(IPC::Connection&, IPC::Decoder&);
    bool dispatchSyncMessage(IPC::Connection&, IPC::Decoder&, UniqueRef<IPC::Encoder>&);

    // Messages.
    void didReceiveRemoteCommand(WebCore::PlatformMediaSession::RemoteControlCommandType, const WebCore::PlatformMediaSession::RemoteCommandArgument&);
    void didInitialize(std::optional<GPUProcessConnectionInfo>&&);

#if ENABLE(ROUTING_ARBITRATION)
    void beginRoutingArbitrationWithCategory(WebCore::AudioSession::CategoryType, WebCore::AudioSessionRoutingArbitrationClient::ArbitrationCallback&&);
    void endRoutingArbitration();
#endif

    // The connection from the web process to the GPU process.
    Ref<IPC::Connection> m_connection;
    IPC::MessageReceiverMap m_messageReceiverMap;
    bool m_hasInitialized { false };
#if HAVE(AUDIT_TOKEN)
    std::optional<audit_token_t> m_auditToken;
#endif
#if PLATFORM(COCOA) && ENABLE(MEDIA_STREAM)
    std::unique_ptr<SampleBufferDisplayLayerManager> m_sampleBufferDisplayLayerManager;
#endif
#if ENABLE(VIDEO)
    RefPtr<RemoteVideoFrameObjectHeapProxy> m_videoFrameObjectHeapProxy;
#endif
#if PLATFORM(COCOA) && ENABLE(WEB_AUDIO)
    RefPtr<RemoteAudioSourceProviderManager> m_audioSourceProviderManager;
#endif
#if ENABLE(VP9)
    bool m_enableVP8Decoder { false };
    bool m_enableVP9Decoder { false };
    bool m_enableVP9SWDecoder { false };
    bool m_hasVP9HardwareDecoder { false };
    bool m_hasVP9ExtensionSupport { false };
#endif

#if PLATFORM(COCOA)
    MediaOverridesForTesting m_mediaOverridesForTesting;
#endif

    WeakHashSet<Client> m_clients;
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
