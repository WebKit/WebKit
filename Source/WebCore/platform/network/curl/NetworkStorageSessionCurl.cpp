/*
 * Copyright (C) 2017 Sony Interactive Entertainment Inc.
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
#include "NetworkStorageSession.h"

#if USE(CURL)

#include "Cookie.h"
#include "CookieJarCurlDatabase.h"
#include "CookieJarDB.h"
#include "CurlContext.h"
#include "FileSystem.h"
#include "NetworkingContext.h"
#include "ResourceHandle.h"

#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>

namespace WebCore {

static String defaultCookieJarPath()
{
    static const char* defaultFileName = "cookie.jar.db";
    char* cookieJarPath = getenv("CURL_COOKIE_JAR_PATH");
    if (cookieJarPath)
        return cookieJarPath;

    return FileSystem::pathByAppendingComponent(FileSystem::localUserSpecificStorageDirectory(), defaultFileName);
}

NetworkStorageSession::NetworkStorageSession(PAL::SessionID sessionID, NetworkingContext* context)
    : m_sessionID(sessionID)
    , m_context(context)
    , m_cookieStorage(makeUniqueRef<CookieJarCurlDatabase>())
    , m_cookieDatabase(makeUniqueRef<CookieJarDB>(defaultCookieJarPath()))
{
}

NetworkStorageSession::~NetworkStorageSession()
{
}

NetworkingContext* NetworkStorageSession::context() const
{
    return m_context.get();
}

void NetworkStorageSession::setCookieDatabase(UniqueRef<CookieJarDB>&& cookieDatabase) const
{
    m_cookieDatabase = WTFMove(cookieDatabase);
}

CookieJarDB& NetworkStorageSession::cookieDatabase() const
{
    m_cookieDatabase->open();
    return m_cookieDatabase;
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

void NetworkStorageSession::ensureSession(PAL::SessionID, const String&)
{
    // FIXME: Implement for WebKit to use.
}

void NetworkStorageSession::switchToNewTestingSession()
{
    // FIXME: Implement for WebKit to use.
}

void NetworkStorageSession::setCookies(const Vector<Cookie>&, const URL&, const URL&)
{
    // FIXME: Implement for WebKit to use.
}

void NetworkStorageSession::setCookie(const Cookie&)
{
    // FIXME: Implement for WebKit to use.
}

void NetworkStorageSession::deleteCookie(const Cookie&)
{
    // FIXME: Implement for WebKit to use.
}

Vector<Cookie> NetworkStorageSession::getAllCookies()
{
    // FIXME: Implement for WebKit to use.
    return { };
}

Vector<Cookie> NetworkStorageSession::getCookies(const URL&)
{
    // FIXME: Implement for WebKit to use.
    return { };
}

void NetworkStorageSession::flushCookieStore()
{
    // FIXME: Implement for WebKit to use.
}

} // namespace WebCore

#endif // USE(CURL)
