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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if USE(SOUP)

#include "SoupNetworkSession.h"

#include "AuthenticationChallenge.h"
#include "FileSystem.h"
#include "GUniquePtrSoup.h"
#include "Logging.h"
#include "SoupNetworkProxySettings.h"
#include <glib/gstdio.h>
#include <libsoup/soup.h>
#include <pal/crypto/CryptoDigest.h>
#include <wtf/HashSet.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/Base64.h>
#include <wtf/text/CString.h>

namespace WebCore {

static bool gIgnoreTLSErrors;
static GType gCustomProtocolRequestType;

static CString& initialAcceptLanguages()
{
    static NeverDestroyed<CString> storage;
    return storage.get();
}

static SoupNetworkProxySettings& proxySettings()
{
    static NeverDestroyed<SoupNetworkProxySettings> settings;
    return settings.get();
}

#if !LOG_DISABLED
inline static void soupLogPrinter(SoupLogger*, SoupLoggerLogLevel, char direction, const char* data, gpointer)
{
    LOG(Network, "%c %s", direction, data);
}
#endif

class HostTLSCertificateSet {
public:
    void add(GTlsCertificate* certificate)
    {
        String certificateHash = computeCertificateHash(certificate);
        if (!certificateHash.isEmpty())
            m_certificates.add(certificateHash);
    }

    bool contains(GTlsCertificate* certificate) const
    {
        return m_certificates.contains(computeCertificateHash(certificate));
    }

private:
    static String computeCertificateHash(GTlsCertificate* certificate)
    {
        GRefPtr<GByteArray> certificateData;
        g_object_get(G_OBJECT(certificate), "certificate", &certificateData.outPtr(), nullptr);
        if (!certificateData)
            return String();

        auto digest = PAL::CryptoDigest::create(PAL::CryptoDigest::Algorithm::SHA_256);
        digest->addBytes(certificateData->data, certificateData->len);

        auto hash = digest->computeHash();
        return base64Encode(reinterpret_cast<const char*>(hash.data()), hash.size());
    }

