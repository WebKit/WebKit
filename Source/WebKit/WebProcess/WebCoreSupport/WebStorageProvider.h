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

#pragma once

#include "Logging.h"
#include "MessageSender.h"
#include "WebProcess.h"
#include "WebProcessProxyMessages.h"
#include "WebStorageConnection.h"
#include <WebCore/StorageProvider.h>
#include <WebCore/StorageUtilities.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {

class WebStorageProvider final : public WebCore::StorageProvider {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(WebStorageProvider);
public:
    WebStorageProvider(WebPageProxyIdentifier webPageProxyID, const String& mediaKeysStorageDirectory, FileSystem::Salt mediaKeysStorageSalt)
        : m_webPageProxyID(webPageProxyID)
        , m_mediaKeysStorageDirectory(mediaKeysStorageDirectory)
        , m_mediaKeysStorageSalt(mediaKeysStorageSalt)
    {
    }

private:
    WebCore::StorageConnection& storageConnection() final
    {
        ASSERT_WITH_SECURITY_IMPLICATION(WTF::isMainRunLoop());

        if (!m_connection)
            lazyInitialize(m_connection, WebStorageConnection::create());
        
        return *m_connection;
    }

#if !ENABLE(BLOCKING_OF_WEBCONTENT_FILE_WRITE)
    String ensureMediaKeysStorageDirectoryForOrigin(const WebCore::SecurityOriginData& origin) final
    {
        if (m_mediaKeysStorageDirectory.isEmpty())
            return emptyString();

        auto originDirectoryName = WebCore::StorageUtilities::encodeSecurityOriginForFileName(m_mediaKeysStorageSalt, origin);
        auto originDirectory = FileSystem::pathByAppendingComponent(m_mediaKeysStorageDirectory, originDirectoryName);
        WebCore::StorageUtilities::writeOriginToFile(FileSystem::pathByAppendingComponent(originDirectory, "origin"_s), WebCore::ClientOrigin { origin, origin });
        return originDirectory;
    }
#else
    String ensureMediaKeysStorageDirectoryForOrigin(const WebCore::SecurityOriginData& origin, std::optional<WebCore::FrameIdentifier> frameID) final
    {
        RELEASE_LOG(Media, "ensureMediaKeysStorageDirectoryForOrigin %" PRIu64, m_webPageProxyID.toUInt64());
        if (m_mediaKeysStorageDirectory.isEmpty())
            return emptyString();
        if (!frameID)
            return emptyString();

        WebProcess::singleton().send(Messages::WebProcessProxy::EnsureMediaKeysStorageDirectory(*frameID));

        auto originDirectoryName = WebCore::StorageUtilities::encodeSecurityOriginForFileName(m_mediaKeysStorageSalt, origin);
        return FileSystem::pathByAppendingComponent(m_mediaKeysStorageDirectory, originDirectoryName);
    }
#endif

    void setMediaKeysStorageDirectory(const String&) final
    {
        RELEASE_ASSERT_NOT_REACHED();
    }

    const RefPtr<WebStorageConnection> m_connection;
    WebPageProxyIdentifier m_webPageProxyID;
    String m_mediaKeysStorageDirectory;
    FileSystem::Salt m_mediaKeysStorageSalt;
};

} // namespace WebKit
