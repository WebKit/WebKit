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

#include "ChildProcess.h"
#include "WebFrameNetworkingContext.h"
#include "WebKitSoupCookieJarSqlite.h"
#include <WebCore/CookieJarSoup.h>
#include <WebCore/SoupNetworkSession.h>
#include <libsoup/soup.h>
#include <wtf/gobject/GRefPtr.h>
#include <wtf/text/CString.h>

using namespace WebCore;

namespace WebKit {

void WebCookieManager::platformSetHTTPCookieAcceptPolicy(HTTPCookieAcceptPolicy policy)
{
    WebFrameNetworkingContext::setCookieAcceptPolicyForAllContexts(policy);
}

HTTPCookieAcceptPolicy WebCookieManager::platformGetHTTPCookieAcceptPolicy()
{
    SoupCookieJar* cookieJar = WebCore::soupCookieJar();
    SoupCookieJarAcceptPolicy soupPolicy;

    HTTPCookieAcceptPolicy policy;

    soupPolicy = soup_cookie_jar_get_accept_policy(cookieJar);
    switch (soupPolicy) {
    case SOUP_COOKIE_JAR_ACCEPT_ALWAYS:
        policy = HTTPCookieAcceptPolicyAlways;
        break;
    case SOUP_COOKIE_JAR_ACCEPT_NEVER:
        policy = HTTPCookieAcceptPolicyNever;
        break;
    case SOUP_COOKIE_JAR_ACCEPT_NO_THIRD_PARTY:
        policy = HTTPCookieAcceptPolicyOnlyFromMainDocumentDomain;
        break;
    default:
        policy = HTTPCookieAcceptPolicyAlways;
    }
    return policy;
}

void WebCookieManager::setCookiePersistentStorage(const String& storagePath, uint32_t storageType)
{
    GRefPtr<SoupCookieJar> jar;
    switch (storageType) {
    case SoupCookiePersistentStorageText:
        jar = adoptGRef(soup_cookie_jar_text_new(storagePath.utf8().data(), FALSE));
        break;
    case SoupCookiePersistentStorageSQLite:
        jar = adoptGRef(webkitSoupCookieJarSqliteNew(storagePath));
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    soup_cookie_jar_set_accept_policy(jar.get(), soup_cookie_jar_get_accept_policy(WebCore::soupCookieJar()));
    SoupNetworkSession::defaultSession().setCookieJar(jar.get());
    WebCore::setSoupCookieJar(jar.get());
}

} // namespace WebKit
