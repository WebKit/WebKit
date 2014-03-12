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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "SoupNetworkSession.h"

#include "AuthenticationChallenge.h"
#include "CookieJarSoup.h"
#include "GUniquePtrSoup.h"
#include "Logging.h"
#include "ResourceHandle.h"
#include <libsoup/soup.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>

#if PLATFORM(EFL)
#include "ProxyResolverSoup.h"
#endif

namespace WebCore {

#if !LOG_DISABLED
inline static void soupLogPrinter(SoupLogger*, SoupLoggerLogLevel, char direction, const char* data, gpointer)
{
    LOG(Network, "%c %s", direction, data);
}
#endif

SoupNetworkSession& SoupNetworkSession::defaultSession()
{
    static SoupNetworkSession networkSession(soupCookieJar());
    return networkSession;
}

std::unique_ptr<SoupNetworkSession> SoupNetworkSession::createPrivateBrowsingSession()
{
    return std::unique_ptr<SoupNetworkSession>(new SoupNetworkSession(soupCookieJar()));
}

std::unique_ptr<SoupNetworkSession> SoupNetworkSession::createTestingSession()
{
    GRefPtr<SoupCookieJar> cookieJar = adoptGRef(createPrivateBrowsingCookieJar());
    return std::unique_ptr<SoupNetworkSession>(new SoupNetworkSession(cookieJar.get()));
}

std::unique_ptr<SoupNetworkSession> SoupNetworkSession::createForSoupSession(SoupSession* soupSession)
{
    return std::unique_ptr<SoupNetworkSession>(new SoupNetworkSession(soupSession));
}

static void authenticateCallback(SoupSession* session, SoupMessage* soupMessage, SoupAuth* soupAuth, gboolean retrying)
{
    RefPtr<ResourceHandle> handle = static_cast<ResourceHandle*>(g_object_get_data(G_OBJECT(soupMessage), "handle"));
    if (!handle)
        return;
    handle->didReceiveAuthenticationChallenge(AuthenticationChallenge(session, soupMessage, soupAuth, retrying, handle.get()));
}

#if ENABLE(WEB_TIMING)
static void requestStartedCallback(SoupSession*, SoupMessage* soupMessage, SoupSocket*, gpointer)
{
    RefPtr<ResourceHandle> handle = static_cast<ResourceHandle*>(g_object_get_data(G_OBJECT(soupMessage), "handle"));
    if (!handle)
        return;
    handle->didStartRequest();
}
#endif

SoupNetworkSession::SoupNetworkSession(SoupCookieJar* cookieJar)
    : m_soupSession(adoptGRef(soup_session_async_new()))
{
    // Values taken from http://www.browserscope.org/ following
    // the rule "Do What Every Other Modern Browser Is Doing". They seem
    // to significantly improve page loading time compared to soup's
    // default values.
    static const int maxConnections = 35;
    static const int maxConnectionsPerHost = 6;

    g_object_set(m_soupSession.get(),
        SOUP_SESSION_MAX_CONNS, maxConnections,
        SOUP_SESSION_MAX_CONNS_PER_HOST, maxConnectionsPerHost,
        SOUP_SESSION_ADD_FEATURE_BY_TYPE, SOUP_TYPE_CONTENT_DECODER,
        SOUP_SESSION_ADD_FEATURE_BY_TYPE, SOUP_TYPE_CONTENT_SNIFFER,
        SOUP_SESSION_ADD_FEATURE_BY_TYPE, SOUP_TYPE_PROXY_RESOLVER_DEFAULT,
        SOUP_SESSION_ADD_FEATURE, cookieJar,
        SOUP_SESSION_USE_THREAD_CONTEXT, TRUE,
        nullptr);

    setupLogger();

    g_signal_connect(m_soupSession.get(), "authenticate", G_CALLBACK(authenticateCallback), nullptr);
#if ENABLE(WEB_TIMING)
    g_signal_connect(m_soupSession.get(), "request-started", G_CALLBACK(requestStartedCallback), nullptr);
#endif
}

SoupNetworkSession::SoupNetworkSession(SoupSession* soupSession)
    : m_soupSession(soupSession)
{
    setupLogger();
}

SoupNetworkSession::~SoupNetworkSession()
{
}

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

void SoupNetworkSession::setCache(SoupCache* cache)
{
    ASSERT(!soup_session_get_feature(m_soupSession.get(), SOUP_TYPE_CACHE));
    soup_session_add_feature(m_soupSession.get(), SOUP_SESSION_FEATURE(cache));
}

SoupCache* SoupNetworkSession::cache() const
{
    SoupSessionFeature* soupCache = soup_session_get_feature(m_soupSession.get(), SOUP_TYPE_CACHE);
    return soupCache ? SOUP_CACHE(soupCache) : nullptr;
}

void SoupNetworkSession::setSSLPolicy(SSLPolicy flags)
{
    g_object_set(m_soupSession.get(),
        SOUP_SESSION_SSL_USE_SYSTEM_CA_FILE, flags & SSLUseSystemCAFile ? TRUE : FALSE,
        SOUP_SESSION_SSL_STRICT, flags & SSLStrict ? TRUE : FALSE,
        nullptr);
}

SoupNetworkSession::SSLPolicy SoupNetworkSession::sslPolicy() const
{
    gboolean useSystemCAFile, strict;
    g_object_get(m_soupSession.get(),
        SOUP_SESSION_SSL_USE_SYSTEM_CA_FILE, &useSystemCAFile,
        SOUP_SESSION_SSL_STRICT, &strict,
        nullptr);

    SSLPolicy flags = 0;
    if (useSystemCAFile)
        flags |= SSLUseSystemCAFile;
    if (strict)
        flags |= SSLStrict;
    return flags;
}

void SoupNetworkSession::setHTTPProxy(const char* httpProxy, const char* httpProxyExceptions)
{
#if PLATFORM(EFL)
    // Only for EFL because GTK port uses the default resolver, which uses GIO's proxy resolver.
    if (!httpProxy) {
        soup_session_remove_feature_by_type(m_soupSession.get(), SOUP_TYPE_PROXY_URI_RESOLVER);
        return;
    }

    GRefPtr<SoupProxyURIResolver> resolver = adoptGRef(soupProxyResolverWkNew(httpProxy, httpProxyExceptions));
    soup_session_add_feature(m_soupSession.get(), SOUP_SESSION_FEATURE(resolver.get()));
#else
    UNUSED_PARAM(httpProxy);
    UNUSED_PARAM(httpProxyExceptions);
#endif
}

char* SoupNetworkSession::httpProxy() const
{
#if PLATFORM(EFL)
    SoupSessionFeature* soupResolver = soup_session_get_feature(m_soupSession.get(), SOUP_TYPE_PROXY_URI_RESOLVER);
    if (!soupResolver)
        return nullptr;

    GUniqueOutPtr<SoupURI> uri;
    g_object_get(soupResolver, SOUP_PROXY_RESOLVER_WK_PROXY_URI, &uri.outPtr(), nullptr);

    return uri ? soup_uri_to_string(uri.get(), FALSE) : nullptr;
#else
    return nullptr;
#endif
}

void SoupNetworkSession::setupHTTPProxyFromEnvironment()
{
#if PLATFORM(EFL)
    const char* httpProxy = getenv("http_proxy");
    if (!httpProxy)
        return;

    setHTTPProxy(httpProxy, getenv("no_proxy"));
#endif
}

static CString buildAcceptLanguages(const Vector<String>& languages)
{
    size_t languagesCount = languages.size();

    // Ignore "C" locale.
    size_t cLocalePosition = languages.find("c");
    if (cLocalePosition != notFound)
        languagesCount--;

    // Fallback to "en" if the list is empty.
    if (!languagesCount)
        return "en";

    // Calculate deltas for the quality values.
    int delta;
    if (languagesCount < 10)
        delta = 10;
    else if (languagesCount < 20)
        delta = 5;
    else
        delta = 1;

    // Set quality values for each language.
    StringBuilder builder;
    for (size_t i = 0; i < languages.size(); ++i) {
        if (i == cLocalePosition)
            continue;

        if (i)
            builder.appendLiteral(", ");

        builder.append(languages[i]);

        int quality = 100 - i * delta;
        if (quality > 0 && quality < 100) {
            char buffer[8];
            g_ascii_formatd(buffer, 8, "%.2f", quality / 100.0);
            builder.append(String::format(";q=%s", buffer));
        }
    }

    return builder.toString().utf8();
}

void SoupNetworkSession::setAcceptLanguages(const Vector<String>& languages)
{
    g_object_set(m_soupSession.get(), "accept-language", buildAcceptLanguages(languages).data(), nullptr);
}

} // namespace WebCore

