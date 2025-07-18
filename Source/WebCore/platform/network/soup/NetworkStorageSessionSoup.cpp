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
#include "CookieStoreGetOptions.h"
#include "GUniquePtrSoup.h"
#include "HTTPCookieAcceptPolicy.h"
#include "SoupNetworkSession.h"
#include "URLSoup.h"
#include <libsoup/soup.h>
#include <optional>
#include <wtf/DateMath.h>
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/Vector.h>
#include <wtf/glib/GUniquePtr.h>

#if USE(LIBSECRET)
#include "GRefPtrGtk.h"
#include <glib/gi18n-lib.h>
#define SECRET_WITH_UNSTABLE 1
#define SECRET_API_SUBJECT_TO_CHANGE 1
#include <libsecret/secret.h>
#endif

namespace WebCore {

enum class ForHTTPHeader : bool { No, Yes };

template <typename T, auto fn>
struct Deleter {
    void operator()(T* ptr) { fn(ptr); }
};

using CookieList = std::unique_ptr<GSList, Deleter<GSList, soup_cookies_free>>;

NetworkStorageSession::NetworkStorageSession(PAL::SessionID sessionID, IsInMemoryCookieStore isInMemoryCookieStore)
    : m_sessionID(sessionID)
    , m_isInMemoryCookieStore(isInMemoryCookieStore == IsInMemoryCookieStore::Yes)
    , m_cookieAcceptPolicy(HTTPCookieAcceptPolicy::ExclusivelyFromMainDocumentDomain)
    , m_cookieStorage(adoptGRef(soup_cookie_jar_new()))
{
    setCookieAcceptPolicy(m_cookieAcceptPolicy);
    g_signal_connect_swapped(m_cookieStorage.get(), "changed", G_CALLBACK(cookiesDidChange), this);
}

NetworkStorageSession::~NetworkStorageSession()
{
    clearCookiesVersionChangeCallbacks();
    g_signal_handlers_disconnect_matched(m_cookieStorage.get(), G_SIGNAL_MATCH_DATA, 0, 0, nullptr, nullptr, this);
}

#if HAVE(COOKIE_CHANGE_LISTENER_API)
void NetworkStorageSession::notifyCookie(SoupCookie* cookie, bool added)
{
    if (soup_cookie_get_http_only(cookie))
        return;

    for (const auto& host : m_cookieChangeObservers.keys()) {
        if (!soup_cookie_domain_matches(cookie, host.utf8().data()))
            continue;

        auto observers = m_cookieChangeObservers.getOptional(host);
        if (!observers)
            continue;

        for (auto& observer : *observers) {
            if (added)
                observer.cookiesAdded(host, { Cookie(cookie) });
            else
                observer.cookiesDeleted(host, { Cookie(cookie) });
        }
    }
}

void NetworkStorageSession::notifyCookieAdded(SoupCookie* newCookie)
{
    notifyCookie(newCookie, true);
}

void NetworkStorageSession::notifyCookieDeleted(SoupCookie* oldCookie)
{
    notifyCookie(oldCookie, false);
}
#endif

void NetworkStorageSession::cookiesDidChange(NetworkStorageSession* session, SoupCookie* oldCookie, SoupCookie* newCookie, SoupCookieJar*)
{
    if (session->m_cookieObserverHandler)
        session->m_cookieObserverHandler();

#if HAVE(COOKIE_CHANGE_LISTENER_API)
    if (session->m_cookieChangeObservers.isEmpty())
        return;

    // FIXME: This adds/removes cookies one at a time instead of a vector.

    // In the case of a new cookie *or* an updated cookie we notify the observer.
    // Adding a cookie that matches (name & path) the old one to the cookie-jar replaces it.
    if (newCookie) {
        session->notifyCookieAdded(newCookie);
        return;
    }

    if (oldCookie) {
        session->notifyCookieDeleted(oldCookie);
        return;
    }

#else
    UNUSED_PARAM(oldCookie);
    UNUSED_PARAM(newCookie);
#endif
}

void NetworkStorageSession::setCookieStorage(GRefPtr<SoupCookieJar>&& jar)
{
    ASSERT(!m_isInMemoryCookieStore);

    g_signal_handlers_disconnect_matched(m_cookieStorage.get(), G_SIGNAL_MATCH_DATA, 0, 0, nullptr, nullptr, this);
    soup_cookie_jar_set_accept_policy(jar.get(), soup_cookie_jar_get_accept_policy(m_cookieStorage.get()));
    m_cookieStorage = WTFMove(jar);
    g_signal_connect_swapped(m_cookieStorage.get(), "changed", G_CALLBACK(cookiesDidChange), this);

#if HAVE(COOKIE_CHANGE_LISTENER_API)
    for (auto& [host, observers] : m_cookieChangeObservers) {
        for (auto& observer : observers)
            observer.allCookiesDeleted();
    }

    CookieList cookies(soup_cookie_jar_all_cookies(m_cookieStorage.get()));
    for (GSList* item = cookies.get(); item; item = g_slist_next(item)) {
        auto* soupCookie = static_cast<SoupCookie*>(item->data);
        notifyCookieAdded(soupCookie);
    }
#endif
}

void NetworkStorageSession::setCookieObserverHandler(Function<void ()>&& handler)
{
    m_cookieObserverHandler = WTFMove(handler);
}

#if USE(LIBSECRET)
static ASCIILiteral schemeFromProtectionSpaceServerType(ProtectionSpace::ServerType serverType)
{
    switch (serverType) {
    case ProtectionSpace::ServerType::HTTP:
    case ProtectionSpace::ServerType::ProxyHTTP:
        return "http"_s;
    case ProtectionSpace::ServerType::HTTPS:
    case ProtectionSpace::ServerType::ProxyHTTPS:
        return "https"_s;
    case ProtectionSpace::ServerType::FTP:
    case ProtectionSpace::ServerType::ProxyFTP:
        return "ftp"_s;
    case ProtectionSpace::ServerType::FTPS:
    case ProtectionSpace::ServerType::ProxySOCKS:
        break;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

static ASCIILiteral authTypeFromProtectionSpaceAuthenticationScheme(ProtectionSpace::AuthenticationScheme scheme)
{
    switch (scheme) {
    case ProtectionSpace::AuthenticationScheme::Default:
    case ProtectionSpace::AuthenticationScheme::HTTPBasic:
        return "Basic"_s;
    case ProtectionSpace::AuthenticationScheme::HTTPDigest:
        return "Digest"_s;
    case ProtectionSpace::AuthenticationScheme::NTLM:
        return "NTLM"_s;
    case ProtectionSpace::AuthenticationScheme::Negotiate:
        return "Negotiate"_s;
    case ProtectionSpace::AuthenticationScheme::HTMLForm:
    case ProtectionSpace::AuthenticationScheme::ClientCertificateRequested:
    case ProtectionSpace::AuthenticationScheme::ServerTrustEvaluationRequested:
        ASSERT_NOT_REACHED();
        break;
    case ProtectionSpace::AuthenticationScheme::ClientCertificatePINRequested:
        return "Certificate PIN"_s;
    case ProtectionSpace::AuthenticationScheme::OAuth:
        return "OAuth"_s;
    case ProtectionSpace::AuthenticationScheme::Unknown:
        return "unknown"_s;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

struct SecretServiceSearchData {
    WTF_DEPRECATED_MAKE_STRUCT_FAST_ALLOCATED(SecretServiceSearchData);
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
    ASSERT(!m_isInMemoryCookieStore);

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
        "protocol", schemeFromProtectionSpaceServerType(protectionSpace.serverType()).characters(),
        "authtype", authTypeFromProtectionSpaceAuthenticationScheme(protectionSpace.authenticationScheme()).characters(),
        nullptr));
    if (!attributes) {
        completionHandler({ });
        return;
    }

    auto data = makeUnique<SecretServiceSearchData>(cancellable, WTFMove(completionHandler));
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
            data->completionHandler(Credential(user, String::fromUTF8(unsafeMakeSpan(passwordData, length)), CredentialPersistence::Permanent));
        }, data.release());
#else
    UNUSED_PARAM(protectionSpace);
    UNUSED_PARAM(cancellable);
    completionHandler({ });
#endif
}

void NetworkStorageSession::saveCredentialToPersistentStorage(const ProtectionSpace& protectionSpace, const Credential& credential)
{
    ASSERT(!m_isInMemoryCookieStore);

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
        "protocol", schemeFromProtectionSpaceServerType(protectionSpace.serverType()).characters(),
        "authtype", authTypeFromProtectionSpaceAuthenticationScheme(protectionSpace.authenticationScheme()).characters(),
        nullptr));
    if (!attributes)
        return;

    g_hash_table_insert(attributes.get(), g_strdup("user"), g_strdup(credential.user().utf8().data()));
    CString utf8Password = credential.password().utf8();
    GRefPtr<SecretValue> newSecretValue = adoptGRef(secret_value_new(utf8Password.data(), utf8Password.length(), "text/plain"));
    secret_service_store(nullptr, SECRET_SCHEMA_COMPAT_NETWORK, attributes.get(), SECRET_COLLECTION_DEFAULT, _("WebKitGTK password"),
        newSecretValue.get(), nullptr, nullptr, nullptr);
