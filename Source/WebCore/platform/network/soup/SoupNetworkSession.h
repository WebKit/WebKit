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

#ifndef SoupNetworkSession_h
#define SoupNetworkSession_h

#include <wtf/NeverDestroyed.h>
#include <wtf/Noncopyable.h>
#include <wtf/Vector.h>
#include <wtf/gobject/GRefPtr.h>
#include <wtf/text/WTFString.h>

typedef struct _SoupCache SoupCache;
typedef struct _SoupCookieJar SoupCookieJar;
typedef struct _SoupSession SoupSession;

namespace WebCore {

class SoupNetworkSession {
    WTF_MAKE_NONCOPYABLE(SoupNetworkSession); WTF_MAKE_FAST_ALLOCATED;
    friend class NeverDestroyed<SoupNetworkSession>;
public:
    ~SoupNetworkSession();

    static SoupNetworkSession& defaultSession();
    static std::unique_ptr<SoupNetworkSession> createPrivateBrowsingSession();
    static std::unique_ptr<SoupNetworkSession> createTestingSession();
    static std::unique_ptr<SoupNetworkSession> createForSoupSession(SoupSession*);

    enum SSLPolicyFlags {
        SSLStrict = 1 << 0,
        SSLUseSystemCAFile = 1 << 1
    };
    typedef unsigned SSLPolicy;

    SoupSession* soupSession() const { return m_soupSession.get(); }

    void setCookieJar(SoupCookieJar*);
    SoupCookieJar* cookieJar() const;

    void setCache(SoupCache*);
    SoupCache* cache() const;

    void setSSLPolicy(SSLPolicy);
    SSLPolicy sslPolicy() const;

    void setHTTPProxy(const char* httpProxy, const char* httpProxyExceptions);
    char* httpProxy() const;
    void setupHTTPProxyFromEnvironment();

    void setAcceptLanguages(const Vector<String>&);

private:
    SoupNetworkSession(SoupCookieJar*);
    SoupNetworkSession(SoupSession*);

    void setupLogger();

    GRefPtr<SoupSession> m_soupSession;
};

} // namespace WebCore

#endif
