/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(MEDIA_STREAM)

#include <public/WebMediaStream.h>

#include "MediaStreamComponent.h"
#include "MediaStreamDescriptor.h"
#include "MediaStreamSource.h"
#include "UUID.h"
#include <public/WebMediaStreamSource.h>
#include <public/WebMediaStreamTrack.h>
#include <public/WebString.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/Vector.h>

using namespace WebCore;

namespace WebKit {

namespace {

class ExtraDataContainer : public MediaStreamDescriptor::ExtraData {
public:
    ExtraDataContainer(WebMediaStream::ExtraData* extraData) : m_extraData(adoptPtr(extraData)) { }

    WebMediaStream::ExtraData* extraData() { return m_extraData.get(); }

private:
    OwnPtr<WebMediaStream::ExtraData> m_extraData;
};

} // namespace

WebMediaStream::WebMediaStream(const PassRefPtr<WebCore::MediaStreamDescriptor>& mediaStreamDescriptor)
    : m_private(mediaStreamDescriptor)
{
}

WebMediaStream::WebMediaStream(WebCore::MediaStreamDescriptor* mediaStreamDescriptor)
    : m_private(mediaStreamDescriptor)
{
}

void WebMediaStream::reset()
{
    m_private.reset();
}

WebString WebMediaStream::label() const
{
    return m_private->id();
}

WebString WebMediaStream::id() const
{
    return m_private->id();
}

WebMediaStream::ExtraData* WebMediaStream::extraData() const
{
    RefPtr<MediaStreamDescriptor::ExtraData> data = m_private->extraData();
    if (!data)
        return 0;
    return static_cast<ExtraDataContainer*>(data.get())->extraData();
}

void WebMediaStream::setExtraData(ExtraData* extraData)
{
    m_private->setExtraData(adoptRef(new ExtraDataContainer(extraData)));
}

void WebMediaStream::audioTracks(WebVector<WebMediaStreamTrack>& webTracks) const
{
    size_t numberOfTracks = m_private->numberOfAudioComponents();
    WebVector<WebMediaStreamTrack> result(numberOfTracks);
    for (size_t i = 0; i < numberOfTracks; ++i)
        result[i] = m_private->audioComponent(i);
    webTracks.swap(result);
}

void WebMediaStream::videoTracks(WebVector<WebMediaStreamTrack>& webTracks) const
{
    size_t numberOfTracks = m_private->numberOfVideoComponents();
    WebVector<WebMediaStreamTrack> result(numberOfTracks);
    for (size_t i = 0; i < numberOfTracks; ++i)
        result[i] = m_private->videoComponent(i);
    webTracks.swap(result);
}

WebMediaStream& WebMediaStream::operator=(const PassRefPtr<WebCore::MediaStreamDescriptor>& mediaStreamDescriptor)
{
    m_private = mediaStreamDescriptor;
    return *this;
}

WebMediaStream::operator PassRefPtr<WebCore::MediaStreamDescriptor>() const
{
    return m_private.get();
}

WebMediaStream::operator WebCore::MediaStreamDescriptor*() const
{
    return m_private.get();
}

void WebMediaStream::initialize(const WebString& label, const WebVector<WebMediaStreamSource>& audioSources, const WebVector<WebMediaStreamSource>& videoSources)
{
    MediaStreamComponentVector audio, video;
    for (size_t i = 0; i < audioSources.size(); ++i) {
        MediaStreamSource* source = audioSources[i];
        audio.append(MediaStreamComponent::create(source->id(), source));
    }
    for (size_t i = 0; i < videoSources.size(); ++i) {
        MediaStreamSource* source = videoSources[i];
        video.append(MediaStreamComponent::create(source->id(), source));
    }
    m_private = MediaStreamDescriptor::create(label, audio, video);
}

void WebMediaStream::initialize(const WebVector<WebMediaStreamTrack>& audioTracks, const WebVector<WebMediaStreamTrack>& videoTracks)
{
    initialize(createCanonicalUUIDString(), audioTracks, videoTracks);
}

void WebMediaStream::initialize(const WebString& label, const WebVector<WebMediaStreamTrack>& audioTracks, const WebVector<WebMediaStreamTrack>& videoTracks)
{
    MediaStreamComponentVector audio, video;
    for (size_t i = 0; i < audioTracks.size(); ++i) {
        MediaStreamComponent* component = audioTracks[i];
        audio.append(component);
    }
    for (size_t i = 0; i < videoTracks.size(); ++i) {
        MediaStreamComponent* component = videoTracks[i];
        video.append(component);
    }
    m_private = MediaStreamDescriptor::create(label, audio, video);
}

void WebMediaStream::assign(const WebMediaStream& other)
{
    m_private = other.m_private;
}

} // namespace WebKit

#endif // ENABLE(MEDIA_STREAM)
