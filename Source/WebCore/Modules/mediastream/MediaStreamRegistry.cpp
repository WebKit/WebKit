/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2013 Nokia Corporation and/or its subsidiary(-ies).
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
#include "MediaStreamRegistry.h"

#if ENABLE(MEDIA_STREAM)

#include "URL.h"
#include "MediaStream.h"
#include <wtf/MainThread.h>

namespace WebCore {

MediaStreamRegistry& MediaStreamRegistry::registry()
{
    // Since WebWorkers cannot obtain MediaSource objects, we should be on the main thread.
    ASSERT(isMainThread());
    DEFINE_STATIC_LOCAL(MediaStreamRegistry, instance, ());
    return instance;
}

void MediaStreamRegistry::registerURL(SecurityOrigin*, const URL& url, URLRegistrable* stream)
{
    ASSERT(&stream->registry() == this);
    ASSERT(isMainThread());
    m_mediaStreams.set(url.string(), static_cast<MediaStream*>(stream));
}

void MediaStreamRegistry::unregisterURL(const URL& url)
{
    ASSERT(isMainThread());
    m_mediaStreams.remove(url.string());
}

URLRegistrable* MediaStreamRegistry::lookup(const String& url) const
{
    ASSERT(isMainThread());
    return m_mediaStreams.get(url);
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
