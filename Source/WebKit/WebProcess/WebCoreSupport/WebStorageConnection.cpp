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
#include "WebStorageConnection.h"

#include "FileSystemStorageHandleProxy.h"
#include "NetworkProcessConnection.h"
#include "NetworkStorageManagerMessages.h"
#include "WebProcess.h"
#include <WebCore/ClientOrigin.h>
#include <WebCore/ExceptionOr.h>

namespace WebKit {

Ref<WebStorageConnection> WebStorageConnection::create()
{
    return adoptRef(*new WebStorageConnection);
}

void WebStorageConnection::getPersisted(const WebCore::ClientOrigin& origin, CompletionHandler<void(bool)>&& completionHandler)
{
    connection().sendWithAsyncReply(Messages::NetworkStorageManager::Persisted(origin), WTFMove(completionHandler));
}

void WebStorageConnection::persist(const WebCore::ClientOrigin& origin, CompletionHandler<void(bool)>&& completionHandler)
{
    connection().sendWithAsyncReply(Messages::NetworkStorageManager::Persist(origin), WTFMove(completionHandler));
}

void WebStorageConnection::fileSystemGetDirectory(const WebCore::ClientOrigin& origin, CompletionHandler<void(WebCore::ExceptionOr<Ref<WebCore::FileSystemHandleImpl>>&&)>&& completionHandler)
{
    auto& connection = WebProcess::singleton().ensureNetworkProcessConnection().connection();
    connection.sendWithAsyncReply(Messages::NetworkStorageManager::FileSystemGetDirectory(origin), [weakConnection = makeWeakPtr(connection), completionHandler = WTFMove(completionHandler)](auto result) mutable {
        if (!result)
            return completionHandler(WebCore::Exception { convertToExceptionCode(result.error()) });

        if (!weakConnection || !result.value().isValid())
            return completionHandler(WebCore::Exception { WebCore::UnknownError, "Connection is lost"_s });

        Ref<WebCore::FileSystemHandleImpl> impl = FileSystemStorageHandleProxy::create(result.value(), *weakConnection);
        return completionHandler(WTFMove(impl));
    });
}

IPC::Connection& WebStorageConnection::connection()
{
    return WebProcess::singleton().ensureNetworkProcessConnection().connection();
}

} // namespace WebKit
