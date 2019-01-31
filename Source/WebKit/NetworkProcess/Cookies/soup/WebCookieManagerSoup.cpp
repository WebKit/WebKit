/*
 * Copyright (C) 2011 Samsung Electronics.
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
#include "WebCookieManager.h"

#include "NetworkProcess.h"
#include <WebCore/NetworkStorageSession.h>
#include <WebCore/SoupNetworkSession.h>
#include <libsoup/soup.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/text/CString.h>

namespace WebKit {
using namespace WebCore;

void WebCookieManager::platformSetHTTPCookieAcceptPolicy(HTTPCookieAcceptPolicy policy)
{
    SoupCookieJarAcceptPolicy soupPolicy = SOUP_COOKIE_JAR_ACCEPT_NO_THIRD_PARTY;
    switch (policy) {
    case HTTPCookieAcceptPolicyAlways:
        soupPolicy = SOUP_COOKIE_JAR_ACCEPT_ALWAYS;
        break;
    case HTTPCookieAcceptPolicyNever:
        soupPolicy = SOUP_COOKIE_JAR_ACCEPT_NEVER;
        break;
    case HTTPCookieAcceptPolicyOnlyFromMainDocumentDomain:
        soupPolicy = SOUP_COOKIE_JAR_ACCEPT_NO_THIRD_PARTY;
        break;
    }

    m_process.forEachNetworkStorageSession([soupPolicy] (const auto& session) {
        soup_cookie_jar_set_accept_policy(session.cookieStorage(), soupPolicy);
    });
}

HTTPCookieAcceptPolicy WebCookieManager::platformGetHTTPCookieAcceptPolicy()
{
    switch (soup_cookie_jar_get_accept_policy(m_process.defaultStorageSession().cookieStorage())) {
    case SOUP_COOKIE_JAR_ACCEPT_ALWAYS:
        return HTTPCookieAcceptPolicyAlways;
    case SOUP_COOKIE_JAR_ACCEPT_NEVER:
        return HTTPCookieAcceptPolicyNever;
    case SOUP_COOKIE_JAR_ACCEPT_NO_THIRD_PARTY:
        return HTTPCookieAcceptPolicyOnlyFromMainDocumentDomain;
    }

    ASSERT_NOT_REACHED();
    return HTTPCookieAcceptPolicyOnlyFromMainDocumentDomain;
}

void WebCookieManager::setCookiePersistentStorage(PAL::SessionID sessionID, const String& storagePath, uint32_t storageType)
{
    GRefPtr<SoupCookieJar> jar;
    switch (storageType) {
    case SoupCookiePersistentStorageText:
        jar = adoptGRef(soup_cookie_jar_text_new(storagePath.utf8().data(), FALSE));
        break;
    case SoupCookiePersistentStorageSQLite:
        jar = adoptGRef(soup_cookie_jar_db_new(storagePath.utf8().data(), FALSE));
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    if (auto* storageSession = m_process.storageSession(sessionID)) {
        soup_cookie_jar_set_accept_policy(jar.get(), soup_cookie_jar_get_accept_policy(storageSession->cookieStorage()));
        storageSession->setCookieStorage(jar.get());
    }
}

} // namespace WebKit
