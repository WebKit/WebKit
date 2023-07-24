/*
 * Copyright (C) 2012 Igalia S.L.
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
#include "APIHTTPCookieStore.h"

#include "NetworkProcess.h"
#include "NetworkProcessProxy.h"
#include "SoupCookiePersistentStorageType.h"
#include "WebCookieManagerMessages.h"

namespace API {

void HTTPCookieStore::setCookiePersistentStorage(const WTF::String& storagePath, WebKit::SoupCookiePersistentStorageType storageType)
{
    if (auto* networkProcess = networkProcessIfExists())
        networkProcess->send(Messages::WebCookieManager::SetCookiePersistentStorage(m_sessionID, storagePath, storageType), 0);
}

void HTTPCookieStore::replaceCookies(Vector<WebCore::Cookie>&& cookies, CompletionHandler<void()>&& completionHandler)
{
    if (auto* networkProcess = networkProcessIfExists())
        networkProcess->sendWithAsyncReply(Messages::WebCookieManager::ReplaceCookies(m_sessionID, cookies), WTFMove(completionHandler));
    else
        completionHandler();
}

void HTTPCookieStore::getAllCookies(CompletionHandler<void(const Vector<WebCore::Cookie>&)>&& completionHandler)
{
    if (auto* networkProcess = networkProcessIfExists())
        networkProcess->sendWithAsyncReply(Messages::WebCookieManager::GetAllCookies(m_sessionID), WTFMove(completionHandler));
    else
        completionHandler({ });
}

} // namespace API
