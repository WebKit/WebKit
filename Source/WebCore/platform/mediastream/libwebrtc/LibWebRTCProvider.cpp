/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include "LibWebRTCProvider.h"

#if USE(LIBWEBRTC)

#include "LibWebRTCAudioModule.h"
#include <webrtc/api/peerconnectionfactory.h>
#include <webrtc/api/peerconnectionfactoryproxy.h>
#include <webrtc/base/physicalsocketserver.h>
#include <webrtc/p2p/client/basicportallocator.h>
#include <webrtc/sdk/objc/Framework/Classes/videotoolboxvideocodecfactory.h>
#include <wtf/Function.h>
#include <wtf/NeverDestroyed.h>

namespace WebCore {

struct PeerConnectionFactoryAndThreads : public rtc::MessageHandler {
    std::unique_ptr<LibWebRTCAudioModule> audioDeviceModule;
    std::unique_ptr<rtc::Thread> networkThread;
    std::unique_ptr<rtc::Thread> signalingThread;
    rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> factory;
    bool networkThreadWithSocketServer { false };
private:
    void OnMessage(rtc::Message*);
};

static inline PeerConnectionFactoryAndThreads& staticFactoryAndThreads()
{
    static NeverDestroyed<PeerConnectionFactoryAndThreads> factoryAndThreads;
    return factoryAndThreads.get();
}

struct ThreadMessageData : public rtc::MessageData {
    ThreadMessageData(Function<void()>&& callback)
        : callback(WTFMove(callback))
    { }
    Function<void()> callback;
};

void PeerConnectionFactoryAndThreads::OnMessage(rtc::Message* message)
{
    ASSERT(message->message_id == 1);
    static_cast<ThreadMessageData*>(message->pdata)->callback();
}

void LibWebRTCProvider::callOnWebRTCNetworkThread(Function<void()>&& callback)
{
    PeerConnectionFactoryAndThreads& threads = staticFactoryAndThreads();
    threads.networkThread->Post(RTC_FROM_HERE, &threads, 1, new ThreadMessageData(WTFMove(callback)));
}

void LibWebRTCProvider::callOnWebRTCSignalingThread(Function<void()>&& callback)
{
    PeerConnectionFactoryAndThreads& threads = staticFactoryAndThreads();
    threads.signalingThread->Post(RTC_FROM_HERE, &threads, 1, new ThreadMessageData(WTFMove(callback)));
}

static void initializePeerConnectionFactoryAndThreads()
{
    auto& factoryAndThreads = staticFactoryAndThreads();

    ASSERT(!factoryAndThreads.factory);

    auto thread = rtc::Thread::Create();
    factoryAndThreads.networkThread = factoryAndThreads.networkThreadWithSocketServer ? rtc::Thread::CreateWithSocketServer() : rtc::Thread::Create();
    bool result = factoryAndThreads.networkThread->Start();
    ASSERT_UNUSED(result, result);

    factoryAndThreads.signalingThread = rtc::Thread::Create();
    result = factoryAndThreads.signalingThread->Start();
    ASSERT(result);

    factoryAndThreads.audioDeviceModule = std::make_unique<LibWebRTCAudioModule>();

    factoryAndThreads.factory = webrtc::CreatePeerConnectionFactory(factoryAndThreads.networkThread.get(), factoryAndThreads.networkThread.get(), factoryAndThreads.signalingThread.get(), factoryAndThreads.audioDeviceModule.get(), new webrtc::VideoToolboxVideoEncoderFactory(), new webrtc::VideoToolboxVideoDecoderFactory());

    ASSERT(factoryAndThreads.factory);
}

webrtc::PeerConnectionFactoryInterface& LibWebRTCProvider::factory()
{
    if (!staticFactoryAndThreads().factory)
        initializePeerConnectionFactoryAndThreads();
    return *staticFactoryAndThreads().factory;
}

void LibWebRTCProvider::setPeerConnectionFactory(rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface>&& factory)
{
    if (!staticFactoryAndThreads().factory)
        initializePeerConnectionFactoryAndThreads();

    staticFactoryAndThreads().factory = webrtc::PeerConnectionFactoryProxy::Create(staticFactoryAndThreads().signalingThread.get(), WTFMove(factory));
}

static rtc::scoped_refptr<webrtc::PeerConnectionInterface> createActualPeerConnection(webrtc::PeerConnectionObserver& observer, std::unique_ptr<cricket::BasicPortAllocator>&& portAllocator)
{
    ASSERT(staticFactoryAndThreads().factory);

    webrtc::PeerConnectionInterface::RTCConfiguration config;
    // FIXME: Add a default configuration.
    return staticFactoryAndThreads().factory->CreatePeerConnection(config, WTFMove(portAllocator), nullptr, &observer);
}

rtc::scoped_refptr<webrtc::PeerConnectionInterface> LibWebRTCProvider::createPeerConnection(webrtc::PeerConnectionObserver& observer)
{
    // Default WK1 implementation.
    auto& factoryAndThreads = staticFactoryAndThreads();
    if (!factoryAndThreads.factory) {
        staticFactoryAndThreads().networkThreadWithSocketServer = true;
        initializePeerConnectionFactoryAndThreads();
    }
    ASSERT(staticFactoryAndThreads().networkThreadWithSocketServer);

    return createActualPeerConnection(observer, nullptr);
}

rtc::scoped_refptr<webrtc::PeerConnectionInterface> LibWebRTCProvider::createPeerConnection(webrtc::PeerConnectionObserver& observer, rtc::NetworkManager& networkManager, rtc::PacketSocketFactory& packetSocketFactory)
{
    ASSERT(!staticFactoryAndThreads().networkThreadWithSocketServer);

    auto& factoryAndThreads = staticFactoryAndThreads();
    if (!factoryAndThreads.factory)
        initializePeerConnectionFactoryAndThreads();

    std::unique_ptr<cricket::BasicPortAllocator> portAllocator;
    staticFactoryAndThreads().signalingThread->Invoke<void>(RTC_FROM_HERE, [&]() {
        portAllocator.reset(new cricket::BasicPortAllocator(&networkManager, &packetSocketFactory));
    });

    return createActualPeerConnection(observer, WTFMove(portAllocator));
}

} // namespace WebCore

#endif // USE(LIBWEBRTC)
