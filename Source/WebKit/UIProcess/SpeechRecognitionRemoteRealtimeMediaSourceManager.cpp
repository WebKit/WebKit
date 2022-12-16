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

#include "config.h"
#include "SpeechRecognitionRemoteRealtimeMediaSourceManager.h"

#if ENABLE(MEDIA_STREAM)

#include "SpeechRecognitionRealtimeMediaSourceManagerMessages.h"
#include "SpeechRecognitionRemoteRealtimeMediaSource.h"

namespace WebKit {

SpeechRecognitionRemoteRealtimeMediaSourceManager::SpeechRecognitionRemoteRealtimeMediaSourceManager(Ref<IPC::Connection>&& connection)
    : m_connection(WTFMove(connection))
{
}

void SpeechRecognitionRemoteRealtimeMediaSourceManager::addSource(SpeechRecognitionRemoteRealtimeMediaSource& source, const WebCore::CaptureDevice& captureDevice)
{
    auto identifier = source.identifier();
    ASSERT(!m_sources.contains(identifier));
    m_sources.add(identifier, source);

#if ENABLE(SANDBOX_EXTENSIONS)
    if (!captureDevice.isMockDevice()) {
        m_sourcesNeedingSandboxExtension.add(identifier);
        if (m_sourcesNeedingSandboxExtension.size() == 1) {
            auto machBootstrapHandle = SandboxExtension::createHandleForMachBootstrapExtension();
            SandboxExtension::Handle handleForTCCD;
            if (auto handle = SandboxExtension::createHandleForMachLookup("com.apple.tccd"_s, m_connection->getAuditToken()))
                handleForTCCD = WTFMove(*handle);
            SandboxExtension::Handle handleForMicrophone;
            if (auto handle = SandboxExtension::createHandleForGenericExtension("com.apple.webkit.microphone"_s))
                handleForMicrophone = WTFMove(*handle);
            send(Messages::SpeechRecognitionRealtimeMediaSourceManager::GrantSandboxExtensions(machBootstrapHandle, handleForTCCD, handleForMicrophone));
        }
    }
#endif

    send(Messages::SpeechRecognitionRealtimeMediaSourceManager::CreateSource(identifier, captureDevice, source.pageIdentifier()));
}

void SpeechRecognitionRemoteRealtimeMediaSourceManager::removeSource(SpeechRecognitionRemoteRealtimeMediaSource& source)
{
    auto identifier = source.identifier();
    ASSERT(m_sources.get(identifier) == &source);
    m_sources.remove(identifier);

#if ENABLE(SANDBOX_EXTENSIONS)
    if (m_sourcesNeedingSandboxExtension.remove(identifier)) {
        if (m_sourcesNeedingSandboxExtension.isEmpty())
            send(Messages::SpeechRecognitionRealtimeMediaSourceManager::RevokeSandboxExtensions());
    }
#endif

    send(Messages::SpeechRecognitionRealtimeMediaSourceManager::DeleteSource(identifier));
}

void SpeechRecognitionRemoteRealtimeMediaSourceManager::remoteAudioSamplesAvailable(WebCore::RealtimeMediaSourceIdentifier identifier, const WTF::MediaTime& time, uint64_t numberOfFrames)
{
    if (auto source = m_sources.get(identifier))
        source->remoteAudioSamplesAvailable(time, numberOfFrames);
}

void SpeechRecognitionRemoteRealtimeMediaSourceManager::remoteCaptureFailed(WebCore::RealtimeMediaSourceIdentifier identifier)
{
    if (auto source = m_sources.get(identifier))
        source->remoteCaptureFailed();
}

void SpeechRecognitionRemoteRealtimeMediaSourceManager::remoteSourceStopped(WebCore::RealtimeMediaSourceIdentifier identifier)
{
    if (auto source = m_sources.get(identifier))
        source->remoteSourceStopped();
}

IPC::Connection* SpeechRecognitionRemoteRealtimeMediaSourceManager::messageSenderConnection() const
{
    return m_connection.ptr();
}

uint64_t SpeechRecognitionRemoteRealtimeMediaSourceManager::messageSenderDestinationID() const
{
    return 0;
}

#if PLATFORM(COCOA)

void SpeechRecognitionRemoteRealtimeMediaSourceManager::setStorage(WebCore::RealtimeMediaSourceIdentifier identifier, ConsumerSharedCARingBuffer::Handle&& handle, const WebCore::CAAudioStreamDescription& description)
{
    if (auto source = m_sources.get(identifier))
        source->setStorage(WTFMove(handle), description);
}

#endif

} // namespace WebKit

#endif
