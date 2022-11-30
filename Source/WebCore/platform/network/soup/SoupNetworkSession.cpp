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
#include "GUniquePtrSoup.h"
#include "Logging.h"
#include "SoupVersioning.h"
#include "WebKitAutoconfigProxyResolver.h"
#include <glib/gstdio.h>
#include <libsoup/soup.h>
#include <pal/crypto/CryptoDigest.h>
#include <wtf/FileSystem.h>
#include <wtf/HashSet.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/Base64.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringHash.h>

namespace WebCore {

static CString& initialAcceptLanguages()
{
    static NeverDestroyed<CString> storage;
    return storage.get();
}

#if !LOG_DISABLED || !RELEASE_LOG_DISABLED
inline static void soupLogPrinter(SoupLogger*, SoupLoggerLogLevel, char direction, const char* data, gpointer)
{
#if !LOG_DISABLED
    LOG(Network, "%c %s", direction, data);
#elif !RELEASE_LOG_DISABLED
    RELEASE_LOG(Network, "%c %s", direction, data);
#endif
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
        return base64EncodeToString(hash);
    }

    HashSet<String> m_certificates;
};

using AllowedCertificatesMap = HashMap<String, HostTLSCertificateSet, ASCIICaseInsensitiveHash>;

static AllowedCertificatesMap& allowedCertificates()
{
    static NeverDestroyed<AllowedCertificatesMap> certificates;
    return certificates;
}

SoupNetworkSession::SoupNetworkSession(PAL::SessionID sessionID)
    : m_sessionID(sessionID)
{
    // Values taken from http://www.browserscope.org/ following
    // the rule "Do What Every Other Modern Browser Is Doing". They seem
    // to significantly improve page loading time compared to soup's
    // default values.
    static const int maxConnections = 17;
    static const int maxConnectionsPerHost = 6;

    m_soupSession = adoptGRef(soup_session_new_with_options(
        "max-conns", maxConnections,
        "max-conns-per-host", maxConnectionsPerHost,
        "timeout", 0,
        nullptr));

    soup_session_add_feature_by_type(m_soupSession.get(), SOUP_TYPE_CONTENT_SNIFFER);
    soup_session_add_feature_by_type(m_soupSession.get(), SOUP_TYPE_AUTH_NTLM);
#if SOUP_CHECK_VERSION(2, 67, 1)
    soup_session_add_feature_by_type(m_soupSession.get(), SOUP_TYPE_HSTS_ENFORCER);
#endif
#if SOUP_CHECK_VERSION(2, 67, 90)
    soup_session_add_feature_by_type(m_soupSession.get(), SOUP_TYPE_WEBSOCKET_EXTENSION_MANAGER);
#endif

    if (!initialAcceptLanguages().isNull())
        setAcceptLanguages(initialAcceptLanguages());

    if (soup_auth_negotiate_supported() && !m_sessionID.isEphemeral())
        soup_session_add_feature_by_type(m_soupSession.get(), SOUP_TYPE_AUTH_NEGOTIATE);

#if ENABLE(DEVELOPER_MODE)
    // Optionally load a custom path with the TLS CAFile. This is used on the GTK/WPE bundles. See script generate-bundle
    // Don't enable this outside DEVELOPER_MODE because using a non-default TLS database has surprising undesired effects
    // on how certificate verification is performed. See https://webkit.org/b/237107#c14 and git commit 82999879b on glib
    const char* customTLSCAFile = g_getenv("WEBKIT_TLS_CAFILE_PEM");
    if (customTLSCAFile) {
        GUniqueOutPtr<GError> error;
        GRefPtr<GTlsDatabase> customTLSDB = adoptGRef(g_tls_file_database_new(customTLSCAFile, &error.outPtr()));
        if (error)
            WTFLogAlways("Failed to load TLS database \"%s\": %s", customTLSCAFile, error->message);
        else
            soup_session_set_tls_database(m_soupSession.get(), customTLSDB.get());
    }
#endif // ENABLE(DEVELOPER_MODE)

    setupLogger();
}

