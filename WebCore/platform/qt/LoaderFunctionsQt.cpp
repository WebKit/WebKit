/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include <wtf/Vector.h>
#if PLATFORM(KDE)
#include <kio/netaccess.h>
#endif

#include <QFile>

#include "Cache.h"
#include "loader.h"
#include "FrameQt.h"
#include "Request.h"
#include "ResourceHandle.h"
#include "ResourceRequest.h"
#include "DocLoader.h"
#include "CachedResource.h"
#include "DeprecatedString.h"

namespace WebCore {

Vector<char> ServeSynchronousRequest(Loader*, DocLoader *docLoader, const ResourceRequest& request, ResourceResponse&)
{
    // FIXME: Handle last paremeter: "responseHeaders"
    FrameQt* frame = QtFrame(docLoader->frame());

    if (!frame)
        return Vector<char>();

    // FIXME: We shouldn't use temporary files for sync jobs!
    QString tempFile;
#if PLATFORM(KDE)
    if (!KIO::NetAccess::download(KUrl(request.url().url()), tempFile, 0))
        return Vector<char>();
#else
    KURL url = request.url();
    if (!url.isLocalFile())
        return Vector<char>();
    tempFile = url.path();
#endif

    QFile file(tempFile);
    if (!file.open(QIODevice::ReadOnly)) {
#if PLATFORM(KDE)
        KIO::NetAccess::removeTempFile(tempFile);
#endif
        return Vector<char>();
    }

    ASSERT(!file.atEnd());

    QByteArray content = file.readAll();
#if PLATFORM(KDE)
    KIO::NetAccess::removeTempFile(tempFile);
#endif

    Vector<char> resultBuffer;
    resultBuffer.append(content.data(), content.size());

    return resultBuffer;
}

int NumberOfPendingOrLoadingRequests(DocLoader* docLoader)
{
    return cache()->loader()->numRequests(docLoader);
}

bool CheckIfReloading(WebCore::DocLoader* docLoader)
{
    // FIXME: Needs a real implementation!
    return false;
}

void CheckCacheObjectStatus(DocLoader* docLoader, CachedResource* cachedResource)
{
    // Return from the function for objects that we didn't load from the cache.
    if (!cachedResource)
        return;

    switch (cachedResource->status()) {
        case CachedResource::Cached:
            break;
        case CachedResource::NotCached:
        case CachedResource::Unknown:
        case CachedResource::New:
        case CachedResource::Pending:
            return;
    }

    // FIXME: Doesn't work at the moment! ASSERT(cachedResource->response());

    // FIXME: Notify the caller that we "loaded".
}

bool IsResponseURLEqualToURL(PlatformResponse response, const String& url)
{
    if (!response)
        return false;

    return response->url == QString(url);
}

DeprecatedString ResponseURL(PlatformResponse response)
{
    if (!response)
        return DeprecatedString();

    return response->url;
}

DeprecatedString ResponseMIMEType(PlatformResponse)
{
    // FIXME: Store the response mime type in QtPlatformResponse!
    return DeprecatedString();
}

bool ResponseIsMultipart(PlatformResponse response)
{
    return ResponseMIMEType(response) == "multipart/x-mixed-replace";
}

time_t CacheObjectExpiresTime(DocLoader*, PlatformResponse)
{
    // FIXME: Implement me!
    return 0;
}

} // namespace WebCore
