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

#ifndef SoupNetworkSession_h
#define SoupNetworkSession_h

#include <gio/gio.h>
#include <glib-object.h>
#include <pal/SessionID.h>
#include <wtf/Function.h>
#include <wtf/Noncopyable.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/text/WTFString.h>

typedef struct _SoupCache SoupCache;
typedef struct _SoupCookieJar SoupCookieJar;
typedef struct _SoupMessage SoupMessage;
typedef struct _SoupRequest SoupRequest;
typedef struct _SoupSession SoupSession;

namespace WebCore {

class CertificateInfo;
class ResourceError;
struct SoupNetworkProxySettings;

class SoupNetworkSession {
    WTF_MAKE_NONCOPYABLE(SoupNetworkSession); WTF_MAKE_FAST_ALLOCATED;
public:
    explicit SoupNetworkSession(PAL::SessionID);
    ~SoupNetworkSession();

    SoupSession* soupSession() const { return m_soupSession.get(); }

    void setCookieJar(SoupCookieJar*);
    SoupCookieJar* cookieJar() const;

    static void setHSTSPersistentStorage(const CString& hstsStorageDirectory);
    void setupHSTSEnforcer();

    static void clearOldSoupCache(const String& cacheDirectory);

    static void setProxySettings(const SoupNetworkProxySettings&);
    void setupProxy();

    static void setInitialAcceptLanguages(const CString&);
    void setAcceptLanguages(const CString&);

    static void setShouldIgnoreTLSErrors(bool);
    static Optional<ResourceError> checkTLSErrors(const URL&, GTlsCertificate*, GTlsCertificateFlags);
    static void allowSpecificHTTPSCertificateForHost(const CertificateInfo&, const String& host);

    static void setCustomProtocolRequestType(GType);
    void setupCustomProtocols();

    void getHostNamesWithHSTSCache(HashSet<String>&);
    void deleteHSTSCacheForHostNames(const Vector<String>&);
    void clearHSTSCache(WallTime);

private:
    void setupLogger();

    GRefPtr<SoupSession> m_soupSession;
    PAL::SessionID m_sessionID;
};

} // namespace WebCore

#endif
