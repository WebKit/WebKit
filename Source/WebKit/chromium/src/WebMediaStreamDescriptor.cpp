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

#include "platform/WebMediaStreamDescriptor.h"

#include "MediaStreamComponent.h"
#include "MediaStreamDescriptor.h"
#include "MediaStreamSource.h"
#include "WebMediaStreamSource.h"
#include "platform/WebString.h"
#include <wtf/Vector.h>

using namespace WebCore;

namespace WebKit {

WebMediaStreamDescriptor::WebMediaStreamDescriptor(const PassRefPtr<WebCore::MediaStreamDescriptor>& mediaStreamDescriptor)
    : m_private(mediaStreamDescriptor)
{
}

void WebMediaStreamDescriptor::reset()
{
    m_private.reset();
}

WebString WebMediaStreamDescriptor::label() const
{
    return m_private->label();
}

void WebMediaStreamDescriptor::sources(WebVector<WebMediaStreamSource>& webSources) const
{
    size_t numberOfSources = m_private->numberOfComponents();
    WebVector<WebMediaStreamSource> result(numberOfSources);
    for (size_t i = 0; i < numberOfSources; ++i)
        result[i] =  m_private->component(i)->source();
    webSources.swap(result);
}

WebMediaStreamDescriptor& WebMediaStreamDescriptor::operator=(const PassRefPtr<WebCore::MediaStreamDescriptor>& mediaStreamDescriptor)
{
    m_private = mediaStreamDescriptor;
    return *this;
}

WebMediaStreamDescriptor::operator PassRefPtr<WebCore::MediaStreamDescriptor>() const
{
    return m_private.get();
}

WebMediaStreamDescriptor::operator WebCore::MediaStreamDescriptor*() const
{
    return m_private.get();
}

void WebMediaStreamDescriptor::initialize(const WebString& label, const WebVector<WebMediaStreamSource>& sources)
{
    MediaStreamSourceVector s;
    for (size_t i = 0; i < sources.size(); ++i) {
        MediaStreamSource* curr = sources[i];
        s.append(curr);
    }
    m_private = MediaStreamDescriptor::create(label, s);
}

} // namespace WebKit

#endif // ENABLE(MEDIA_STREAM)

