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
#include "Logging.h"
#include "VideoToolBoxEncoderFactory.h"
#include <dlfcn.h>
#include <webrtc/api/peerconnectionfactoryproxy.h>
#include <webrtc/base/physicalsocketserver.h>
#include <webrtc/p2p/client/basicportallocator.h>
#include <webrtc/pc/peerconnectionfactory.h>
#include <webrtc/sdk/objc/Framework/Classes/VideoToolbox/videocodecfactory.h>
#include <wtf/Function.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/darwin/WeakLinking.h>
#endif

namespace WebCore {

#if USE(LIBWEBRTC)
struct PeerConnectionFactoryAndThreads : public rtc::MessageHandler {
    std::unique_ptr<LibWebRTCAudioModule> audioDeviceModule;
    std::unique_ptr<rtc::Thread> networkThread;
    std::unique_ptr<rtc::Thread> signalingThread;
    rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> factory;
    bool networkThreadWithSocketServer { false };
    Function<std::unique_ptr<cricket::WebRtcVideoEncoderFactory>()> encoderFactoryGetter;
    Function<std::unique_ptr<cricket::WebRtcVideoDecoderFactory>()> decoderFactoryGetter;

private:
    void OnMessage(rtc::Message*);
};

static inline PeerConnectionFactoryAndThreads& staticFactoryAndThreads()
{
    static NeverDestroyed<PeerConnectionFactoryAndThreads> factoryAndThreads;
#if PLATFORM(COCOA)
    static std::once_flag once;
    std::call_once(once, [] {
        factoryAndThreads.get().encoderFactoryGetter = []() -> std::unique_ptr<cricket::WebRtcVideoEncoderFactory> { return std::make_unique<VideoToolboxVideoEncoderFactory>(); };
        factoryAndThreads.get().decoderFactoryGetter = []() -> std::unique_ptr<cricket::WebRtcVideoDecoderFactory> { return std::make_unique<webrtc::VideoToolboxVideoDecoderFactory>(); };
    });
#endif
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
    auto* data = static_cast<ThreadMessageData*>(message->pdata);
    data->callback();
    delete data;
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
#if defined(NDEBUG)
#if !LOG_DISABLED || !RELEASE_LOG_DISABLED
    rtc::LogMessage::LogToDebug(LogWebRTC.state != WTFLogChannelOn ? rtc::LS_NONE : rtc::LS_INFO);
#else
    rtc::LogMessage::LogToDebug(rtc::LS_NONE);
#endif
#else
    rtc::LogMessage::LogToDebug(LogWebRTC.state != WTFLogChannelOn ? rtc::LS_WARNING : rtc::LS_INFO);
#endif
    auto& factoryAndThreads = staticFactoryAndThreads();

    ASSERT(!factoryAndThreads.factory);

    factoryAndThreads.networkThread = factoryAndThreads.networkThreadWithSocketServer ? rtc::Thread::CreateWithSocketServer() : rtc::Thread::Create();
    factoryAndThreads.networkThread->SetName("WebKitWebRTCNetwork", nullptr);
    bool result = factoryAndThreads.networkThread->Start();
    ASSERT_UNUSED(result, result);

    factoryAndThreads.signalingThread = rtc::Thread::Create();
    factoryAndThreads.signalingThread->SetName("WebKitWebRTCSignaling", nullptr);
    
    result = factoryAndThreads.signalingThread->Start();
    ASSERT(result);

    factoryAndThreads.audioDeviceModule = std::make_unique<LibWebRTCAudioModule>();

    std::unique_ptr<cricket::WebRtcVideoEncoderFactory> encoderFactory = factoryAndThreads.encoderFactoryGetter ? factoryAndThreads.encoderFactoryGetter() : nullptr;
    std::unique_ptr<cricket::WebRtcVideoDecoderFactory> decoderFactory = factoryAndThreads.decoderFactoryGetter ? factoryAndThreads.decoderFactoryGetter() : nullptr;

    factoryAndThreads.factory = webrtc::CreatePeerConnectionFactory(factoryAndThreads.networkThread.get(), factoryAndThreads.networkThread.get(), factoryAndThreads.signalingThread.get(), factoryAndThreads.audioDeviceModule.get(), encoderFactory.release(), decoderFactory.release());

    ASSERT(factoryAndThreads.factory);
}

webrtc::PeerConnectionFactoryInterface* LibWebRTCProvider::factory()
{
    if (!webRTCAvailable())
        return nullptr;
    if (!staticFactoryAndThreads().factory) {
        staticFactoryAndThreads().networkThreadWithSocketServer = m_useNetworkThreadWithSocketServer;
        initializePeerConnectionFactoryAndThreads();
    }
    return staticFactoryAndThreads().factory;
}

void LibWebRTCProvider::setDecoderFactoryGetter(Function<std::unique_ptr<cricket::WebRtcVideoDecoderFactory>()>&& getter)
{
    if (!staticFactoryAndThreads().factory)
        initializePeerConnectionFactoryAndThreads();

    staticFactoryAndThreads().decoderFactoryGetter = WTFMove(getter);
}

void LibWebRTCProvider::setEncoderFactoryGetter(Function<std::unique_ptr<cricket::WebRtcVideoEncoderFactory>()>&& getter)
{
    if (!staticFactoryAndThreads().factory)
        initializePeerConnectionFactoryAndThreads();

    staticFactoryAndThreads().encoderFactoryGetter = WTFMove(getter);
}

void LibWebRTCProvider::setPeerConnectionFactory(rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface>&& factory)
{
    if (!staticFactoryAndThreads().factory)
        initializePeerConnectionFactoryAndThreads();

    staticFactoryAndThreads().factory = webrtc::PeerConnectionFactoryProxy::Create(staticFactoryAndThreads().signalingThread.get(), WTFMove(factory));
}

static rtc::scoped_refptr<webrtc::PeerConnectionInterface> createActualPeerConnection(webrtc::PeerConnectionObserver& observer, std::unique_ptr<cricket::BasicPortAllocator>&& portAllocator, webrtc::PeerConnectionInterface::RTCConfiguration&& configuration)
{
    ASSERT(staticFactoryAndThreads().factory);

    return staticFactoryAndThreads().factory->CreatePeerConnection(configuration, WTFMove(portAllocator), nullptr, &observer);
}

rtc::scoped_refptr<webrtc::PeerConnectionInterface> LibWebRTCProvider::createPeerConnection(webrtc::PeerConnectionObserver& observer, webrtc::PeerConnectionInterface::RTCConfiguration&& configuration)
{
    // Default WK1 implementation.
    auto& factoryAndThreads = staticFactoryAndThreads();
    if (!factoryAndThreads.factory) {
        staticFactoryAndThreads().networkThreadWithSocketServer = true;
        initializePeerConnectionFactoryAndThreads();
    }
    ASSERT(staticFactoryAndThreads().networkThreadWithSocketServer);

    return createActualPeerConnection(observer, nullptr, WTFMove(configuration));
}

rtc::scoped_refptr<webrtc::PeerConnectionInterface> LibWebRTCProvider::createPeerConnection(webrtc::PeerConnectionObserver& observer, rtc::NetworkManager& networkManager, rtc::PacketSocketFactory& packetSocketFactory, webrtc::PeerConnectionInterface::RTCConfiguration&& configuration)
{
    ASSERT(!staticFactoryAndThreads().networkThreadWithSocketServer);

    auto& factoryAndThreads = staticFactoryAndThreads();
    if (!factoryAndThreads.factory)
        initializePeerConnectionFactoryAndThreads();

    std::unique_ptr<cricket::BasicPortAllocator> portAllocator;
    staticFactoryAndThreads().signalingThread->Invoke<void>(RTC_FROM_HERE, [&]() {
        auto basicPortAllocator = std::make_unique<cricket::BasicPortAllocator>(&networkManager, &packetSocketFactory);
        if (!m_enableEnumeratingAllNetworkInterfaces)
            basicPortAllocator->set_flags(basicPortAllocator->flags() | cricket::PORTALLOCATOR_DISABLE_ADAPTER_ENUMERATION);
        portAllocator = WTFMove(basicPortAllocator);
    });

    return createActualPeerConnection(observer, WTFMove(portAllocator), WTFMove(configuration));
}

#endif // USE(LIBWEBRTC)

bool LibWebRTCProvider::webRTCAvailable()
{
#if USE(LIBWEBRTC)
    return !isNullFunctionPointer(rtc::LogMessage::LogToDebug);
#else
    return true;
#endif
}

} // namespace WebCore
