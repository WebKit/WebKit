/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(BLOB)

#include "DOMURL.h"

#include "Blob.h"
#include "BlobURL.h"
#include "KURL.h"
#include "ScriptExecutionContext.h"
#include "ThreadableBlobRegistry.h"
#include <wtf/MainThread.h>

#if ENABLE(MEDIA_STREAM)
#include "MediaStream.h"
#include "MediaStreamRegistry.h"
#endif

namespace WebCore {

DOMURL::DOMURL(ScriptExecutionContext* scriptExecutionContext)
    : ContextDestructionObserver(scriptExecutionContext)
{
}

DOMURL::~DOMURL()
{
}

void DOMURL::contextDestroyed()
{
    ContextDestructionObserver::contextDestroyed();

    HashSet<String>::iterator publicBlobURLsEnd = m_publicBlobURLs.end();
    for (HashSet<String>::iterator iter = m_publicBlobURLs.begin(); iter != publicBlobURLsEnd; ++iter)
        ThreadableBlobRegistry::unregisterBlobURL(KURL(ParsedURLString, *iter));

#if ENABLE(MEDIA_STREAM)
    HashSet<String>::iterator publicStreamURLsEnd = m_publicStreamURLs.end();
    for (HashSet<String>::iterator iter = m_publicStreamURLs.begin(); iter != publicStreamURLsEnd; ++iter)
        MediaStreamRegistry::registry().unregisterMediaStreamURL(KURL(ParsedURLString, *iter));
#endif
}

#if ENABLE(MEDIA_STREAM)
String DOMURL::createObjectURL(MediaStream* stream)
{
    if (!m_scriptExecutionContext || !stream)
        return String();

    KURL publicURL = BlobURL::createPublicURL(scriptExecutionContext()->securityOrigin());
    if (publicURL.isEmpty())
        return String();

    // Since WebWorkers cannot obtain Stream objects, we should be on the main thread.
    ASSERT(isMainThread());

    MediaStreamRegistry::registry().registerMediaStreamURL(publicURL, stream);
    m_publicStreamURLs.add(publicURL.string());

    return publicURL.string();
}
#endif

String DOMURL::createObjectURL(Blob* blob)
{
    if (!m_scriptExecutionContext || !blob)
        return String();

    KURL publicURL = BlobURL::createPublicURL(scriptExecutionContext()->securityOrigin());
    if (publicURL.isEmpty())
        return String();

    ThreadableBlobRegistry::registerBlobURL(publicURL, blob->url());
    m_publicBlobURLs.add(publicURL.string());

    return publicURL.string();
}

void DOMURL::revokeObjectURL(const String& urlString)
{
    if (!m_scriptExecutionContext)
        return;

    KURL url(KURL(), urlString);

    if (m_publicBlobURLs.contains(url.string())) {
        ThreadableBlobRegistry::unregisterBlobURL(url);
        m_publicBlobURLs.remove(url.string());
    }

#if ENABLE(MEDIA_STREAM)
    if (m_publicStreamURLs.contains(url.string())) {
        // FIXME: make sure of this assertion below. Raise a spec question if required.
        // Since WebWorkers cannot obtain Stream objects, we should be on the main thread.
        ASSERT(isMainThread());
        MediaStreamRegistry::registry().unregisterMediaStreamURL(url);
        m_publicStreamURLs.remove(url.string());
    }
#endif
}

} // namespace WebCore

#endif // ENABLE(BLOB)
