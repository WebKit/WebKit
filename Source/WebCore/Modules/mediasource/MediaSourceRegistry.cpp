/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include "MediaSourceRegistry.h"

#if ENABLE(MEDIA_SOURCE)

#include "MediaSource.h"
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/URL.h>

namespace WebCore {

MediaSourceRegistry& MediaSourceRegistry::registry()
{
    ASSERT(isMainThread());
    static NeverDestroyed<MediaSourceRegistry> instance;
    return instance;
}

void MediaSourceRegistry::registerURL(ScriptExecutionContext&, const URL& url, URLRegistrable& registrable)
{
    ASSERT(&registrable.registry() == this);
    ASSERT(isMainThread());

    MediaSource& source = static_cast<MediaSource&>(registrable);
    source.addedToRegistry();
    m_mediaSources.set(url.string(), &source);
}

void MediaSourceRegistry::unregisterURL(const URL& url)
{
    ASSERT(isMainThread());
    if (auto source = m_mediaSources.take(url.string()))
        source->removedFromRegistry();
}

URLRegistrable* MediaSourceRegistry::lookup(const String& url) const
{
    ASSERT(isMainThread());
    return m_mediaSources.get(url);
}

MediaSourceRegistry::MediaSourceRegistry()
{
    MediaSource::setRegistry(this);
}

} // namespace WebCore

#endif
