/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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
#include "WebMediaSourceImpl.h"

#include "MediaSourcePrivate.h"
#include "WebMediaSourceClient.h"

#if ENABLE(MEDIA_SOURCE)

namespace WebKit {

class MediaSourcePrivateImpl : public WebCore::MediaSourcePrivate {
public:
    explicit MediaSourcePrivateImpl(PassOwnPtr<WebMediaSourceClient>);
    virtual ~MediaSourcePrivateImpl() { }

    // MediaSourcePrivate methods.
    virtual WebCore::MediaSourcePrivate::AddIdStatus addId(const String& id, const String& type, const Vector<String>& codecs);
    virtual bool removeId(const String& id);
    virtual PassRefPtr<WebCore::TimeRanges> buffered(const String& id);
    virtual bool append(const String& id, const unsigned char* data, unsigned length);
    virtual bool abort(const String& id);
    virtual double duration();
    virtual void setDuration(double);
    virtual void endOfStream(WebCore::MediaSourcePrivate::EndOfStreamStatus);
    virtual bool setTimestampOffset(const String& id, double offset);

private:
    OwnPtr<WebKit::WebMediaSourceClient> m_client;
};

MediaSourcePrivateImpl::MediaSourcePrivateImpl(PassOwnPtr<WebKit::WebMediaSourceClient> client)
    : m_client(client)
{
}

WebCore::MediaSourcePrivate::AddIdStatus MediaSourcePrivateImpl::addId(const String& id, const String& type, const Vector<String>& codecs)
{
    if (!m_client)
        return WebCore::MediaSourcePrivate::NotSupported;

    return static_cast<WebCore::MediaSourcePrivate::AddIdStatus>(m_client->addId(id, type, codecs));
}

bool MediaSourcePrivateImpl::removeId(const String& id)
{
    if (!m_client)
        return false;

    return m_client->removeId(id);
}

PassRefPtr<WebCore::TimeRanges> MediaSourcePrivateImpl::buffered(const String& id)
{
    if (!m_client)
        return WebCore::TimeRanges::create();

    WebTimeRanges webRanges = m_client->buffered(id);
    RefPtr<WebCore::TimeRanges> ranges = WebCore::TimeRanges::create();
    for (size_t i = 0; i < webRanges.size(); ++i)
        ranges->add(webRanges[i].start, webRanges[i].end);
    return ranges.release();
}

bool MediaSourcePrivateImpl::append(const String& id, const unsigned char* data, unsigned length)
{
    if (!m_client)
        return false;

    return m_client->append(id, data, length);
}

bool MediaSourcePrivateImpl::abort(const String& id)
{
    if (!m_client)
        return false;

    return m_client->abort(id);
}

double MediaSourcePrivateImpl::duration()
{
    if (!m_client)
        return std::numeric_limits<float>::quiet_NaN();
    return m_client->duration();
}

void MediaSourcePrivateImpl::setDuration(double duration)
{
    if (m_client)
        m_client->setDuration(duration);
}

void MediaSourcePrivateImpl::endOfStream(WebCore::MediaSourcePrivate::EndOfStreamStatus status)
{
    if (m_client)
        m_client->endOfStream(static_cast<WebMediaSourceClient::EndOfStreamStatus>(status));
}

bool MediaSourcePrivateImpl::setTimestampOffset(const String& id, double offset)
{
    if (!m_client)
        return false;
    return m_client->setTimestampOffset(id, offset);
}


WebMediaSourceImpl::WebMediaSourceImpl(PassRefPtr<WebCore::MediaSource> mediaSource)
{
    m_mediaSource = mediaSource;
}

WebMediaSourceImpl::~WebMediaSourceImpl()
{
}


void WebMediaSourceImpl::open(WebMediaSourceClient* client)
{
    ASSERT(client);
    m_mediaSource->setPrivateAndOpen(adoptPtr(new MediaSourcePrivateImpl(adoptPtr(client))));
}

}

#endif
