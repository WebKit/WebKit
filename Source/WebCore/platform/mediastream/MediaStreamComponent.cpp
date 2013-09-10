/*
 * Copyright (C) 2011 Ericsson AB. All rights reserved.
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

#if ENABLE(MEDIA_STREAM)

#include "MediaStreamComponent.h"

#include "MediaStreamSource.h"
#include "UUID.h"

namespace WebCore {

PassRefPtr<MediaStreamComponent> MediaStreamComponent::create(PassRefPtr<MediaStreamSource> source)
{
    return adoptRef(new MediaStreamComponent(createCanonicalUUIDString(), 0, source));
}

PassRefPtr<MediaStreamComponent> MediaStreamComponent::create(MediaStreamDescriptor* stream, PassRefPtr<MediaStreamSource> source)
{
    return adoptRef(new MediaStreamComponent(createCanonicalUUIDString(), stream, source));
}

MediaStreamComponent::MediaStreamComponent(const String& id, MediaStreamDescriptor* stream, PassRefPtr<MediaStreamSource> source)
    : m_stream(stream)
    , m_source(source)
    , m_id(id)
    , m_enabled(true)
{
    ASSERT(m_id.length());
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
