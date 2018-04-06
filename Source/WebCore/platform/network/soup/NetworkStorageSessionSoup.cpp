/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
 * Copyright (C) 2013 University of Szeged. All rights reserved.
 * Copyright (C) 2016 Igalia S.L.
 * Copyright (C) 2017 Endless Mobile, Inc.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS''
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
#include "NetworkStorageSession.h"

#if USE(SOUP)

#include "Cookie.h"
#include "GUniquePtrSoup.h"
#include "ResourceHandle.h"
#include "SoupNetworkSession.h"
#include <libsoup/soup.h>
#include <wtf/DateMath.h>
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/glib/GUniquePtr.h>

#if USE(LIBSECRET)
#include "GRefPtrGtk.h"
#include <glib/gi18n-lib.h>
#define SECRET_WITH_UNSTABLE 1
#define SECRET_API_SUBJECT_TO_CHANGE 1
#include <libsecret/secret.h>
#endif

namespace WebCore {

NetworkStorageSession::NetworkStorageSession(PAL::SessionID sessionID, std::unique_ptr<SoupNetworkSession>&& session)
    : m_sessionID(sessionID)
    , m_session(WTFMove(session))
{
    setCookieStorage(m_session ? m_session->cookieJar() : nullptr);
}

NetworkStorageSession::~NetworkStorageSession()
{
    g_signal_handlers_disconnect_matched(m_cookieStorage.get(), G_SIGNAL_MATCH_DATA, 0, 0, nullptr, nullptr, this);
}

static std::unique_ptr<NetworkStorageSession>& defaultSession()
{
    ASSERT(isMainThread());
    static NeverDestroyed<std::unique_ptr<NetworkStorageSession>> session;
    return session;
}

NetworkStorageSession& NetworkStorageSession::defaultStorageSession()
{
    if (!defaultSession())
        defaultSession() = std::make_unique<NetworkStorageSession>(PAL::SessionID::defaultSessionID(), nullptr);
    return *defaultSession();
}

void NetworkStorageSession::ensureSession(PAL::SessionID sessionID, const String&)
{
    ASSERT(!globalSessionMap().contains(sessionID));
    globalSessionMap().add(sessionID, std::make_unique<NetworkStorageSession>(sessionID, std::make_unique<SoupNetworkSession>(sessionID)));
}

void NetworkStorageSession::switchToNewTestingSession()
{
    defaultSession() = std::make_unique<NetworkStorageSession>(PAL::SessionID::defaultSessionID(), std::make_unique<SoupNetworkSession>());
}

SoupNetworkSession& NetworkStorageSession::getOrCreateSoupNetworkSession() const
{
    if (!m_session)
        m_session = std::make_unique<SoupNetworkSession>(m_sessionID, m_cookieStorage.get());
    return *m_session;
}

void NetworkStorageSession::clearSoupNetworkSessionAndCookieStorage()
{
    ASSERT(defaultSession().get() == this);
    m_session = nullptr;
    m_cookieObserverHandler = nullptr;
    m_cookieStorage = nullptr;
}

void NetworkStorageSession::cookiesDidChange(NetworkStorageSession* session)
{
    if (session->m_cookieObserverHandler)
        session->m_cookieObserverHandler();
}

SoupCookieJar* NetworkStorageSession::cookieStorage() const
{
    RELEASE_ASSERT(!m_session || m_session->cookieJar() == m_cookieStorage.get());
    return m_cookieStorage.get();
}

void NetworkStorageSession::setCookieStorage(SoupCookieJar* jar)
{
    if (m_cookieStorage)
        g_signal_handlers_disconnect_matched(m_cookieStorage.get(), G_SIGNAL_MATCH_DATA, 0, 0, nullptr, nullptr, this);

    // We always have a valid cookieStorage.
    if (jar)
        m_cookieStorage = jar;
    else {
        m_cookieStorage = adoptGRef(soup_cookie_jar_new());
        soup_cookie_jar_set_accept_policy(m_cookieStorage.get(), SOUP_COOKIE_JAR_ACCEPT_NO_THIRD_PARTY);
    }
    g_signal_connect_swapped(m_cookieStorage.get(), "changed", G_CALLBACK(cookiesDidChange), this);
    if (m_session && m_session->cookieJar() != m_cookieStorage.get())
        m_session->setCookieJar(m_cookieStorage.get());
}

void NetworkStorageSession::setCookieObserverHandler(Function<void ()>&& handler)
{
    m_cookieObserverHandler = WTFMove(handler);
}

#if USE(LIBSECRET)
static const char* schemeFromProtectionSpaceServerType(ProtectionSpaceServerType serverType)
{
    switch (serverType) {
    case ProtectionSpaceServerHTTP:
    case ProtectionSpaceProxyHTTP:
        return SOUP_URI_SCHEME_HTTP;
    case ProtectionSpaceServerHTTPS:
    case ProtectionSpaceProxyHTTPS:
        return SOUP_URI_SCHEME_HTTPS;
    case ProtectionSpaceServerFTP:
    case ProtectionSpaceProxyFTP:
        return SOUP_URI_SCHEME_FTP;
    case ProtectionSpaceServerFTPS:
    case ProtectionSpaceProxySOCKS:
        break;
    }

    ASSERT_NOT_REACHED();
    return SOUP_URI_SCHEME_HTTP;
}

static const char* authTypeFromProtectionSpaceAuthenticationScheme(ProtectionSpaceAuthenticationScheme scheme)
{
    switch (scheme) {
    case ProtectionSpaceAuthenticationSchemeDefault:
    case ProtectionSpaceAuthenticationSchemeHTTPBasic:
        return "Basic";
    case ProtectionSpaceAuthenticationSchemeHTTPDigest:
        return "Digest";
    case ProtectionSpaceAuthenticationSchemeNTLM:
        return "NTLM";
    case ProtectionSpaceAuthenticationSchemeNegotiate:
        return "Negotiate";
    case ProtectionSpaceAuthenticationSchemeHTMLForm:
    case ProtectionSpaceAuthenticationSchemeClientCertificateRequested:
    case ProtectionSpaceAuthenticationSchemeServerTrustEvaluationRequested:
        ASSERT_NOT_REACHED();
        break;
    case ProtectionSpaceAuthenticationSchemeUnknown:
        return "unknown";
    }

    ASSERT_NOT_REACHED();
    return "unknown";
}

struct SecretServiceSearchData {
    SecretServiceSearchData(GCancellable* cancellable, Function<void (Credential&&)>&& completionHandler)
        : cancellable(cancellable)
        , completionHandler(WTFMove(completionHandler))
    {
    }

