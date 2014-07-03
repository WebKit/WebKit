/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "NetworkBlobRegistry.h"

#if ENABLE(NETWORK_PROCESS)

#include "BlobDataFileReferenceWithSandboxExtension.h"
#include "SandboxExtension.h"
#include <WebCore/BlobPart.h>
#include <WebCore/BlobRegistryImpl.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RunLoop.h>

using namespace WebCore;

namespace WebKit {

NetworkBlobRegistry& NetworkBlobRegistry::shared()
{
    ASSERT(RunLoop::isMain());
    static NeverDestroyed<NetworkBlobRegistry> registry;
    return registry;
}

NetworkBlobRegistry::NetworkBlobRegistry()
{
}

void NetworkBlobRegistry::registerFileBlobURL(NetworkConnectionToWebProcess* connection, const URL& url, const String& path, PassRefPtr<SandboxExtension> sandboxExtension, const String& contentType)
{
    blobRegistry().registerFileBlobURL(url, BlobDataFileReferenceWithSandboxExtension::create(path, sandboxExtension), contentType);

    ASSERT(!m_blobsForConnection.get(connection).contains(url));
    BlobForConnectionMap::iterator mapIterator = m_blobsForConnection.find(connection);
    if (mapIterator == m_blobsForConnection.end())
        mapIterator = m_blobsForConnection.add(connection, HashSet<URL>()).iterator;
    mapIterator->value.add(url);
}

void NetworkBlobRegistry::registerBlobURL(NetworkConnectionToWebProcess* connection, const URL& url, Vector<WebCore::BlobPart> blobParts, const String& contentType)
{
    blobRegistry().registerBlobURL(url, WTF::move(blobParts), contentType);

    ASSERT(!m_blobsForConnection.get(connection).contains(url));
    BlobForConnectionMap::iterator mapIterator = m_blobsForConnection.find(connection);
    if (mapIterator == m_blobsForConnection.end())
        mapIterator = m_blobsForConnection.add(connection, HashSet<URL>()).iterator;
    mapIterator->value.add(url);
}

void NetworkBlobRegistry::registerBlobURL(NetworkConnectionToWebProcess* connection, const WebCore::URL& url, const WebCore::URL& srcURL)
{
    // The connection may not be registered if NetworkProcess prevously crashed for any reason.
    BlobForConnectionMap::iterator mapIterator = m_blobsForConnection.find(connection);
    if (mapIterator == m_blobsForConnection.end())
        return;

    blobRegistry().registerBlobURL(url, srcURL);

    ASSERT(mapIterator->value.contains(srcURL));
    mapIterator->value.add(url);
}

void NetworkBlobRegistry::registerBlobURLForSlice(NetworkConnectionToWebProcess* connection, const WebCore::URL& url, const WebCore::URL& srcURL, int64_t start, int64_t end)
{
    // The connection may not be registered if NetworkProcess prevously crashed for any reason.
    BlobForConnectionMap::iterator mapIterator = m_blobsForConnection.find(connection);
    if (mapIterator == m_blobsForConnection.end())
        return;

    blobRegistry().registerBlobURLForSlice(url, srcURL, start, end);

    ASSERT(mapIterator->value.contains(srcURL));
    mapIterator->value.add(url);
}

void NetworkBlobRegistry::unregisterBlobURL(NetworkConnectionToWebProcess* connection, const WebCore::URL& url)
{
    // The connection may not be registered if NetworkProcess prevously crashed for any reason.
    BlobForConnectionMap::iterator mapIterator = m_blobsForConnection.find(connection);
    if (mapIterator == m_blobsForConnection.end())
        return;

    blobRegistry().unregisterBlobURL(url);

    ASSERT(mapIterator->value.contains(url));
    mapIterator->value.remove(url);
}

uint64_t NetworkBlobRegistry::blobSize(NetworkConnectionToWebProcess* connection, const WebCore::URL& url)
{
    if (!m_blobsForConnection.contains(connection) || !m_blobsForConnection.find(connection)->value.contains(url))
        return 0;

    return blobRegistry().blobSize(url);
}

void NetworkBlobRegistry::connectionToWebProcessDidClose(NetworkConnectionToWebProcess* connection)
{
    if (!m_blobsForConnection.contains(connection))
        return;

    HashSet<URL>& blobsForConnection = m_blobsForConnection.find(connection)->value;
    for (HashSet<URL>::iterator iter = blobsForConnection.begin(), end = blobsForConnection.end(); iter != end; ++iter)
        blobRegistry().unregisterBlobURL(*iter);

    m_blobsForConnection.remove(connection);
}

Vector<RefPtr<BlobDataFileReference>> NetworkBlobRegistry::filesInBlob(NetworkConnectionToWebProcess* connection, const WebCore::URL& url)
{
    if (!m_blobsForConnection.contains(connection) || !m_blobsForConnection.find(connection)->value.contains(url))
        return Vector<RefPtr<BlobDataFileReference>>();

    ASSERT(blobRegistry().isBlobRegistryImpl());
    BlobData* blobData = static_cast<BlobRegistryImpl&>(blobRegistry()).getBlobDataFromURL(url);
    if (!blobData)
        return Vector<RefPtr<BlobDataFileReference>>();

    Vector<RefPtr<BlobDataFileReference>> result;
    for (const BlobDataItem& item : blobData->items()) {
        if (item.type == BlobDataItem::File)
            result.append(item.file);
    }

    return result;
}

}

#endif
