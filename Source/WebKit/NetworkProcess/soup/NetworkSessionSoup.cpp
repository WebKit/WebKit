/*
 * Copyright (C) 2016 Igalia S.L.
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
#include "NetworkSessionSoup.h"

#include "NetworkProcess.h"
#include "NetworkSessionCreationParameters.h"
#include "WebCookieManager.h"
#include <WebCore/NetworkStorageSession.h>
#include <WebCore/SoupNetworkSession.h>
#include <libsoup/soup.h>

namespace WebKit {
using namespace WebCore;

NetworkSessionSoup::NetworkSessionSoup(NetworkProcess& networkProcess, NetworkSessionCreationParameters&& parameters)
    : NetworkSession(networkProcess, parameters.sessionID)
{
    networkStorageSession().setCookieObserverHandler([this] {
        NetworkProcess::singleton().supplement<WebCookieManager>()->notifyCookiesDidChange(m_sessionID);
    });
}

NetworkSessionSoup::~NetworkSessionSoup()
{
    if (auto* storageSession = NetworkStorageSession::storageSession(m_sessionID))
        storageSession->setCookieObserverHandler(nullptr);
}

SoupSession* NetworkSessionSoup::soupSession() const
{
    return networkStorageSession().getOrCreateSoupNetworkSession().soupSession();
}

void NetworkSessionSoup::clearCredentials()
{
#if SOUP_CHECK_VERSION(2, 57, 1)
    soup_auth_manager_clear_cached_credentials(SOUP_AUTH_MANAGER(soup_session_get_feature(soupSession(), SOUP_TYPE_AUTH_MANAGER)));
#endif
}

} // namespace WebKit