    HashSet<String> m_certificates;
};

static HashMap<String, HostTLSCertificateSet, ASCIICaseInsensitiveHash>& clientCertificates()
{
    static NeverDestroyed<HashMap<String, HostTLSCertificateSet, ASCIICaseInsensitiveHash>> certificates;
    return certificates;
}

SoupNetworkSession::SoupNetworkSession(PAL::SessionID sessionID, SoupCookieJar* cookieJar)
    : m_soupSession(adoptGRef(soup_session_new()))
{
    // Values taken from http://www.browserscope.org/ following
    // the rule "Do What Every Other Modern Browser Is Doing". They seem
    // to significantly improve page loading time compared to soup's
    // default values.
    static const int maxConnections = 17;
    static const int maxConnectionsPerHost = 6;

    GRefPtr<SoupCookieJar> jar = cookieJar;
    if (!jar) {
        jar = adoptGRef(soup_cookie_jar_new());
        soup_cookie_jar_set_accept_policy(jar.get(), SOUP_COOKIE_JAR_ACCEPT_NO_THIRD_PARTY);
    }

    g_object_set(m_soupSession.get(),
        SOUP_SESSION_MAX_CONNS, maxConnections,
        SOUP_SESSION_MAX_CONNS_PER_HOST, maxConnectionsPerHost,
        SOUP_SESSION_TIMEOUT, 0,
        SOUP_SESSION_IDLE_TIMEOUT, 0,
        SOUP_SESSION_ADD_FEATURE_BY_TYPE, SOUP_TYPE_CONTENT_SNIFFER,
        SOUP_SESSION_ADD_FEATURE, jar.get(),
        nullptr);

    setupCustomProtocols();

    if (!initialAcceptLanguages().isNull())
        setAcceptLanguages(initialAcceptLanguages());

#if SOUP_CHECK_VERSION(2, 53, 92)
    if (soup_auth_negotiate_supported() && !sessionID.isEphemeral()) {
        g_object_set(m_soupSession.get(),
            SOUP_SESSION_ADD_FEATURE_BY_TYPE, SOUP_TYPE_AUTH_NEGOTIATE,
            nullptr);
    }
#else
    UNUSED_PARAM(sessionID);
#endif

    if (proxySettings().mode != SoupNetworkProxySettings::Mode::Default)
        setupProxy();
    setupLogger();
}

SoupNetworkSession::~SoupNetworkSession() = default;

void SoupNetworkSession::setupLogger()
{
#if !LOG_DISABLED
    if (LogNetwork.state != WTFLogChannelOn || soup_session_get_feature(m_soupSession.get(), SOUP_TYPE_LOGGER))
        return;

    GRefPtr<SoupLogger> logger = adoptGRef(soup_logger_new(SOUP_LOGGER_LOG_BODY, -1));
    soup_session_add_feature(m_soupSession.get(), SOUP_SESSION_FEATURE(logger.get()));
    soup_logger_set_printer(logger.get(), soupLogPrinter, nullptr, nullptr);
#endif
}

void SoupNetworkSession::setCookieJar(SoupCookieJar* jar)
{
    if (SoupCookieJar* currentJar = cookieJar())
        soup_session_remove_feature(m_soupSession.get(), SOUP_SESSION_FEATURE(currentJar));
    soup_session_add_feature(m_soupSession.get(), SOUP_SESSION_FEATURE(jar));
}

SoupCookieJar* SoupNetworkSession::cookieJar() const
{
    return SOUP_COOKIE_JAR(soup_session_get_feature(m_soupSession.get(), SOUP_TYPE_COOKIE_JAR));
}

static inline bool stringIsNumeric(const char* str)
{
    while (*str) {
        if (!g_ascii_isdigit(*str))
            return false;
        str++;
    }
    return true;
}

// Old versions of WebKit created this cache.
void SoupNetworkSession::clearOldSoupCache(const String& cacheDirectory)
{
    CString cachePath = FileSystem::fileSystemRepresentation(cacheDirectory);
    GUniquePtr<char> cacheFile(g_build_filename(cachePath.data(), "soup.cache2", nullptr));
    if (!g_file_test(cacheFile.get(), G_FILE_TEST_IS_REGULAR))
        return;

    GUniquePtr<GDir> dir(g_dir_open(cachePath.data(), 0, nullptr));
    if (!dir)
        return;

    while (const char* name = g_dir_read_name(dir.get())) {
        if (!g_str_has_prefix(name, "soup.cache") && !stringIsNumeric(name))
            continue;

        GUniquePtr<gchar> filename(g_build_filename(cachePath.data(), name, nullptr));
        if (g_file_test(filename.get(), G_FILE_TEST_IS_REGULAR))
            g_unlink(filename.get());
    }
}

void SoupNetworkSession::setupProxy()
{
    GRefPtr<GProxyResolver> resolver;
    switch (proxySettings().mode) {
    case SoupNetworkProxySettings::Mode::Default: {
        GRefPtr<GProxyResolver> currentResolver;
        g_object_get(m_soupSession.get(), SOUP_SESSION_PROXY_RESOLVER, &currentResolver.outPtr(), nullptr);
        GProxyResolver* defaultResolver = g_proxy_resolver_get_default();
        if (currentResolver.get() == defaultResolver)
            return;
        resolver = defaultResolver;
        break;
    }
    case SoupNetworkProxySettings::Mode::NoProxy:
        // Do nothing in this case, resolver is nullptr so that when set it will disable proxies.
        break;
    case SoupNetworkProxySettings::Mode::Custom:
        resolver = adoptGRef(g_simple_proxy_resolver_new(nullptr, nullptr));
        if (!proxySettings().defaultProxyURL.isNull())
            g_simple_proxy_resolver_set_default_proxy(G_SIMPLE_PROXY_RESOLVER(resolver.get()), proxySettings().defaultProxyURL.data());
        if (proxySettings().ignoreHosts)
            g_simple_proxy_resolver_set_ignore_hosts(G_SIMPLE_PROXY_RESOLVER(resolver.get()), proxySettings().ignoreHosts.get());
        for (const auto& iter : proxySettings().proxyMap)
            g_simple_proxy_resolver_set_uri_proxy(G_SIMPLE_PROXY_RESOLVER(resolver.get()), iter.key.data(), iter.value.data());
        break;
    }

    g_object_set(m_soupSession.get(), SOUP_SESSION_PROXY_RESOLVER, resolver.get(), nullptr);
    soup_session_abort(m_soupSession.get());
}

void SoupNetworkSession::setProxySettings(const SoupNetworkProxySettings& settings)
{
    proxySettings() = settings;
}

void SoupNetworkSession::setInitialAcceptLanguages(const CString& languages)
{
    initialAcceptLanguages() = languages;
}

void SoupNetworkSession::setAcceptLanguages(const CString& languages)
{
    g_object_set(m_soupSession.get(), "accept-language", languages.data(), nullptr);
}

void SoupNetworkSession::setCustomProtocolRequestType(GType requestType)
{
    ASSERT(g_type_is_a(requestType, SOUP_TYPE_REQUEST));
    gCustomProtocolRequestType = requestType;
}

void SoupNetworkSession::setupCustomProtocols()
{
    if (!g_type_is_a(gCustomProtocolRequestType, SOUP_TYPE_REQUEST))
        return;

    auto* requestClass = static_cast<SoupRequestClass*>(g_type_class_peek(gCustomProtocolRequestType));
    if (!requestClass || !requestClass->schemes)
        return;

    soup_session_add_feature_by_type(m_soupSession.get(), gCustomProtocolRequestType);
}

void SoupNetworkSession::setShouldIgnoreTLSErrors(bool ignoreTLSErrors)
{
    gIgnoreTLSErrors = ignoreTLSErrors;
}

std::optional<ResourceError> SoupNetworkSession::checkTLSErrors(const URL& requestURL, GTlsCertificate* certificate, GTlsCertificateFlags tlsErrors)
{
    if (gIgnoreTLSErrors)
        return std::nullopt;

    if (!tlsErrors)
        return std::nullopt;

    auto it = clientCertificates().find(requestURL.host().toString());
    if (it != clientCertificates().end() && it->value.contains(certificate))
        return std::nullopt;

    return ResourceError::tlsError(requestURL, tlsErrors, certificate);
}

void SoupNetworkSession::allowSpecificHTTPSCertificateForHost(const CertificateInfo& certificateInfo, const String& host)
{
    clientCertificates().add(host, HostTLSCertificateSet()).iterator->value.add(certificateInfo.certificate());
}

} // namespace WebCore

#endif
