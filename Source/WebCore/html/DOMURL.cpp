/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2011 Motorola Mobility Inc.
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
#include "DOMURL.h"

#include "ActiveDOMObject.h"
#include "KURL.h"
#include "SecurityOrigin.h"
#include <wtf/MainThread.h>

#if ENABLE(BLOB)
#include "Blob.h"
#include "BlobURL.h"
#include "ScriptExecutionContext.h"
#include "ThreadableBlobRegistry.h"
#include <wtf/PassOwnPtr.h>
#if ENABLE(MEDIA_STREAM)
#include "MediaStream.h"
#include "MediaStreamRegistry.h"
#endif
#endif

namespace WebCore {

#if ENABLE(BLOB)
class PublicURLManager;
typedef HashMap<ScriptExecutionContext*, OwnPtr<PublicURLManager> > PublicURLManagerMap;
static PublicURLManagerMap& publicURLManagerMap();

class PublicURLManager : public ContextDestructionObserver {
public:
    explicit PublicURLManager(ScriptExecutionContext* scriptExecutionContext)
        : ContextDestructionObserver(scriptExecutionContext) { }

    virtual void contextDestroyed()
    {
        HashSet<String>::iterator blobURLsEnd = m_blobURLs.end();
        for (HashSet<String>::iterator iter = m_blobURLs.begin(); iter != blobURLsEnd; ++iter)
            ThreadableBlobRegistry::unregisterBlobURL(KURL(ParsedURLString, *iter));

#if ENABLE(MEDIA_STREAM)
        HashSet<String>::iterator streamURLsEnd = m_streamURLs.end();
        for (HashSet<String>::iterator iter = m_streamURLs.begin(); iter != streamURLsEnd; ++iter)
            MediaStreamRegistry::registry().unregisterMediaStreamURL(KURL(ParsedURLString, *iter));
#endif

        ScriptExecutionContext* context = scriptExecutionContext();
        ContextDestructionObserver::contextDestroyed();
        publicURLManagerMap().remove(context);
    }

    HashSet<String>& blobURLs() { return m_blobURLs; }
#if ENABLE(MEDIA_STREAM)
    HashSet<String>& streamURLs() { return m_streamURLs; }
#endif

private:
    HashSet<String> m_blobURLs;
#if ENABLE(MEDIA_STREAM)
    HashSet<String> m_streamURLs;
#endif
};

static PublicURLManagerMap& publicURLManagerMap()
{
    DEFINE_STATIC_LOCAL(PublicURLManagerMap, staticPublicURLManagers, ());
    return staticPublicURLManagers;
}

static PublicURLManager& publicURLManager(ScriptExecutionContext* scriptExecutionContext)
{
    PublicURLManagerMap& map = publicURLManagerMap();
    OwnPtr<PublicURLManager>& manager = map.add(scriptExecutionContext, nullptr).first->second;
    if (!manager)
        manager = adoptPtr(new PublicURLManager(scriptExecutionContext));
    return *manager;
}

static HashSet<String>& publicBlobURLs(ScriptExecutionContext* scriptExecutionContext)
{
    return publicURLManager(scriptExecutionContext).blobURLs();
}

#if ENABLE(MEDIA_STREAM)
static HashSet<String>& publicStreamURLs(ScriptExecutionContext* scriptExecutionContext)
{
    return publicURLManager(scriptExecutionContext).streamURLs();
}

String DOMURL::createObjectURL(ScriptExecutionContext* scriptExecutionContext, MediaStream* stream)
{
    if (!scriptExecutionContext || !stream)
        return String();

    KURL publicURL = BlobURL::createPublicURL(scriptExecutionContext->securityOrigin());
    if (publicURL.isEmpty())
        return String();

    // Since WebWorkers cannot obtain Stream objects, we should be on the main thread.
    ASSERT(isMainThread());

    MediaStreamRegistry::registry().registerMediaStreamURL(publicURL, stream);
    publicStreamURLs(scriptExecutionContext).add(publicURL.string());

    return publicURL.string();
}
#endif

String DOMURL::createObjectURL(ScriptExecutionContext* scriptExecutionContext, Blob* blob)
{
    if (!scriptExecutionContext || !blob)
        return String();

    KURL publicURL = BlobURL::createPublicURL(scriptExecutionContext->securityOrigin());
    if (publicURL.isEmpty())
        return String();

    ThreadableBlobRegistry::registerBlobURL(publicURL, blob->url());
    publicBlobURLs(scriptExecutionContext).add(publicURL.string());

    return publicURL.string();
}

void DOMURL::revokeObjectURL(ScriptExecutionContext* scriptExecutionContext, const String& urlString)
{
    if (!scriptExecutionContext)
        return;

    KURL url(KURL(), urlString);

    HashSet<String>& blobURLs = publicBlobURLs(scriptExecutionContext);
    if (blobURLs.contains(url.string())) {
        ThreadableBlobRegistry::unregisterBlobURL(url);
        blobURLs.remove(url.string());
    }
#if ENABLE(MEDIA_STREAM)
    HashSet<String>& streamURLs = publicStreamURLs(scriptExecutionContext);
    if (streamURLs.contains(url.string())) {
        // FIXME: make sure of this assertion below. Raise a spec question if required.
        // Since WebWorkers cannot obtain Stream objects, we should be on the main thread.
        ASSERT(isMainThread());
        MediaStreamRegistry::registry().unregisterMediaStreamURL(url);
        streamURLs.remove(url.string());
    }
#endif
}
#endif // ENABLE(BLOB)

} // namespace WebCore

