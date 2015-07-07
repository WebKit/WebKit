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

#if ENABLE(MEDIA_STREAM)

#include "MediaStreamAudioSource.h"

#include "NotImplemented.h"
#include "UUID.h"

namespace WebCore {

RefPtr<MediaStreamAudioSource> MediaStreamAudioSource::create()
{
    return adoptRef(new MediaStreamAudioSource());
}

MediaStreamAudioSource::MediaStreamAudioSource()
    : MediaStreamSource(ASCIILiteral("WebAudio-") + createCanonicalUUIDString(), MediaStreamSource::Audio, "MediaStreamAudioDestinationNode")
{
}

RefPtr<MediaStreamSourceCapabilities> MediaStreamAudioSource::capabilities() const
{
    // FIXME: implement this.
    // https://bugs.webkit.org/show_bug.cgi?id=122430
    notImplemented();
    return nullptr;
}

const MediaStreamSourceStates& MediaStreamAudioSource::states()
{
    // FIXME: implement this.
    // https://bugs.webkit.org/show_bug.cgi?id=122430
    notImplemented();
    return m_currentStates;
    
}

void MediaStreamAudioSource::addAudioConsumer(PassRefPtr<AudioDestinationConsumer> consumer)
{
    MutexLocker locker(m_audioConsumersLock);
    m_audioConsumers.append(consumer);
}

bool MediaStreamAudioSource::removeAudioConsumer(AudioDestinationConsumer* consumer)
{
    MutexLocker locker(m_audioConsumersLock);
    size_t pos = m_audioConsumers.find(consumer);
    if (pos != notFound) {
        m_audioConsumers.remove(pos);
        return true;
    }
    return false;
}

void MediaStreamAudioSource::setAudioFormat(size_t numberOfChannels, float sampleRate)
{
    MutexLocker locker(m_audioConsumersLock);
    for (Vector<RefPtr<AudioDestinationConsumer>>::iterator it = m_audioConsumers.begin(); it != m_audioConsumers.end(); ++it)
        (*it)->setFormat(numberOfChannels, sampleRate);
}

void MediaStreamAudioSource::consumeAudio(AudioBus* bus, size_t numberOfFrames)
{
    MutexLocker locker(m_audioConsumersLock);
    for (Vector<RefPtr<AudioDestinationConsumer>>::iterator it = m_audioConsumers.begin(); it != m_audioConsumers.end(); ++it)
        (*it)->consumeAudio(bus, numberOfFrames);
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
