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

#include "platform/WebMediaStreamSource.h"

#include "MediaStreamSource.h"
#include "platform/WebString.h"
#include <wtf/Vector.h>

using namespace WebCore;

namespace WebKit {

WebMediaStreamSource::WebMediaStreamSource(const PassRefPtr<MediaStreamSource>& mediaStreamSource)
    : m_private(mediaStreamSource)
{
}

WebMediaStreamSource& WebMediaStreamSource::operator=(WebCore::MediaStreamSource* mediaStreamSource)
{
    m_private.reset();
    if (mediaStreamSource) {
        mediaStreamSource->ref();
        m_private = mediaStreamSource;
    }
    return *this;
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

} // namespace WebKit

#endif // ENABLE(MEDIA_STREAM)

