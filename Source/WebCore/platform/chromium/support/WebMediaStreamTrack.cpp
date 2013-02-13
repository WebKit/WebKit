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

#include <public/WebMediaStreamTrack.h>

#include "MediaStreamComponent.h"
#include <public/WebMediaStream.h>
#include <public/WebMediaStreamSource.h>
#include <public/WebString.h>
#include <wtf/Vector.h>

using namespace WebCore;

namespace WebKit {

WebMediaStreamTrack::WebMediaStreamTrack(PassRefPtr<WebCore::MediaStreamComponent> mediaStreamComponent)
    : m_private(mediaStreamComponent)
{
}

WebMediaStreamTrack::WebMediaStreamTrack(WebCore::MediaStreamComponent* mediaStreamComponent)
    : m_private(mediaStreamComponent)
{
}

WebMediaStreamTrack& WebMediaStreamTrack::operator=(WebCore::MediaStreamComponent* mediaStreamComponent)
{
    m_private = mediaStreamComponent;
    return *this;
}

void WebMediaStreamTrack::initialize(const WebMediaStreamSource& source)
{
    m_private = MediaStreamComponent::create(source);
}

void WebMediaStreamTrack::initialize(const WebString& id, const WebMediaStreamSource& source)
{
    m_private = MediaStreamComponent::create(id, source);
}

void WebMediaStreamTrack::reset()
{
    m_private.reset();
}

WebMediaStreamTrack::operator PassRefPtr<MediaStreamComponent>() const
{
    return m_private.get();
}

WebMediaStreamTrack::operator MediaStreamComponent*() const
{
    return m_private.get();
}

bool WebMediaStreamTrack::isEnabled() const
{
    ASSERT(!m_private.isNull());
    return m_private->enabled();
}

WebString WebMediaStreamTrack::id() const
{
    ASSERT(!m_private.isNull());
    return m_private->id();
}

WebMediaStream WebMediaStreamTrack::stream() const
{
    ASSERT(!m_private.isNull());
    return WebMediaStream(m_private->stream());
}

WebMediaStreamSource WebMediaStreamTrack::source() const
{
    ASSERT(!m_private.isNull());
    return WebMediaStreamSource(m_private->source());
}

void WebMediaStreamTrack::assign(const WebMediaStreamTrack& other)
{
    m_private = other.m_private;
}

} // namespace WebKit

#endif // ENABLE(MEDIA_STREAM)
