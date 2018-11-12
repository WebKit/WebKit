/*
 * Copyright (C) 2018 Sony Interactive Entertainment Inc.
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

#if USE(CURL)

#include <WebCore/CookieJarDB.h>
#include <WebCore/CookieUtil.h>
#include <WebCore/FileSystem.h>
#include <memory>

using namespace WebCore;

namespace TestWebKitAPI {

namespace Curl {

class CurlCookies: public testing::Test {
public:
    void SetUp() final
    {
        m_cookieDirectory = FileSystem::createTemporaryDirectory();

        m_cookieJar = std::make_unique<WebCore::CookieJarDB>(FileSystem::pathByAppendingComponent(m_cookieDirectory, "cookiedb.sql"));
        m_cookieJar->open();
        m_cookieJar->setEnabled(true);
    }

    void TearDown() final
    {
        m_cookieJar = nullptr;

        FileSystem::deleteNonEmptyDirectory(m_cookieDirectory);
        m_cookieDirectory = String();
    }

protected:
    std::unique_ptr<CookieJarDB> m_cookieJar;
    String m_cookieDirectory;
};

TEST_F(CurlCookies, TestHttpOnlyCase)
{
    // success: from network
    EXPECT_TRUE(m_cookieJar->setCookie("http://example.com", "foo=bar; HttpOnly", CookieJarDB::Source::Network));
    // success: wildcard of domains
    EXPECT_TRUE(m_cookieJar->setCookie("http://example.com", "bingo=bongo;", CookieJarDB::Source::Script));
    // failure: foo is already stored as HttpOnly
    EXPECT_FALSE(m_cookieJar->setCookie("http://example.com", "foo=bar;", CookieJarDB::Source::Script));
    // failure: inconsistent. Source is Script, but attribute says HttpOnly
    EXPECT_FALSE(m_cookieJar->setCookie("http://example.com", "foo=bar; HttpOnly", CookieJarDB::Source::Script));
}

}

}

#endif // USE(CURL)