SoupNetworkSession::~SoupNetworkSession() = default;

void SoupNetworkSession::setupLogger()
{
#if !LOG_DISABLED || !RELEASE_LOG_DISABLED
    if (LogNetwork.state != WTFLogChannelState::On || soup_session_get_feature(m_soupSession.get(), SOUP_TYPE_LOGGER))
        return;

#if USE(SOUP2)
    GRefPtr<SoupLogger> logger = adoptGRef(soup_logger_new(SOUP_LOGGER_LOG_BODY, -1));
#else
    GRefPtr<SoupLogger> logger = adoptGRef(soup_logger_new(SOUP_LOGGER_LOG_BODY));
#endif
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

void SoupNetworkSession::setHSTSPersistentStorage(const String& directory)
{
    if (m_sessionID.isEphemeral())
        return;

#if SOUP_CHECK_VERSION(2, 67, 1)
    if (!FileSystem::makeAllDirectories(directory)) {
        RELEASE_LOG_ERROR(Network, "Unable to create the HSTS storage directory \"%s\". Using a memory enforcer instead.", directory.utf8().data());
        return;
    }

    CString storagePath = FileSystem::fileSystemRepresentation(directory);
    GUniquePtr<char> dbFilename(g_build_filename(storagePath.data(), "hsts-storage.sqlite", nullptr));
    GRefPtr<SoupHSTSEnforcer> enforcer = adoptGRef(soup_hsts_enforcer_db_new(dbFilename.get()));
    soup_session_remove_feature_by_type(m_soupSession.get(), SOUP_TYPE_HSTS_ENFORCER);
    soup_session_add_feature(m_soupSession.get(), SOUP_SESSION_FEATURE(enforcer.get()));
#else
    UNUSED_PARAM(directory);
#endif
}

void SoupNetworkSession::getHostNamesWithHSTSCache(HashSet<String>& hostNames)
{
#if SOUP_CHECK_VERSION(2, 67, 91)
    auto* enforcer = SOUP_HSTS_ENFORCER(soup_session_get_feature(m_soupSession.get(), SOUP_TYPE_HSTS_ENFORCER));
    ASSERT(enforcer);

    GUniquePtr<GList> domains(soup_hsts_enforcer_get_domains(enforcer, FALSE));
    for (GList* iter = domains.get(); iter; iter = iter->next) {
        GUniquePtr<gchar> domain(static_cast<gchar*>(iter->data));
        hostNames.add(String::fromUTF8(domain.get()));
    }
#else
    UNUSED_PARAM(hostNames);
#endif
}

void SoupNetworkSession::deleteHSTSCacheForHostNames(const Vector<String>& hostNames)
{
#if SOUP_CHECK_VERSION(2, 67, 1)
    auto* enforcer = SOUP_HSTS_ENFORCER(soup_session_get_feature(m_soupSession.get(), SOUP_TYPE_HSTS_ENFORCER));
    ASSERT(enforcer);

    for (const auto& hostName : hostNames) {
        GUniquePtr<SoupHSTSPolicy> policy(soup_hsts_policy_new(hostName.utf8().data(), SOUP_HSTS_POLICY_MAX_AGE_PAST, FALSE));
        soup_hsts_enforcer_set_policy(enforcer, policy.get());
    }
#else
    UNUSED_PARAM(hostNames);
#endif
}

void SoupNetworkSession::clearHSTSCache(WallTime modifiedSince)
{
#if SOUP_CHECK_VERSION(2, 67, 91)
    auto* enforcer = SOUP_HSTS_ENFORCER(soup_session_get_feature(m_soupSession.get(), SOUP_TYPE_HSTS_ENFORCER));
    ASSERT(enforcer);

    GUniquePtr<GList> policies(soup_hsts_enforcer_get_policies(enforcer, FALSE));
    for (GList* iter = policies.get(); iter != nullptr; iter = iter->next) {
        GUniquePtr<SoupHSTSPolicy> policy(static_cast<SoupHSTSPolicy*>(iter->data));
#if USE(SOUP2)
        auto modified = soup_date_to_time_t(policy.get()->expires) - policy.get()->max_age;
#else
        auto modified = g_date_time_to_unix(soup_hsts_policy_get_expires(policy.get())) - soup_hsts_policy_get_max_age(policy.get());
#endif
        if (modified >= modifiedSince.secondsSinceEpoch().seconds()) {
            GUniquePtr<SoupHSTSPolicy> newPolicy(soup_hsts_policy_new(soup_hsts_policy_get_domain(policy.get()), SOUP_HSTS_POLICY_MAX_AGE_PAST, FALSE));
            soup_hsts_enforcer_set_policy(enforcer, newPolicy.get());
        }
    }
#else
    UNUSED_PARAM(modifiedSince);
#endif
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

void SoupNetworkSession::setProxySettings(const SoupNetworkProxySettings& settings)
{
    m_proxySettings = settings;

    GRefPtr<GProxyResolver> resolver;
    switch (m_proxySettings.mode) {
    case SoupNetworkProxySettings::Mode::Default: {
        GProxyResolver* defaultResolver = g_proxy_resolver_get_default();
        if (defaultResolver == soup_session_get_proxy_resolver(m_soupSession.get()))
            return;
        resolver = defaultResolver;
        break;
    }
    case SoupNetworkProxySettings::Mode::NoProxy:
        // Do nothing in this case, resolver is nullptr so that when set it will disable proxies.
        break;
    case SoupNetworkProxySettings::Mode::Custom:
        resolver = adoptGRef(g_simple_proxy_resolver_new(nullptr, nullptr));
        if (!m_proxySettings.defaultProxyURL.isNull())
            g_simple_proxy_resolver_set_default_proxy(G_SIMPLE_PROXY_RESOLVER(resolver.get()), m_proxySettings.defaultProxyURL.data());
        if (m_proxySettings.ignoreHosts)
            g_simple_proxy_resolver_set_ignore_hosts(G_SIMPLE_PROXY_RESOLVER(resolver.get()), m_proxySettings.ignoreHosts.get());
        for (const auto& iter : m_proxySettings.proxyMap)
            g_simple_proxy_resolver_set_uri_proxy(G_SIMPLE_PROXY_RESOLVER(resolver.get()), iter.key.data(), iter.value.data());
        break;
    case SoupNetworkProxySettings::Mode::Auto:
        resolver = webkitAutoconfigProxyResolverNew(m_proxySettings.defaultProxyURL);
        break;
    }

    soup_session_set_proxy_resolver(m_soupSession.get(), resolver.get());
    soup_session_abort(m_soupSession.get());
}

void SoupNetworkSession::setInitialAcceptLanguages(const CString& languages)
{
    initialAcceptLanguages() = languages;
}

void SoupNetworkSession::setAcceptLanguages(const CString& languages)
{
    soup_session_set_accept_language(m_soupSession.get(), languages.data());
}

void SoupNetworkSession::setIgnoreTLSErrors(bool ignoreTLSErrors)
{
    m_ignoreTLSErrors = ignoreTLSErrors;
}

std::optional<ResourceError> SoupNetworkSession::checkTLSErrors(const URL& requestURL, GTlsCertificate* certificate, GTlsCertificateFlags tlsErrors)
{
    if (m_ignoreTLSErrors || !tlsErrors)
        return std::nullopt;

    auto it = allowedCertificates().find<ASCIICaseInsensitiveStringViewHashTranslator>(requestURL.host());
    if (it != allowedCertificates().end() && it->value.contains(certificate))
        return std::nullopt;

    return ResourceError::tlsError(requestURL, tlsErrors, certificate);
}

void SoupNetworkSession::allowSpecificHTTPSCertificateForHost(const CertificateInfo& certificateInfo, const String& host)
{
    allowedCertificates().add(host, HostTLSCertificateSet()).iterator->value.add(certificateInfo.certificate().get());
}

} // namespace WebCore

#endif
