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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "BaseAudioSharedUnit.h"

#if ENABLE(MEDIA_STREAM)

#include "CoreAudioCaptureSource.h"
#include "Logging.h"

namespace WebCore {

void BaseAudioSharedUnit::addClient(CoreAudioCaptureSource& client)
{
    auto locker = holdLock(m_clientsLock);
    m_clients.add(&client);
}

void BaseAudioSharedUnit::removeClient(CoreAudioCaptureSource& client)
{
    auto locker = holdLock(m_clientsLock);
    m_clients.remove(&client);
}

void BaseAudioSharedUnit::forEachClient(const Function<void(CoreAudioCaptureSource&)>& apply) const
{
    Vector<CoreAudioCaptureSource*> clientsCopy;
    {
        auto locker = holdLock(m_clientsLock);
        clientsCopy = copyToVector(m_clients);
    }
    for (auto* client : clientsCopy) {
        auto locker = holdLock(m_clientsLock);
        // Make sure the client has not been destroyed.
        if (!m_clients.contains(client))
            continue;
        apply(*client);
    }
}

void BaseAudioSharedUnit::clearClients()
{
    auto locker = holdLock(m_clientsLock);
    m_clients.clear();
}

void BaseAudioSharedUnit::startProducingData()
{
    ASSERT(isMainThread());

    if (++m_producingCount != 1)
        return;

    if (isProducingData())
        return;

    if (hasAudioUnit()) {
        cleanupAudioUnit();
        ASSERT(!hasAudioUnit());
    }

    if (startInternal())
        captureFailed();
}

OSStatus BaseAudioSharedUnit::resume()
{
    ASSERT(isMainThread());
    ASSERT(m_suspended);
    ASSERT(!isProducingData());

    m_suspended = false;

    if (!hasAudioUnit())
        return 0;

    startInternal();

    return 0;
}

void BaseAudioSharedUnit::prepareForNewCapture()
{
    if (!m_suspended)
        return;
    m_suspended = false;

    if (!m_producingCount)
        return;

    RELEASE_LOG_ERROR(WebRTC, "CoreAudioSharedUnit::prepareForNewCapture, notifying suspended sources of capture failure");
    captureFailed();
}

void BaseAudioSharedUnit::captureFailed()
{
    RELEASE_LOG_ERROR(WebRTC, "CoreAudioSharedUnit::captureFailed - capture failed");
    forEachClient([](auto& client) {
        client.captureFailed();
    });

    m_producingCount = 0;

    clearClients();

    stopInternal();
    cleanupAudioUnit();
}

void BaseAudioSharedUnit::stopProducingData()
{
    ASSERT(isMainThread());
    ASSERT(m_producingCount);

    if (m_producingCount && --m_producingCount)
        return;

    stopInternal();
    cleanupAudioUnit();
}

OSStatus BaseAudioSharedUnit::suspend()
{
    ASSERT(isMainThread());

    m_suspended = true;
    stopInternal();

    return 0;
}

void BaseAudioSharedUnit::audioSamplesAvailable(const MediaTime& time, const PlatformAudioData& data, const AudioStreamDescription& description, size_t numberOfFrames)
{
    forEachClient([&](auto& client) {
        if (client.isProducingData())
            client.audioSamplesAvailable(time, data, description, numberOfFrames);
    });
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
