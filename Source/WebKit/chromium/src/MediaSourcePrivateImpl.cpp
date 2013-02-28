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
#include "MediaSourcePrivateImpl.h"

#if ENABLE(MEDIA_SOURCE)

#include "SourceBufferPrivateImpl.h"
#include "WebMediaSourceClient.h"
#include "WebSourceBuffer.h"
#include <wtf/PassOwnPtr.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

MediaSourcePrivateImpl::MediaSourcePrivateImpl(PassOwnPtr<WebKit::WebMediaSourceClient> client)
    : m_client(client)
{
}

WebCore::MediaSourcePrivate::AddStatus MediaSourcePrivateImpl::addSourceBuffer(const String& type, const CodecsArray& codecs,
    OwnPtr<WebCore::SourceBufferPrivate>* sourceBuffer)
{
    if (!m_client)
        return WebCore::MediaSourcePrivate::NotSupported;

    WebSourceBuffer* webSourceBuffer = 0;
    WebCore::MediaSourcePrivate::AddStatus result =
        static_cast<WebCore::MediaSourcePrivate::AddStatus>(m_client->addSourceBuffer(type, codecs, &webSourceBuffer));

    if (result == WebCore::MediaSourcePrivate::Ok) {
        ASSERT(webSourceBuffer);
        *sourceBuffer = adoptPtr(new SourceBufferPrivateImpl(adoptPtr(webSourceBuffer)));
    }
    return result;
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

}

#endif
