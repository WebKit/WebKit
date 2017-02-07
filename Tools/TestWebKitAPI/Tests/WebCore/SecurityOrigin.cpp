/*
 * Copyright (C) 2016 Igalia S.L.
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
#include "WTFStringUtilities.h"
#include <WebCore/FileSystem.h>
#include <WebCore/SecurityOrigin.h>
#include <WebCore/URL.h>
#include <wtf/MainThread.h>

using namespace WebCore;

namespace TestWebKitAPI {

class SecurityOriginTest : public testing::Test {
public:
    void SetUp() final {
        WTF::initializeMainThread();

        // create temp file
        PlatformFileHandle handle;
        m_tempFilePath = openTemporaryFile("tempTestFile", handle);
        closeFile(handle);
        
        m_spaceContainingFilePath = openTemporaryFile("temp Empty Test File", handle);
        closeFile(handle);
        
        m_bangContainingFilePath = openTemporaryFile("temp!Empty!Test!File", handle);
        closeFile(handle);
        
        m_quoteContainingFilePath = openTemporaryFile("temp\"Empty\"TestFile", handle);
        closeFile(handle);
    }

    void TearDown() override
    {
        deleteFile(m_tempFilePath);
        deleteFile(m_spaceContainingFilePath);
        deleteFile(m_bangContainingFilePath);
        deleteFile(m_quoteContainingFilePath);
    }
    
    const String& tempFilePath() { return m_tempFilePath; }
    const String& spaceContainingFilePath() { return m_spaceContainingFilePath; }
    const String& bangContainingFilePath() { return m_bangContainingFilePath; }
    const String& quoteContainingFilePath() { return m_quoteContainingFilePath; }
    
private:
    String m_tempFilePath;
    String m_spaceContainingFilePath;
    String m_bangContainingFilePath;
    String m_quoteContainingFilePath;
};

TEST_F(SecurityOriginTest, SecurityOriginConstructors)
{
    Ref<SecurityOrigin> o1 = SecurityOrigin::create("http", "example.com", std::optional<uint16_t>(80));
    Ref<SecurityOrigin> o2 = SecurityOrigin::create("http", "example.com", std::optional<uint16_t>());
    Ref<SecurityOrigin> o3 = SecurityOrigin::createFromString("http://example.com");
    Ref<SecurityOrigin> o4 = SecurityOrigin::createFromString("http://example.com:80");
    Ref<SecurityOrigin> o5 = SecurityOrigin::create(URL(URL(), "http://example.com"));
    Ref<SecurityOrigin> o6 = SecurityOrigin::create(URL(URL(), "http://example.com:80"));

    EXPECT_EQ(String("http"), o1->protocol());
    EXPECT_EQ(String("http"), o2->protocol());
    EXPECT_EQ(String("http"), o3->protocol());
    EXPECT_EQ(String("http"), o4->protocol());
    EXPECT_EQ(String("http"), o5->protocol());
    EXPECT_EQ(String("http"), o6->protocol());

    EXPECT_EQ(String("example.com"), o1->host());
    EXPECT_EQ(String("example.com"), o2->host());
    EXPECT_EQ(String("example.com"), o3->host());
    EXPECT_EQ(String("example.com"), o4->host());
    EXPECT_EQ(String("example.com"), o5->host());
    EXPECT_EQ(String("example.com"), o6->host());

    EXPECT_FALSE(o1->port());
    EXPECT_FALSE(o2->port());
    EXPECT_FALSE(o3->port());
    EXPECT_FALSE(o4->port());
    EXPECT_FALSE(o5->port());
    EXPECT_FALSE(o6->port());

    EXPECT_EQ("http://example.com", o1->toString());
    EXPECT_EQ("http://example.com", o2->toString());
    EXPECT_EQ("http://example.com", o3->toString());
    EXPECT_EQ("http://example.com", o4->toString());
    EXPECT_EQ("http://example.com", o5->toString());
    EXPECT_EQ("http://example.com", o6->toString());

    EXPECT_TRUE(o1->isSameOriginAs(o2.get()));
    EXPECT_TRUE(o1->isSameOriginAs(o3.get()));
    EXPECT_TRUE(o1->isSameOriginAs(o4.get()));
    EXPECT_TRUE(o1->isSameOriginAs(o5.get()));
    EXPECT_TRUE(o1->isSameOriginAs(o6.get()));
}

TEST_F(SecurityOriginTest, SecurityOriginFileBasedConstructors)
{
    auto tempFileOrigin = SecurityOrigin::create(URL(URL(), "file:///" + tempFilePath()));
    auto spaceContainingOrigin = SecurityOrigin::create(URL(URL(), "file:///" + spaceContainingFilePath()));
    auto bangContainingOrigin = SecurityOrigin::create(URL(URL(), "file:///" + bangContainingFilePath()));
    auto quoteContainingOrigin = SecurityOrigin::create(URL(URL(), "file:///" + quoteContainingFilePath()));
    
    EXPECT_EQ(String("file"), tempFileOrigin->protocol());
    EXPECT_EQ(String("file"), spaceContainingOrigin->protocol());
    EXPECT_EQ(String("file"), bangContainingOrigin->protocol());
    EXPECT_EQ(String("file"), quoteContainingOrigin->protocol());

    EXPECT_TRUE(tempFileOrigin->isSameOriginAs(spaceContainingOrigin.get()));
    EXPECT_TRUE(tempFileOrigin->isSameOriginAs(bangContainingOrigin.get()));
    EXPECT_TRUE(tempFileOrigin->isSameOriginAs(quoteContainingOrigin.get()));
    
    EXPECT_TRUE(tempFileOrigin->isSameSchemeHostPort(spaceContainingOrigin.get()));
    EXPECT_TRUE(tempFileOrigin->isSameSchemeHostPort(bangContainingOrigin.get()));
    EXPECT_TRUE(tempFileOrigin->isSameSchemeHostPort(quoteContainingOrigin.get()));

    EXPECT_TRUE(tempFileOrigin->canAccess(spaceContainingOrigin.get()));
    EXPECT_TRUE(tempFileOrigin->canAccess(bangContainingOrigin.get()));
    EXPECT_TRUE(tempFileOrigin->canAccess(quoteContainingOrigin.get()));
}

} // namespace TestWebKitAPI