#else
    UNUSED_PARAM(protectionSpace);
    UNUSED_PARAM(credential);
#endif
}

void NetworkStorageSession::setCookieAcceptPolicy(HTTPCookieAcceptPolicy policy)
{
    if (m_isTrackingPreventionEnabled && m_thirdPartyCookieBlockingMode == ThirdPartyCookieBlockingMode::All) {
        m_cookieAcceptPolicy = policy;
        if (m_cookieAcceptPolicy == HTTPCookieAcceptPolicy::ExclusivelyFromMainDocumentDomain)
            policy = HTTPCookieAcceptPolicy::AlwaysAccept;
    }

    SoupCookieJarAcceptPolicy soupPolicy = SOUP_COOKIE_JAR_ACCEPT_NO_THIRD_PARTY;
    switch (policy) {
    case HTTPCookieAcceptPolicy::AlwaysAccept:
        soupPolicy = SOUP_COOKIE_JAR_ACCEPT_ALWAYS;
        break;
    case HTTPCookieAcceptPolicy::Never:
        soupPolicy = SOUP_COOKIE_JAR_ACCEPT_NEVER;
        break;
    case HTTPCookieAcceptPolicy::OnlyFromMainDocumentDomain:
#if SOUP_CHECK_VERSION(2, 71, 0)
        soupPolicy = SOUP_COOKIE_JAR_ACCEPT_GRANDFATHERED_THIRD_PARTY;
        break;
#else
        [[fallthrough]];
#endif
    case HTTPCookieAcceptPolicy::ExclusivelyFromMainDocumentDomain:
        soupPolicy = SOUP_COOKIE_JAR_ACCEPT_NO_THIRD_PARTY;
        break;
    }

    soup_cookie_jar_set_accept_policy(cookieStorage(), soupPolicy);
}

