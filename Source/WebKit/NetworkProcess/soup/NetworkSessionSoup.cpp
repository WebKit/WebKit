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
#include "WebSocketTaskSoup.h"
#include <WebCore/DeprecatedGlobalSettings.h>
#include <WebCore/NetworkStorageSession.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/SoupNetworkSession.h>
#include <libsoup/soup.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {
using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(NetworkSessionSoup);

NetworkSessionSoup::NetworkSessionSoup(NetworkProcess& networkProcess, const NetworkSessionCreationParameters& parameters)
    : NetworkSession(networkProcess, parameters)
    , m_networkSession(makeUnique<SoupNetworkSession>(m_sessionID))
    , m_persistentCredentialStorageEnabled(parameters.persistentCredentialStorageEnabled)
{
    CheckedPtr storageSession = networkStorageSession();
    ASSERT(storageSession);

    storageSession->setCookieAcceptPolicy(parameters.cookieAcceptPolicy);

    setIgnoreTLSErrors(parameters.ignoreTLSErrors);

    if (parameters.proxySettings.mode != SoupNetworkProxySettings::Mode::Default)
        setProxySettings(parameters.proxySettings);

    if (!parameters.cookiePersistentStoragePath.isEmpty())
        setCookiePersistentStorage(parameters.cookiePersistentStoragePath, parameters.cookiePersistentStorageType);
    else
        m_networkSession->setCookieJar(storageSession->cookieStorage());

    if (!parameters.hstsStorageDirectory.isEmpty())
        m_networkSession->setHSTSPersistentStorage(parameters.hstsStorageDirectory);
}

NetworkSessionSoup::~NetworkSessionSoup() = default;

SoupSession* NetworkSessionSoup::soupSession() const
{
    return m_networkSession->soupSession();
}

void NetworkSessionSoup::setCookiePersistentStorage(const String& storagePath, SoupCookiePersistentStorageType storageType)
{
    CheckedPtr storageSession = networkStorageSession();
    if (!storageSession)
        return;

    GRefPtr<SoupCookieJar> jar;
    switch (storageType) {
    case SoupCookiePersistentStorageType::Text:
        jar = adoptGRef(soup_cookie_jar_text_new(storagePath.utf8().data(), FALSE));
        break;
    case SoupCookiePersistentStorageType::SQLite:
        jar = adoptGRef(soup_cookie_jar_db_new(storagePath.utf8().data(), FALSE));
        break;
    }
    storageSession->setCookieStorage(WTF::move(jar));

    m_networkSession->setCookieJar(storageSession->cookieStorage());
}

void NetworkSessionSoup::clearCredentials(WallTime)
{
    soup_auth_manager_clear_cached_credentials(SOUP_AUTH_MANAGER(soup_session_get_feature(soupSession(), SOUP_TYPE_AUTH_MANAGER)));
}

RefPtr<WebSocketTask> NetworkSessionSoup::createWebSocketTask(WebPageProxyIdentifier webPageProxyID, std::optional<FrameIdentifier> frameID, std::optional<PageIdentifier> pageID, NetworkSocketChannel& channel, const ResourceRequest& request, const String& protocol, const ClientOrigin&, bool, bool, OptionSet<WebCore::AdvancedPrivacyProtections>, StoredCredentialsPolicy)
{
    GRefPtr<SoupMessage> soupMessage = request.createSoupMessage(blobRegistry());
    if (!soupMessage)
        return nullptr;

    if (request.url().protocolIs("wss"_s)) {
        g_signal_connect(soupMessage.get(), "accept-certificate", G_CALLBACK(+[](SoupMessage* message, GTlsCertificate* certificate, GTlsCertificateFlags errors,  NetworkSessionSoup* session) -> gboolean {
            if (DeprecatedGlobalSettings::allowsAnySSLCertificate())
                return TRUE;

            return !session->soupNetworkSession().checkTLSErrors(soup_message_get_uri(message), certificate, errors);
        }), this);
    }

    bool shouldBlockCookies = protect(networkStorageSession())->shouldBlockCookies(request, frameID, pageID, networkProcess().shouldRelaxThirdPartyCookieBlockingForPage(webPageProxyID), WebCore::IsKnownCrossSiteTracker::No);
    if (shouldBlockCookies)
        soup_message_disable_feature(soupMessage.get(), SOUP_TYPE_COOKIE_JAR);

    return WebSocketTask::create(channel, request, soupSession(), soupMessage.get(), protocol);
}

void NetworkSessionSoup::setIgnoreTLSErrors(bool ignoreTLSErrors)
{
    m_networkSession->setIgnoreTLSErrors(ignoreTLSErrors);
}

void NetworkSessionSoup::allowSpecificHTTPSCertificateForHost(const CertificateInfo& certificateInfo, const String& host)
{
    m_networkSession->allowSpecificHTTPSCertificateForHost(certificateInfo, host);
}

void NetworkSessionSoup::setProxySettings(const SoupNetworkProxySettings& settings)
{
    m_networkSession->setProxySettings(settings);
}

} // namespace WebKit
