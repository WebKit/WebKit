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
#include "CookieRequestHeaderFieldProxy.h"
#include "GUniquePtrSoup.h"
#include "ResourceHandle.h"
#include "SoupNetworkSession.h"
#include "URLSoup.h"
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

SoupNetworkSession& NetworkStorageSession::getOrCreateSoupNetworkSession() const
{
    if (!m_session)
        m_session = std::make_unique<SoupNetworkSession>(m_sessionID, m_cookieStorage.get());
    return *m_session;
}

void NetworkStorageSession::clearSoupNetworkSessionAndCookieStorage()
{
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
    case ProtectionSpaceAuthenticationSchemeOAuth:
        return "OAuth";
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
            g_list_foreach(elements.get(), reinterpret_cast<GFunc>(reinterpret_cast<GCallback>(g_object_unref)), nullptr);
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

bool NetworkStorageSession::cookiesEnabled() const
{
    auto policy = soup_cookie_jar_get_accept_policy(cookieStorage());
    return policy == SOUP_COOKIE_JAR_ACCEPT_ALWAYS || policy == SOUP_COOKIE_JAR_ACCEPT_NO_THIRD_PARTY;
}

static inline bool httpOnlyCookieExists(const GSList* cookies, const gchar* name, const gchar* path)
{
    for (const GSList* iter = cookies; iter; iter = g_slist_next(iter)) {
        SoupCookie* cookie = static_cast<SoupCookie*>(iter->data);
        if (!strcmp(soup_cookie_get_name(cookie), name) 
            && !g_strcmp0(soup_cookie_get_path(cookie), path)) {
            if (soup_cookie_get_http_only(cookie))
                return true;
            break;
        }
    }
    return false;
}

void NetworkStorageSession::setCookiesFromDOM(const URL& firstParty, const SameSiteInfo&, const URL& url, Optional<uint64_t> frameID, Optional<uint64_t> pageID, const String& value) const
{
    UNUSED_PARAM(frameID);
    UNUSED_PARAM(pageID);
    GUniquePtr<SoupURI> origin = urlToSoupURI(url);
    if (!origin)
        return;

    GUniquePtr<SoupURI> firstPartyURI = urlToSoupURI(firstParty);
    if (!firstPartyURI)
        return;

    // Get existing cookies for this origin.
    SoupCookieJar* jar = cookieStorage();
    GSList* existingCookies = soup_cookie_jar_get_cookie_list(jar, origin.get(), TRUE);

    for (auto& cookieString : value.split('\n')) {
        GUniquePtr<SoupCookie> cookie(soup_cookie_parse(cookieString.utf8().data(), origin.get()));

        if (!cookie)
            continue;

        // Make sure the cookie is not httpOnly since such cookies should not be set from JavaScript.
        if (soup_cookie_get_http_only(cookie.get()))
            continue;

        // Make sure we do not overwrite httpOnly cookies from JavaScript.
        if (httpOnlyCookieExists(existingCookies, soup_cookie_get_name(cookie.get()), soup_cookie_get_path(cookie.get())))
            continue;

        soup_cookie_jar_add_cookie_with_first_party(jar, firstPartyURI.get(), cookie.release());
    }

    soup_cookies_free(existingCookies);
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

void NetworkStorageSession::deleteCookie(const URL& url, const String& name) const
{
    GUniquePtr<SoupURI> uri = urlToSoupURI(url);
    if (!uri)
        return;

    SoupCookieJar* jar = cookieStorage();
    GUniquePtr<GSList> cookies(soup_cookie_jar_get_cookie_list(jar, uri.get(), TRUE));
    if (!cookies)
        return;

    CString cookieName = name.utf8();
    bool wasDeleted = false;
    for (GSList* iter = cookies.get(); iter; iter = g_slist_next(iter)) {
        SoupCookie* cookie = static_cast<SoupCookie*>(iter->data);
        if (!wasDeleted && cookieName == cookie->name) {
            soup_cookie_jar_delete_cookie(jar, cookie);
            wasDeleted = true;
        }
        soup_cookie_free(cookie);
    }
}

void NetworkStorageSession::deleteAllCookies()
{
    SoupCookieJar* cookieJar = cookieStorage();
    GUniquePtr<GSList> cookies(soup_cookie_jar_all_cookies(cookieJar));
    for (GSList* item = cookies.get(); item; item = g_slist_next(item)) {
        SoupCookie* cookie = static_cast<SoupCookie*>(item->data);
        soup_cookie_jar_delete_cookie(cookieJar, cookie);
        soup_cookie_free(cookie);
    }
}

void NetworkStorageSession::deleteAllCookiesModifiedSince(WallTime timestamp)
{
    // FIXME: Add support for deleting cookies modified since the given timestamp. It should probably be added to libsoup.
    if (timestamp == WallTime::fromRawSeconds(0))
        deleteAllCookies();
    else
        g_warning("Deleting cookies modified since a given time span is not supported yet");
}

void NetworkStorageSession::deleteCookiesForHostnames(const Vector<String>& hostnames)
{
    SoupCookieJar* cookieJar = cookieStorage();

    for (const auto& hostname : hostnames) {
        CString hostNameString = hostname.utf8();

        GUniquePtr<GSList> cookies(soup_cookie_jar_all_cookies(cookieJar));
        for (GSList* item = cookies.get(); item; item = g_slist_next(item)) {
            SoupCookie* cookie = static_cast<SoupCookie*>(item->data);
            if (soup_cookie_domain_matches(cookie, hostNameString.data()))
                soup_cookie_jar_delete_cookie(cookieJar, cookie);
            soup_cookie_free(cookie);
        }
    }
}

void NetworkStorageSession::getHostnamesWithCookies(HashSet<String>& hostnames)
{
    GUniquePtr<GSList> cookies(soup_cookie_jar_all_cookies(cookieStorage()));
    for (GSList* item = cookies.get(); item; item = g_slist_next(item)) {
        SoupCookie* cookie = static_cast<SoupCookie*>(item->data);
        if (cookie->domain)
            hostnames.add(String::fromUTF8(cookie->domain));
        soup_cookie_free(cookie);
    }
}

Vector<Cookie> NetworkStorageSession::getAllCookies()
{
    // FIXME: Implement for WK2 to use.
    return { };
}

Vector<Cookie> NetworkStorageSession::getCookies(const URL& url)
{
    Vector<Cookie> cookies;
    GUniquePtr<SoupURI> uri = urlToSoupURI(url);
    if (!uri)
        return cookies;

    GUniquePtr<GSList> cookiesList(soup_cookie_jar_get_cookie_list(cookieStorage(), uri.get(), TRUE));
    for (GSList* item = cookiesList.get(); item; item = g_slist_next(item)) {
        GUniquePtr<SoupCookie> soupCookie(static_cast<SoupCookie*>(item->data));
        cookies.append(WebCore::Cookie(soupCookie.get()));
    }

    return cookies;
}

bool NetworkStorageSession::getRawCookies(const URL& firstParty, const SameSiteInfo&, const URL& url, Optional<uint64_t> frameID, Optional<uint64_t> pageID, Vector<Cookie>& rawCookies) const
{
    UNUSED_PARAM(firstParty);
    UNUSED_PARAM(frameID);
    UNUSED_PARAM(pageID);
    rawCookies.clear();
    GUniquePtr<SoupURI> uri = urlToSoupURI(url);
    if (!uri)
        return false;

    GUniquePtr<GSList> cookies(soup_cookie_jar_get_cookie_list(cookieStorage(), uri.get(), TRUE));
    if (!cookies)
        return false;

    for (GSList* iter = cookies.get(); iter; iter = g_slist_next(iter)) {
        SoupCookie* soupCookie = static_cast<SoupCookie*>(iter->data);
        Cookie cookie;
        cookie.name = String::fromUTF8(soupCookie->name);
        cookie.value = String::fromUTF8(soupCookie->value);
        cookie.domain = String::fromUTF8(soupCookie->domain);
        cookie.path = String::fromUTF8(soupCookie->path);
        cookie.created = 0;
        cookie.expires = soupCookie->expires ? static_cast<double>(soup_date_to_time_t(soupCookie->expires)) * 1000 : 0;
        cookie.httpOnly = soupCookie->http_only;
        cookie.secure = soupCookie->secure;
        cookie.session = !soupCookie->expires;
        rawCookies.append(WTFMove(cookie));
        soup_cookie_free(soupCookie);
    }

    return true;
}

static std::pair<String, bool> cookiesForSession(const NetworkStorageSession& session, const URL& url, bool forHTTPHeader, IncludeSecureCookies includeSecureCookies)
{
    GUniquePtr<SoupURI> uri = urlToSoupURI(url);
    if (!uri)
        return { { }, false };

    GSList* cookies = soup_cookie_jar_get_cookie_list(session.cookieStorage(), uri.get(), forHTTPHeader);
    bool didAccessSecureCookies = false;

    // libsoup should omit secure cookies itself if the protocol is not https.
    if (url.protocolIs("https")) {
        GSList* item = cookies;
        while (item) {
            auto cookie = static_cast<SoupCookie*>(item->data);
            if (soup_cookie_get_secure(cookie)) {
                didAccessSecureCookies = true;
                if (includeSecureCookies == IncludeSecureCookies::No) {
                    GSList* next = item->next;
                    soup_cookie_free(static_cast<SoupCookie*>(item->data));
                    cookies = g_slist_remove_link(cookies, item);
                    item = next;
                    continue;
                }
            }
            item = item->next;
        }
    }

    if (!cookies)
        return { { }, false };

    GUniquePtr<char> cookieHeader(soup_cookies_to_cookie_header(cookies));
    soup_cookies_free(cookies);

    return { String::fromUTF8(cookieHeader.get()), didAccessSecureCookies };
}

std::pair<String, bool> NetworkStorageSession::cookiesForDOM(const URL& firstParty, const SameSiteInfo&, const URL& url, Optional<uint64_t> frameID, Optional<uint64_t> pageID, IncludeSecureCookies includeSecureCookies) const
{
    UNUSED_PARAM(firstParty);
    UNUSED_PARAM(frameID);
    UNUSED_PARAM(pageID);
    return cookiesForSession(*this, url, false, includeSecureCookies);
}

std::pair<String, bool> NetworkStorageSession::cookieRequestHeaderFieldValue(const URL& firstParty, const SameSiteInfo&, const URL& url, Optional<uint64_t> frameID, Optional<uint64_t> pageID, IncludeSecureCookies includeSecureCookies) const
{
    UNUSED_PARAM(firstParty);
    UNUSED_PARAM(frameID);
    UNUSED_PARAM(pageID);
    // Secure cookies will still only be included if url's protocol is https.
    return cookiesForSession(*this, url, true, includeSecureCookies);
}

std::pair<String, bool> NetworkStorageSession::cookieRequestHeaderFieldValue(const CookieRequestHeaderFieldProxy& headerFieldProxy) const
{
    return cookieRequestHeaderFieldValue(headerFieldProxy.firstParty, headerFieldProxy.sameSiteInfo, headerFieldProxy.url, headerFieldProxy.frameID, headerFieldProxy.pageID, headerFieldProxy.includeSecureCookies);
}

void NetworkStorageSession::flushCookieStore()
{
    // FIXME: Implement for WK2 to use.
}

} // namespace WebCore

#endif // USE(SOUP)
