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
#include "ScriptExecutionContext.h"
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

void MediaSourceRegistry::registerURL(const ScriptExecutionContext& context, const URL& url, URLRegistrable& registrable)
{
    ASSERT(&registrable.registry() == this);
    ASSERT(isMainThread());

    auto& urlString = url.string();
    m_urlsPerContext.add(context.identifier(), HashSet<String>()).iterator->value.add(urlString);

    MediaSource& source = static_cast<MediaSource&>(registrable);
    source.addedToRegistry();
    m_mediaSources.add(urlString, std::pair { RefPtr { &source }, context.identifier() });
}

void MediaSourceRegistry::unregisterURL(const URL& url, const SecurityOriginData&)
{
    // MediaSource objects are not exposed to workers.
    if (!isMainThread())
        return;

    auto& urlString = url.string();
    auto [source, contextIdentifier] = m_mediaSources.take(urlString);
    if (!source)
        return;

    source->removedFromRegistry();

    auto m_urlsPerContextIterator = m_urlsPerContext.find(contextIdentifier);
    ASSERT(m_urlsPerContextIterator != m_urlsPerContext.end());
    ASSERT(m_urlsPerContextIterator->value.contains(urlString));
    m_urlsPerContextIterator->value.remove(urlString);
    if (m_urlsPerContextIterator->value.isEmpty())
        m_urlsPerContext.remove(m_urlsPerContextIterator);
}

void MediaSourceRegistry::unregisterURLsForContext(const ScriptExecutionContext& context)
{
    // MediaSource objects are not exposed to workers.
    if (!isMainThread())
        return;

    auto urls = m_urlsPerContext.take(context.identifier());
    for (auto& url : urls) {
        ASSERT(m_mediaSources.contains(url));
        auto [source, contextIdentifier] = m_mediaSources.take(url);
        source->removedFromRegistry();
    }
}

URLRegistrable* MediaSourceRegistry::lookup(const String& url) const
{
    ASSERT(isMainThread());
    if (auto it = m_mediaSources.find(url); it != m_mediaSources.end())
        return it->value.first.get();
    return nullptr;
}

MediaSourceRegistry::MediaSourceRegistry()
{
    MediaSource::setRegistry(this);
}

} // namespace WebCore

#endif
