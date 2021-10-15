/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include "NetworkStorageManager.h"

#include "NetworkStorageManagerMessages.h"
#include "OriginStorageManager.h"
#include <pal/crypto/CryptoDigest.h>
#include <wtf/text/Base64.h>

namespace WebKit {

Ref<NetworkStorageManager> NetworkStorageManager::create(PAL::SessionID sessionID, const String& path)
{
    return adoptRef(*new NetworkStorageManager(sessionID, path));
}

NetworkStorageManager::NetworkStorageManager(PAL::SessionID sessionID, const String& path)
    : m_sessionID(sessionID)
    , m_queue(WorkQueue::create("com.apple.WebKit.Storage"))
{
    ASSERT(RunLoop::isMain());

    m_queue->dispatch([this, protectedThis = makeRef(*this), path = path.isolatedCopy()]() mutable {
        m_path = path;
        if (!m_path.isEmpty()) {
            auto saltPath = FileSystem::pathByAppendingComponent(m_path, "salt");
            m_salt = FileSystem::readOrMakeSalt(saltPath).value_or(FileSystem::Salt());
        }
    });
}

void NetworkStorageManager::startReceivingMessageFromConnection(IPC::Connection& connection)
{
    ASSERT(RunLoop::isMain());

    connection.addWorkQueueMessageReceiver(Messages::NetworkStorageManager::messageReceiverName(), m_queue.get(), this);
}

void NetworkStorageManager::stopReceivingMessageFromConnection(IPC::Connection& connection)
{
    ASSERT(RunLoop::isMain());

    connection.removeWorkQueueMessageReceiver(Messages::NetworkStorageManager::messageReceiverName());
}

static String encode(const String& string, FileSystem::Salt salt)
{
    auto crypto = PAL::CryptoDigest::create(PAL::CryptoDigest::Algorithm::SHA_256);
    auto utf8String = string.utf8();
    crypto->addBytes(utf8String.data(), utf8String.length());
    crypto->addBytes(salt.data(), salt.size());
    auto hash = crypto->computeHash();
    return base64URLEncodeToString(hash.data(), hash.size());
}

static String originPath(const String& rootPath, const WebCore::ClientOrigin& origin, FileSystem::Salt salt)
{
    if (rootPath.isEmpty())
        return rootPath;

    auto encodedTopOrigin = encode(origin.topOrigin.toString(), salt);
    auto encodedOpeningOrigin = encode(origin.clientOrigin.toString(), salt);
    return FileSystem::pathByAppendingComponents(rootPath, { encodedTopOrigin, encodedOpeningOrigin });
}

OriginStorageManager& NetworkStorageManager::localOriginStorageManager(const WebCore::ClientOrigin& origin)
{
    ASSERT(!RunLoop::isMain());

    return *m_localOriginStorageManagers.ensure(origin, [&] {
        return makeUnique<OriginStorageManager>(originPath(m_path, origin, m_salt));
    }).iterator->value;
}

void NetworkStorageManager::persisted(const WebCore::ClientOrigin& origin, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    completionHandler(localOriginStorageManager(origin).persisted());
}

void NetworkStorageManager::persist(const WebCore::ClientOrigin& origin, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    localOriginStorageManager(origin).persist();
    completionHandler(true);
}

} // namespace WebKit

