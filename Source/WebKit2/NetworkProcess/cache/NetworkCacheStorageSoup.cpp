/*
 * Copyright (C) 2014 Igalia S.L.
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
#include "NetworkCacheStorage.h"

#if ENABLE(NETWORK_CACHE)

#include "Logging.h"
#include "NetworkCacheCoders.h"
#include <WebCore/FileSystem.h>
#include <WebCore/NotImplemented.h>
#include <wtf/RunLoop.h>

namespace WebKit {

static const char* networkCacheSubdirectory = "WebKitCache";

NetworkCacheStorage::Data::Data()
    : m_data(nullptr)
    , m_size(0)
{
    notImplemented();
}

NetworkCacheStorage::Data::Data(const uint8_t* data, size_t size)
    : m_data(data)
    , m_size(size)
{
    notImplemented();
}

const uint8_t* NetworkCacheStorage::Data::data() const
{
    notImplemented();
    return nullptr;
}

bool NetworkCacheStorage::Data::isNull() const
{
    notImplemented();
    return true;
}

std::unique_ptr<NetworkCacheStorage> NetworkCacheStorage::open(const String& applicationCachePath)
{
    ASSERT(RunLoop::isMain());
    String networkCachePath = WebCore::pathByAppendingComponent(applicationCachePath, networkCacheSubdirectory);
    if (!WebCore::makeAllDirectories(networkCachePath))
        return nullptr;
    return std::unique_ptr<NetworkCacheStorage>(new NetworkCacheStorage(networkCachePath));
}

NetworkCacheStorage::NetworkCacheStorage(const String& directoryPath)
    : m_directoryPath(directoryPath)
    , m_maximumSize(std::numeric_limits<size_t>::max())
    , m_activeRetrieveOperationCount(0)
{
    initializeKeyFilter();
}

void NetworkCacheStorage::initializeKeyFilter()
{
    ASSERT(RunLoop::isMain());
    notImplemented();
}

void NetworkCacheStorage::removeEntry(const NetworkCacheKey&)
{
    ASSERT(RunLoop::isMain());
    notImplemented();
}

void NetworkCacheStorage::dispatchRetrieveOperation(const RetrieveOperation&)
{
    ASSERT(RunLoop::isMain());
    notImplemented();
}

void NetworkCacheStorage::dispatchPendingRetrieveOperations()
{
    ASSERT(RunLoop::isMain());
    notImplemented();
}

void NetworkCacheStorage::retrieve(const NetworkCacheKey&, unsigned /* priority */, std::function<bool (std::unique_ptr<Entry>)> completionHandler)
{
    ASSERT(RunLoop::isMain());
    notImplemented();
    completionHandler(nullptr);
}

void NetworkCacheStorage::store(const NetworkCacheKey&, const Entry&, std::function<void (bool success)> completionHandler)
{
    ASSERT(RunLoop::isMain());
    notImplemented();
    completionHandler(false);
}

void NetworkCacheStorage::setMaximumSize(size_t size)
{
    ASSERT(RunLoop::isMain());
    notImplemented();
    m_maximumSize = size;
}

void NetworkCacheStorage::clear()
{
    ASSERT(RunLoop::isMain());
    LOG(NetworkCacheStorage, "(NetworkProcess) clearing cache");
    notImplemented();
    m_keyFilter.clear();
}

} // namespace WebKit

#endif // ENABLE(NETWORK_CACHE)
