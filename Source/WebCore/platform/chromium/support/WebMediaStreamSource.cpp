/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(MEDIA_STREAM)

#include <public/WebMediaStreamSource.h>

#include "AudioBus.h"
#include "MediaStreamSource.h"
#include <public/WebAudioDestinationConsumer.h>
#include <public/WebString.h>
#include <wtf/MainThread.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/Vector.h>

using namespace WebCore;

namespace WebKit {

WebMediaStreamSource::WebMediaStreamSource(const PassRefPtr<MediaStreamSource>& mediaStreamSource)
    : m_private(mediaStreamSource)
{
}

WebMediaStreamSource& WebMediaStreamSource::operator=(WebCore::MediaStreamSource* mediaStreamSource)
{
    m_private = mediaStreamSource;
    return *this;
}

void WebMediaStreamSource::assign(const WebMediaStreamSource& other)
{
    m_private = other.m_private;
}

void WebMediaStreamSource::reset()
{
    m_private.reset();
}

WebMediaStreamSource::operator PassRefPtr<MediaStreamSource>() const
{
    return m_private.get();
}

WebMediaStreamSource::operator MediaStreamSource*() const
{
    return m_private.get();
}

void WebMediaStreamSource::initialize(const WebString& id, Type type, const WebString& name)
{
    m_private = MediaStreamSource::create(id, static_cast<MediaStreamSource::Type>(type), name);
}

WebString WebMediaStreamSource::id() const
{
    ASSERT(!m_private.isNull());
    return m_private.get()->id();
}

WebMediaStreamSource::Type WebMediaStreamSource::type() const
{
    ASSERT(!m_private.isNull());
    return static_cast<Type>(m_private.get()->type());
}

WebString WebMediaStreamSource::name() const
{
    ASSERT(!m_private.isNull());
    return m_private.get()->name();
}

void WebMediaStreamSource::setReadyState(ReadyState state)
{
    ASSERT(!m_private.isNull());
    m_private->setReadyState(static_cast<MediaStreamSource::ReadyState>(state));
}

WebMediaStreamSource::ReadyState WebMediaStreamSource::readyState() const
{
    ASSERT(!m_private.isNull());
    return static_cast<ReadyState>(m_private->readyState());
}

class ExtraDataContainer : public WebCore::MediaStreamSource::ExtraData {
public:
    ExtraDataContainer(WebMediaStreamSource::ExtraData* extraData) : m_extraData(WTF::adoptPtr(extraData)) { }

    WebMediaStreamSource::ExtraData* extraData() { return m_extraData.get(); }

private:
    OwnPtr<WebMediaStreamSource::ExtraData> m_extraData;
};

WebMediaStreamSource::ExtraData* WebMediaStreamSource::extraData() const
{
    ASSERT(!m_private.isNull());
    RefPtr<MediaStreamSource::ExtraData> data = m_private->extraData();
    if (!data)
        return 0;
    return static_cast<ExtraDataContainer*>(data.get())->extraData();
}

void WebMediaStreamSource::setExtraData(ExtraData* extraData)
{
    ASSERT(!m_private.isNull());
    m_private->setExtraData(adoptRef(new ExtraDataContainer(extraData)));
}

bool WebMediaStreamSource::requiresAudioConsumer() const
{
    ASSERT(!m_private.isNull());
    return m_private->requiresAudioConsumer();
}

class ConsumerWrapper : public WebCore::AudioDestinationConsumer {
public:
    static PassRefPtr<ConsumerWrapper> create(WebAudioDestinationConsumer* consumer)
    {
        return adoptRef(new ConsumerWrapper(consumer));
    }

    virtual void setFormat(size_t numberOfChannels, float sampleRate) OVERRIDE;
    virtual void consumeAudio(AudioBus*, size_t numberOfFrames) OVERRIDE;

    WebAudioDestinationConsumer* consumer() { return m_consumer; }

private:
    explicit ConsumerWrapper(WebAudioDestinationConsumer* consumer) : m_consumer(consumer) { }

    // m_consumer is not owned by this class.
    WebAudioDestinationConsumer* m_consumer;
};

void ConsumerWrapper::setFormat(size_t numberOfChannels, float sampleRate)
{
    m_consumer->setFormat(numberOfChannels, sampleRate);
}

void ConsumerWrapper::consumeAudio(AudioBus* bus, size_t numberOfFrames)
{
    if (!bus)
        return;

    // Wrap AudioBus.
    size_t numberOfChannels = bus->numberOfChannels();
    WebKit::WebVector<const float*> busVector(numberOfChannels);
    for (size_t i = 0; i < numberOfChannels; ++i)
        busVector[i] = bus->channel(i)->data();

    m_consumer->consumeAudio(busVector, numberOfFrames);
}

void WebMediaStreamSource::addAudioConsumer(WebAudioDestinationConsumer* consumer)
{
    ASSERT(isMainThread());
    ASSERT(!m_private.isNull() && consumer);

    m_private->addAudioConsumer(ConsumerWrapper::create(consumer));
}

bool WebMediaStreamSource::removeAudioConsumer(WebAudioDestinationConsumer* consumer)
{
    ASSERT(isMainThread());
    ASSERT(!m_private.isNull() && consumer);

    const Vector<RefPtr<AudioDestinationConsumer> >& consumers = m_private->audioConsumers();
    for (Vector<RefPtr<AudioDestinationConsumer> >::const_iterator it = consumers.begin(); it != consumers.end(); ++it) {
        ConsumerWrapper* wrapper = static_cast<ConsumerWrapper*>((*it).get());
        if (wrapper->consumer() == consumer) {
            m_private->removeAudioConsumer(wrapper);
            return true;
        }
    }
    return false;
}

} // namespace WebKit

#endif // ENABLE(MEDIA_STREAM)

