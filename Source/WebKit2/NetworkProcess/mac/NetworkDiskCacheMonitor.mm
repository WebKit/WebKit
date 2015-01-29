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

#import "config.h"
#import "NetworkDiskCacheMonitor.h"

#import "NetworkConnectionToWebProcess.h"
#import "NetworkProcessConnectionMessages.h"
#import "NetworkResourceLoader.h"
#import "WebCoreArgumentCoders.h"
#import <WebCore/CFNetworkSPI.h>

using namespace WebCore;

namespace WebKit {

void NetworkDiskCacheMonitor::monitorFileBackingStoreCreation(CFCachedURLResponseRef cachedResponse, NetworkResourceLoader* loader)
{
    if (!cachedResponse)
        return;

    ASSERT(loader);

    new NetworkDiskCacheMonitor(cachedResponse, loader); // Balanced by adoptPtr in the blocks set up in the DiskCacheMonitor constructor, one of which is guaranteed to run.
}

NetworkDiskCacheMonitor::NetworkDiskCacheMonitor(CFCachedURLResponseRef cachedResponse, NetworkResourceLoader* loader)
    : DiskCacheMonitor(loader->originalRequest(), loader->sessionID(), cachedResponse)
    , m_connectionToWebProcess(loader->connectionToWebProcess())
{
}

void NetworkDiskCacheMonitor::resourceBecameFileBacked(SharedBuffer& fileBackedBuffer)
{
    ShareableResource::Handle handle;
    NetworkResourceLoader::tryGetShareableHandleFromSharedBuffer(handle, fileBackedBuffer);
    if (handle.isNull())
        return;

    send(Messages::NetworkProcessConnection::DidCacheResource(resourceRequest(), handle, sessionID()));
}

IPC::Connection* NetworkDiskCacheMonitor::messageSenderConnection()
{
    return m_connectionToWebProcess->connection();
}

uint64_t NetworkDiskCacheMonitor::messageSenderDestinationID()
{
    return 0;
}

} // namespace WebKit