    ~SecretServiceSearchData() = default;

    GRefPtr<GCancellable> cancellable;
    Function<void (Credential&&)> completionHandler;
};
#endif // USE(LIBSECRET)

void NetworkStorageSession::getCredentialFromPersistentStorage(const ProtectionSpace& protectionSpace, GCancellable* cancellable, Function<void (Credential&&)>&& completionHandler)
{
#if USE(LIBSECRET)
    if (m_sessionID.isEphemeral()) {
        completionHandler({ });
        return;
    }

    const String& realm = protectionSpace.realm();
    if (realm.isEmpty()) {
        completionHandler({ });
        return;
    }

    GRefPtr<GHashTable> attributes = adoptGRef(secret_attributes_build(SECRET_SCHEMA_COMPAT_NETWORK,
        "domain", realm.utf8().data(),
        "server", protectionSpace.host().utf8().data(),
        "port", protectionSpace.port(),
        "protocol", schemeFromProtectionSpaceServerType(protectionSpace.serverType()),
        "authtype", authTypeFromProtectionSpaceAuthenticationScheme(protectionSpace.authenticationScheme()),
        nullptr));
    if (!attributes) {
        completionHandler({ });
        return;
    }

    auto data = std::make_unique<SecretServiceSearchData>(cancellable, WTFMove(completionHandler));
    secret_service_search(nullptr, SECRET_SCHEMA_COMPAT_NETWORK, attributes.get(),
        static_cast<SecretSearchFlags>(SECRET_SEARCH_UNLOCK | SECRET_SEARCH_LOAD_SECRETS), cancellable,
        [](GObject* source, GAsyncResult* result, gpointer userData) {
            auto data = std::unique_ptr<SecretServiceSearchData>(static_cast<SecretServiceSearchData*>(userData));
            GUniqueOutPtr<GError> error;
            GUniquePtr<GList> elements(secret_service_search_finish(SECRET_SERVICE(source), result, &error.outPtr()));
            if (g_cancellable_is_cancelled(data->cancellable.get()) || error || !elements || !elements->data) {
                data->completionHandler({ });
                return;
            }

            GRefPtr<SecretItem> secretItem = static_cast<SecretItem*>(elements->data);
            g_list_foreach(elements.get(), reinterpret_cast<GFunc>(g_object_unref), nullptr);
            GRefPtr<GHashTable> attributes = adoptGRef(secret_item_get_attributes(secretItem.get()));
            String user = String::fromUTF8(static_cast<const char*>(g_hash_table_lookup(attributes.get(), "user")));
            if (user.isEmpty()) {
                data->completionHandler({ });
                return;
            }

            size_t length;
            GRefPtr<SecretValue> secretValue = adoptGRef(secret_item_get_secret(secretItem.get()));
            const char* passwordData = secret_value_get(secretValue.get(), &length);
            data->completionHandler(Credential(user, String::fromUTF8(passwordData, length), CredentialPersistencePermanent));
        }, data.release());
#else
    UNUSED_PARAM(protectionSpace);
    UNUSED_PARAM(cancellable);
    completionHandler({ });
#endif
}

void NetworkStorageSession::saveCredentialToPersistentStorage(const ProtectionSpace& protectionSpace, const Credential& credential)
{
#if USE(LIBSECRET)
    if (m_sessionID.isEphemeral())
        return;

    if (credential.isEmpty())
        return;

    const String& realm = protectionSpace.realm();
    if (realm.isEmpty())
        return;

    GRefPtr<GHashTable> attributes = adoptGRef(secret_attributes_build(SECRET_SCHEMA_COMPAT_NETWORK,
        "domain", realm.utf8().data(),
        "server", protectionSpace.host().utf8().data(),
        "port", protectionSpace.port(),
        "protocol", schemeFromProtectionSpaceServerType(protectionSpace.serverType()),
        "authtype", authTypeFromProtectionSpaceAuthenticationScheme(protectionSpace.authenticationScheme()),
        nullptr));
    if (!attributes)
        return;

    g_hash_table_insert(attributes.get(), g_strdup("user"), g_strdup(credential.user().utf8().data()));
    CString utf8Password = credential.password().utf8();
    GRefPtr<SecretValue> newSecretValue = adoptGRef(secret_value_new(utf8Password.data(), utf8Password.length(), "text/plain"));
    secret_service_store(nullptr, SECRET_SCHEMA_COMPAT_NETWORK, attributes.get(), SECRET_COLLECTION_DEFAULT, _("WebKitGTK+ password"),
        newSecretValue.get(), nullptr, nullptr, nullptr);
#else
    UNUSED_PARAM(protectionSpace);
    UNUSED_PARAM(credential);
#endif
}

void NetworkStorageSession::setCookies(const Vector<Cookie>& cookies, const URL&, const URL&)
{
    for (auto cookie : cookies)
        soup_cookie_jar_add_cookie(cookieStorage(), cookie.toSoupCookie());
}

void NetworkStorageSession::setCookie(const Cookie& cookie)
{
    soup_cookie_jar_add_cookie(cookieStorage(), cookie.toSoupCookie());
}

void NetworkStorageSession::deleteCookie(const Cookie& cookie)
{
    GUniquePtr<SoupCookie> targetCookie(cookie.toSoupCookie());
    soup_cookie_jar_delete_cookie(cookieStorage(), targetCookie.get());
}

Vector<Cookie> NetworkStorageSession::getAllCookies()
{
    // FIXME: Implement for WK2 to use.
    return { };
}

Vector<Cookie> NetworkStorageSession::getCookies(const URL& url)
{
    Vector<Cookie> cookies;
    GUniquePtr<SoupURI> uri = url.createSoupURI();
    GUniquePtr<GSList> cookiesList(soup_cookie_jar_get_cookie_list(cookieStorage(), uri.get(), TRUE));
    for (GSList* item = cookiesList.get(); item; item = g_slist_next(item)) {
        GUniquePtr<SoupCookie> soupCookie(static_cast<SoupCookie*>(item->data));
        cookies.append(WebCore::Cookie(soupCookie.get()));
    }

    return cookies;
}

void NetworkStorageSession::flushCookieStore()
{
    // FIXME: Implement for WK2 to use.
}

} // namespace WebCore

#endif // USE(SOUP)