HTTPCookieAcceptPolicy NetworkStorageSession::cookieAcceptPolicy() const
{
    switch (soup_cookie_jar_get_accept_policy(cookieStorage())) {
    case SOUP_COOKIE_JAR_ACCEPT_ALWAYS:
        return HTTPCookieAcceptPolicy::AlwaysAccept;
    case SOUP_COOKIE_JAR_ACCEPT_NO_THIRD_PARTY:
        return HTTPCookieAcceptPolicy::ExclusivelyFromMainDocumentDomain;
#if SOUP_CHECK_VERSION(2, 71, 0)
    case SOUP_COOKIE_JAR_ACCEPT_GRANDFATHERED_THIRD_PARTY:
        return HTTPCookieAcceptPolicy::OnlyFromMainDocumentDomain;
#endif
    case SOUP_COOKIE_JAR_ACCEPT_NEVER:
        return HTTPCookieAcceptPolicy::Never;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

void NetworkStorageSession::setTrackingPreventionEnabled(bool enabled)
{
    if (enabled) {
        m_cookieAcceptPolicy = cookieAcceptPolicy();
        if (m_thirdPartyCookieBlockingMode == ThirdPartyCookieBlockingMode::All && m_cookieAcceptPolicy == HTTPCookieAcceptPolicy::ExclusivelyFromMainDocumentDomain)
            setCookieAcceptPolicy(HTTPCookieAcceptPolicy::AlwaysAccept);
        m_isTrackingPreventionEnabled = true;
    } else {
        m_isTrackingPreventionEnabled = false;
        setCookieAcceptPolicy(m_cookieAcceptPolicy);
    }
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

void NetworkStorageSession::setCookiesFromDOM(const URL& firstParty, const SameSiteInfo&, const URL& url, std::optional<FrameIdentifier> frameID, std::optional<PageIdentifier> pageID, ApplyTrackingPrevention applyTrackingPrevention, RequiresScriptTrackingPrivacy requiresScriptTrackingPrivacy, const String& value, ShouldRelaxThirdPartyCookieBlocking relaxThirdPartyCookieBlocking) const
{
    if (applyTrackingPrevention == ApplyTrackingPrevention::Yes && shouldBlockCookies(firstParty, url, frameID, pageID, relaxThirdPartyCookieBlocking))
        return;

    auto origin = urlToSoupURI(url);
    if (!origin)
        return;

    auto firstPartyURI = urlToSoupURI(firstParty);
    if (!firstPartyURI)
        return;

    auto cappedLifetime = clientSideCookieCap(RegistrableDomain { firstParty }, requiresScriptTrackingPrivacy, pageID);

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

        // Cap lifetime of persistent, client-side cookies to a week.
        if (cappedLifetime) {
            if (auto* expiresDate = soup_cookie_get_expires(cookie.get())) {
#if USE(SOUP2)
                auto timeIntervalSinceNow = Seconds(static_cast<double>(soup_date_to_time_t(expiresDate))) - WallTime::now().secondsSinceEpoch();
#else
                auto timeIntervalSinceNow = Seconds(static_cast<double>(g_date_time_to_unix(expiresDate))) - WallTime::now().secondsSinceEpoch();
#endif
                if (timeIntervalSinceNow > cappedLifetime.value())
                    soup_cookie_set_max_age(cookie.get(), cappedLifetime->secondsAs<int>());
            }
        }

#if SOUP_CHECK_VERSION(2, 67, 1)
        soup_cookie_jar_add_cookie_full(jar, cookie.release(), origin.get(), firstPartyURI.get());
#else
        soup_cookie_jar_add_cookie_with_first_party(jar, firstPartyURI.get(), cookie.release());
#endif
    }

    soup_cookies_free(existingCookies);
}

bool NetworkStorageSession::setCookieFromDOM(const URL& firstParty, const SameSiteInfo& sameSiteInfo, const URL& url, std::optional<FrameIdentifier> frameID, std::optional<PageIdentifier> pageID, ApplyTrackingPrevention applyTrackingPrevention, RequiresScriptTrackingPrivacy, const Cookie& cookie, ShouldRelaxThirdPartyCookieBlocking shouldRelaxThirdPartyCookieBlocking) const
{
    if (applyTrackingPrevention == ApplyTrackingPrevention::Yes && shouldBlockCookies(firstParty, url, frameID, pageID, shouldRelaxThirdPartyCookieBlocking))
        return false;

    GUniquePtr<SoupCookie> soupCookie(cookie.toSoupCookie());
    if (!soupCookie)
        return false;

    auto uri = urlToSoupURI(url);
    if (!uri)
        return false;

    auto firstPartyURI = urlToSoupURI(firstParty);
    if (!firstPartyURI)
        return false;

    // FIXME: We can't set any of these properties when making a cookie, I'm not sure why it is here.
    UNUSED_PARAM(sameSiteInfo);

    // Ensure DOM can't ovewrite http-only cookies.
    GSList* existingCookies = soup_cookie_jar_get_cookie_list(cookieStorage(), uri.get(), TRUE);
    if (httpOnlyCookieExists(existingCookies, soup_cookie_get_name(soupCookie.get()), soup_cookie_get_path(soupCookie.get()))) {
        soup_cookies_free(existingCookies);
        return false;
    }
    soup_cookies_free(existingCookies);

#if SOUP_CHECK_VERSION(2, 67, 1)
    soup_cookie_jar_add_cookie_full(cookieStorage(), soupCookie.release(), uri.get(), firstPartyURI.get());
#else
    soup_cookie_jar_add_cookie_with_first_party(cookieStorage(), firstPartyURI.get(), soupCookie.release());
    UNUSED_PARAM(uri);
#endif

    return true;
}

void NetworkStorageSession::setCookies(const Vector<Cookie>& cookies, const URL& url, const URL& firstParty)
{
    for (auto cookie : cookies) {
#if SOUP_CHECK_VERSION(2, 67, 1)
        auto origin = urlToSoupURI(url);
        auto firstPartyURI = urlToSoupURI(firstParty);

        soup_cookie_jar_add_cookie_full(cookieStorage(), cookie.toSoupCookie(), origin.get(), firstPartyURI.get());
#else
        UNUSED_PARAM(url);
        UNUSED_PARAM(firstParty);
        soup_cookie_jar_add_cookie(cookieStorage(), cookie.toSoupCookie());
#endif
    }
}

void NetworkStorageSession::setCookie(const Cookie& cookie)
{
    soup_cookie_jar_add_cookie(cookieStorage(), cookie.toSoupCookie());
}

void NetworkStorageSession::setCookie(const Cookie& cookie, const URL&, const URL&)
{
    setCookie(cookie);
}

void NetworkStorageSession::replaceCookies(const Vector<Cookie>& cookies)
{
    ASSERT(!m_isInMemoryCookieStore);

    SoupCookieJar* jar = cookieStorage();

    // Delete existing cookies and add the new ones. During the process, disable
    // signals from the cookie jar so we don't get a ton of them.
    guint signalId = g_signal_lookup("changed", soup_cookie_jar_get_type());
    gulong handler = g_signal_handler_find(jar, G_SIGNAL_MATCH_ID, signalId, 0, nullptr, nullptr, nullptr);
    g_signal_handler_block(jar, handler);

    deleteAllCookies([] { });
    for (const auto& cookie : cookies)
        soup_cookie_jar_add_cookie(jar, cookie.toSoupCookie());

    g_signal_handler_unblock(jar, handler);

    // Emit one "changed" signal at the end.
    // FIXME: This isn't correct usage of this signal, libsoup should have a way to
    //        signal multiple cookies changing at once.
    g_signal_emit(jar, signalId, 0, nullptr, nullptr);
}

void NetworkStorageSession::deleteCookie(const Cookie& cookie, CompletionHandler<void()>&& completionHandler)
{
    GUniquePtr<SoupCookie> targetCookie(cookie.toSoupCookie());
    soup_cookie_jar_delete_cookie(cookieStorage(), targetCookie.get());
    completionHandler();
}

void NetworkStorageSession::deleteCookie(const URL&, const URL& url, const String& name, CompletionHandler<void()>&& completionHandler) const
{
    auto uri = urlToSoupURI(url);
    if (!uri)
        return completionHandler();

    SoupCookieJar* jar = cookieStorage();
    CookieList cookies(soup_cookie_jar_get_cookie_list(jar, uri.get(), TRUE));
    if (!cookies)
        return completionHandler();

    CString cookieName = name.utf8();
    bool wasDeleted = false;
    for (GSList* iter = cookies.get(); iter; iter = g_slist_next(iter)) {
        SoupCookie* cookie = static_cast<SoupCookie*>(iter->data);
        if (!wasDeleted && cookieName == soup_cookie_get_name(cookie)) {
            soup_cookie_jar_delete_cookie(jar, cookie);
            wasDeleted = true;
        }
    }
    completionHandler();
}

void NetworkStorageSession::deleteAllCookies(CompletionHandler<void()>&& completionHandler)
{
    SoupCookieJar* cookieJar = cookieStorage();
    CookieList cookies(soup_cookie_jar_all_cookies(cookieJar));
    for (GSList* item = cookies.get(); item; item = g_slist_next(item)) {
        auto* cookie = static_cast<SoupCookie*>(item->data);
        soup_cookie_jar_delete_cookie(cookieJar, cookie);
    }

#if HAVE(COOKIE_CHANGE_LISTENER_API)
    for (auto& [host, observers] : m_cookieChangeObservers) {
        for (auto& observer : observers)
            observer.allCookiesDeleted();
    }
#endif

    completionHandler();
}

void NetworkStorageSession::deleteAllCookiesModifiedSince(WallTime timestamp, CompletionHandler<void()>&& completionHandler)
{
    // FIXME: Add support for deleting cookies modified since the given timestamp. It should probably be added to libsoup.
    if (timestamp == WallTime::fromRawSeconds(0))
        deleteAllCookies(WTFMove(completionHandler));
    else {
        g_warning("Deleting cookies modified since a given time span is not supported yet");
        completionHandler();
    }
}

void NetworkStorageSession::deleteCookiesForHostnames(const Vector<String>& hostnames, IncludeHttpOnlyCookies includeHttpOnlyCookies, ScriptWrittenCookiesOnly, CompletionHandler<void()>&& completionHandler)
{
    SoupCookieJar* cookieJar = cookieStorage();
    for (const auto& hostname : hostnames) {
        CString hostNameString = hostname.utf8();

        CookieList cookies(soup_cookie_jar_all_cookies(cookieJar));
        for (auto* item = cookies.get(); item; item = g_slist_next(item)) {
            auto* cookie = static_cast<SoupCookie*>(item->data);
            if (includeHttpOnlyCookies == IncludeHttpOnlyCookies::No && soup_cookie_get_http_only(cookie))
                continue;

            if (soup_cookie_domain_matches(cookie, hostNameString.data()))
                soup_cookie_jar_delete_cookie(cookieJar, cookie);
        }
    }
    completionHandler();
}

void NetworkStorageSession::getHostnamesWithCookies(HashSet<String>& hostnames)
{
    CookieList cookies(soup_cookie_jar_all_cookies(cookieStorage()));
    for (GSList* item = cookies.get(); item; item = g_slist_next(item)) {
        auto* cookie = static_cast<SoupCookie*>(item->data);
        if (const char* domain = soup_cookie_get_domain(cookie))
            hostnames.add(String::fromUTF8(domain));
    }
}

Vector<Cookie> NetworkStorageSession::getAllCookies()
{
    Vector<Cookie> cookies;
    CookieList cookiesList(soup_cookie_jar_all_cookies(cookieStorage()));
    for (GSList* item = cookiesList.get(); item; item = g_slist_next(item)) {
        auto* soupCookie = static_cast<SoupCookie*>(item->data);
        cookies.insert(0, WebCore::Cookie(soupCookie));
    }
    return cookies;
}

Vector<Cookie> NetworkStorageSession::getCookies(const URL& url)
{
    Vector<Cookie> cookies;
    auto uri = urlToSoupURI(url);
    if (!uri)
        return cookies;

    CookieList cookiesList(soup_cookie_jar_get_cookie_list(cookieStorage(), uri.get(), TRUE));
    for (GSList* item = cookiesList.get(); item; item = g_slist_next(item)) {
        auto* soupCookie = static_cast<SoupCookie*>(item->data);
        cookies.append(WebCore::Cookie(soupCookie));
    }

    return cookies;
}

void NetworkStorageSession::hasCookies(const RegistrableDomain& domain, CompletionHandler<void(bool)>&& completionHandler) const
{
    CookieList cookies(soup_cookie_jar_all_cookies(cookieStorage()));
    for (auto* item = cookies.get(); item; item = g_slist_next(item)) {
        auto* cookie = static_cast<SoupCookie*>(item->data);
        if (RegistrableDomain::uncheckedCreateFromHost(String::fromLatin1(soup_cookie_get_domain(cookie))) == domain) {
            completionHandler(true);
            return;
        }
    }
    completionHandler(false);
}

static std::optional<CookieList> lookupCookies(const NetworkStorageSession& session, const URL& firstParty, const SameSiteInfo& sameSiteInfo, const URL& url, ForHTTPHeader forHTTPHeader, std::optional<FrameIdentifier> frameID, std::optional<PageIdentifier> pageID, ApplyTrackingPrevention applyTrackingPrevention, ShouldRelaxThirdPartyCookieBlocking shouldRelaxThirdPartyCookieBlocking, IncludeSecureCookies includeSecureCookies, bool* didAccessSecureCookies = nullptr)
{
    if (applyTrackingPrevention == ApplyTrackingPrevention::Yes && session.shouldBlockCookies(firstParty, url, frameID, pageID, shouldRelaxThirdPartyCookieBlocking))
        return nullptr;

    auto uri = urlToSoupURI(url);
    if (!uri)
        return std::nullopt;

#if SOUP_CHECK_VERSION(2, 69, 90)
    auto firstPartyURI = urlToSoupURI(firstParty);
    if (!firstPartyURI)
        return std::nullopt;

    auto cookieURI = sameSiteInfo.isSameSite ? urlToSoupURI(url) : nullptr;
    CookieList cookies(soup_cookie_jar_get_cookie_list_with_same_site_info(session.cookieStorage(), uri.get(), firstPartyURI.get(), cookieURI.get(), forHTTPHeader == ForHTTPHeader::Yes,
        sameSiteInfo.isSafeHTTPMethod, sameSiteInfo.isTopSite));
#else
    CookieList cookies(soup_cookie_jar_get_cookie_list(session.cookieStorage(), uri.get(), forHTTPHeader == ForHTTPHeader::Yes));
#endif
    if (!cookies)
        return nullptr;

    bool accessedSecureCookies = false;

    // libsoup should omit secure cookies itself if the protocol is not https.
    if (url.protocolIs("https"_s)) {
        GSList* item = cookies.get();
        while (item) {
            auto* cookie = static_cast<SoupCookie*>(item->data);
            if (soup_cookie_get_secure(cookie)) {
                accessedSecureCookies = true;
                if (includeSecureCookies == IncludeSecureCookies::No) {
                    GSList* next = item->next;

                    soup_cookie_free(static_cast<SoupCookie*>(item->data));
                    cookies = CookieList(g_slist_delete_link(cookies.release(), item));

                    item = next;
                    continue;
                }
            }
            item = item->next;
        }
    }

    if (didAccessSecureCookies)
        *didAccessSecureCookies = accessedSecureCookies;

    return cookies;
}

static std::pair<String, bool> lookupCookiesHeaders(const NetworkStorageSession& session, const URL& firstParty, const SameSiteInfo& sameSiteInfo, const URL& url, ForHTTPHeader forHTTPHeader, std::optional<FrameIdentifier> frameID, std::optional<PageIdentifier> pageID, ApplyTrackingPrevention applyTrackingPrevention, ShouldRelaxThirdPartyCookieBlocking shouldRelaxThirdPartyCookieBlocking, IncludeSecureCookies includeSecureCookies)
{
    bool didAccessSecureCookies = false;
    auto cookies = lookupCookies(
        session,
        firstParty,
        sameSiteInfo,
        url,
        forHTTPHeader,
        frameID,
        pageID,
        applyTrackingPrevention,
        shouldRelaxThirdPartyCookieBlocking,
        includeSecureCookies,
        &didAccessSecureCookies);

    if (!cookies || !*cookies)
        return { { }, false };

    GUniquePtr<char> cookieHeader(soup_cookies_to_cookie_header(cookies->get()));

    return { String::fromUTF8(cookieHeader.get()), didAccessSecureCookies };
}

bool NetworkStorageSession::getRawCookies(const URL& firstParty, const SameSiteInfo& sameSiteInfo, const URL& url, std::optional<FrameIdentifier> frameID, std::optional<PageIdentifier> pageID, ApplyTrackingPrevention applyTrackingPrevention, ShouldRelaxThirdPartyCookieBlocking shouldRelaxThirdPartyCookieBlocking, Vector<Cookie>& rawCookies) const
{
    rawCookies.clear();

    auto soupCookies = lookupCookies(
        *this,
        firstParty,
        sameSiteInfo,
        url,
        ForHTTPHeader::Yes,
        frameID,
        pageID,
        applyTrackingPrevention,
        shouldRelaxThirdPartyCookieBlocking,
        IncludeSecureCookies::Yes);

    if (!soupCookies)
        return false;

    for (GSList* iter = soupCookies->get(); iter; iter = g_slist_next(iter)) {
        auto* cookie = static_cast<SoupCookie*>(iter->data);
        rawCookies.append(Cookie(cookie));
    }

    return true;
}

Vector<Cookie> NetworkStorageSession::domCookiesForHost(const URL& url)
{
    auto host = url.host().utf8();

    CookieList soupCookies(soup_cookie_jar_all_cookies(cookieStorage()));

    Vector<Cookie> cookies;
    for (GSList* iter = soupCookies.get(); iter; iter = g_slist_next(iter)) {
        auto* soupCookie = static_cast<SoupCookie*>(iter->data);
        if (soup_cookie_domain_matches(soupCookie, host.data())) {
            // soup_cookie_jar_all_cookies() always returns a reversed list.
            cookies.insert(0, Cookie(soupCookie));
        }
    }

    return cookies;
}

std::pair<String, bool> NetworkStorageSession::cookiesForDOM(const URL& firstParty, const SameSiteInfo& sameSiteInfo, const URL& url, std::optional<FrameIdentifier> frameID, std::optional<PageIdentifier> pageID, IncludeSecureCookies includeSecureCookies, ApplyTrackingPrevention applyTrackingPrevention, ShouldRelaxThirdPartyCookieBlocking relaxThirdPartyCookieBlocking) const
{
    return lookupCookiesHeaders(
        *this,
        firstParty,
        sameSiteInfo,
        url,
        ForHTTPHeader::No,
        frameID,
        pageID,
        applyTrackingPrevention,
        relaxThirdPartyCookieBlocking,
        includeSecureCookies
    );
}

std::optional<Vector<Cookie>> NetworkStorageSession::cookiesForDOMAsVector(const URL& firstParty, const SameSiteInfo& sameSiteInfo, const URL& url, std::optional<FrameIdentifier> frameID, std::optional<PageIdentifier> pageID, IncludeSecureCookies includeSecureCookies, ApplyTrackingPrevention applyTrackingPrevention, ShouldRelaxThirdPartyCookieBlocking shouldRelaxThirdPartyCookieBlocking, CookieStoreGetOptions&& options) const
{
    auto soupCookies = lookupCookies(
        *this,
        firstParty,
        sameSiteInfo,
        url,
        ForHTTPHeader::No,
        frameID,
        pageID,
        applyTrackingPrevention,
        shouldRelaxThirdPartyCookieBlocking,
        includeSecureCookies);

    if (!soupCookies)
        return std::nullopt;

    Vector<Cookie> cookies;
    for (GSList* iter = soupCookies->get(); iter; iter = g_slist_next(iter)) {
        auto* cookie = static_cast<SoupCookie*>(iter->data);
        if (!options.name.isNull() && options.name != String::fromUTF8(soup_cookie_get_name(cookie)))
            continue;

        cookies.append(Cookie(cookie));
    }

    return cookies;
}

std::pair<String, bool> NetworkStorageSession::cookieRequestHeaderFieldValue(const URL& firstParty, const SameSiteInfo& sameSiteInfo, const URL& url, std::optional<FrameIdentifier> frameID, std::optional<PageIdentifier> pageID, IncludeSecureCookies includeSecureCookies, ApplyTrackingPrevention applyTrackingPrevention, ShouldRelaxThirdPartyCookieBlocking relaxThirdPartyCookieBlocking) const
{
    return lookupCookiesHeaders(
        *this,
        firstParty,
        sameSiteInfo,
        url,
        ForHTTPHeader::Yes,
        frameID,
        pageID,
        applyTrackingPrevention,
        relaxThirdPartyCookieBlocking,
        includeSecureCookies);
}

std::pair<String, bool> NetworkStorageSession::cookieRequestHeaderFieldValue(const CookieRequestHeaderFieldProxy& headerFieldProxy) const
{
    return lookupCookiesHeaders(
        *this,
        headerFieldProxy.firstParty,
        headerFieldProxy.sameSiteInfo,
        headerFieldProxy.url,
        ForHTTPHeader::Yes,
        headerFieldProxy.frameID,
        headerFieldProxy.pageID,
        ApplyTrackingPrevention::Yes,
        ShouldRelaxThirdPartyCookieBlocking::No,
        headerFieldProxy.includeSecureCookies);
}

#if HAVE(COOKIE_CHANGE_LISTENER_API)
bool NetworkStorageSession::startListeningForCookieChangeNotifications(CookieChangeObserver& observer, const URL& url, const URL& firstParty, FrameIdentifier frameID, PageIdentifier pageID, ShouldRelaxThirdPartyCookieBlocking shouldRelaxThirdPartyCookieBlocking)
{
    if (shouldBlockCookies(firstParty, url, frameID, pageID, shouldRelaxThirdPartyCookieBlocking))
        return false;

    auto host = url.host().toString();
    auto& observers = m_cookieChangeObservers.ensure(host, [] {
        return WeakHashSet<CookieChangeObserver> { };
    }).iterator->value;
    observers.add(observer);
    return true;
}

void NetworkStorageSession::stopListeningForCookieChangeNotifications(CookieChangeObserver& observer, const HashSet<String>& hosts)
{
    for (auto& host : hosts) {
        auto it = m_cookieChangeObservers.find(host);
        ASSERT(it != m_cookieChangeObservers.end());

        auto& observers = it->value;
        ASSERT(observers.contains(observer));
        observers.remove(observer);

        if (observers.isEmptyIgnoringNullReferences())
            m_cookieChangeObservers.remove(it);
    }
}
#endif

} // namespace WebCore

#endif // USE(SOUP)
