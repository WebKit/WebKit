/*
 * Copyright (C) 2016 Igalia S.L.
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include <WebCore/SecurityOrigin.h>
#include <wtf/FileSystem.h>
#include <wtf/MainThread.h>
#include <wtf/URL.h>

using namespace WebCore;

namespace TestWebKitAPI {

class SecurityOriginTest : public testing::Test {
public:
    void SetUp() final {
        WTF::initializeMainThread();

        // create temp file
        FileSystem::PlatformFileHandle handle;
        m_tempFilePath = FileSystem::openTemporaryFile("tempTestFile", handle);
        FileSystem::closeFile(handle);
        
        m_spaceContainingFilePath = FileSystem::openTemporaryFile("temp Empty Test File", handle);
        FileSystem::closeFile(handle);
        
        m_bangContainingFilePath = FileSystem::openTemporaryFile("temp!Empty!Test!File", handle);
        FileSystem::closeFile(handle);
        
        m_quoteContainingFilePath = FileSystem::openTemporaryFile("temp\"Empty\"TestFile", handle);
        FileSystem::closeFile(handle);
    }

    void TearDown() override
    {
        FileSystem::deleteFile(m_tempFilePath);
        FileSystem::deleteFile(m_spaceContainingFilePath);
        FileSystem::deleteFile(m_bangContainingFilePath);
        FileSystem::deleteFile(m_quoteContainingFilePath);
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
    Ref<SecurityOrigin> o1 = SecurityOrigin::create("http", "example.com", Optional<uint16_t>(80));
    Ref<SecurityOrigin> o2 = SecurityOrigin::create("http", "example.com", Optional<uint16_t>());
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

TEST_F(SecurityOriginTest, IsPotentiallyTrustworthy)
{
    // Potentially Trustworthy
    EXPECT_TRUE(SecurityOrigin::create(URL(URL(), "file:///" + tempFilePath()))->isPotentiallyTrustworthy());
    EXPECT_TRUE(SecurityOrigin::createFromString("blob:http://127.0.0.1/3D45F36F-C126-493A-A8AA-457FA495247B")->isPotentiallyTrustworthy());
    EXPECT_TRUE(SecurityOrigin::createFromString("blob:http://localhost/3D45F36F-C126-493A-A8AA-457FA495247B")->isPotentiallyTrustworthy());
    EXPECT_TRUE(SecurityOrigin::createFromString("blob:http://[::1]/3D45F36F-C126-493A-A8AA-457FA495247B")->isPotentiallyTrustworthy());
    EXPECT_TRUE(SecurityOrigin::createFromString("blob:https://example.com/3D45F36F-C126-493A-A8AA-457FA495247B")->isPotentiallyTrustworthy());
    EXPECT_TRUE(SecurityOrigin::createFromString("data:,a")->isPotentiallyTrustworthy());
    EXPECT_TRUE(SecurityOrigin::createFromString("about:")->isPotentiallyTrustworthy());
    EXPECT_TRUE(SecurityOrigin::createFromString("about:blank")->isPotentiallyTrustworthy());
    EXPECT_TRUE(SecurityOrigin::createFromString("about:srcdoc")->isPotentiallyTrustworthy());
    EXPECT_TRUE(SecurityOrigin::createFromString("wss://example.com")->isPotentiallyTrustworthy());
    EXPECT_TRUE(SecurityOrigin::createFromString("https://example.com")->isPotentiallyTrustworthy());
    EXPECT_TRUE(SecurityOrigin::createFromString("http://127.0.0.0")->isPotentiallyTrustworthy());
    EXPECT_TRUE(SecurityOrigin::createFromString("http://127.0.0.1")->isPotentiallyTrustworthy());
    EXPECT_TRUE(SecurityOrigin::createFromString("http://127.0.0.2")->isPotentiallyTrustworthy());
    EXPECT_TRUE(SecurityOrigin::createFromString("http://127.0.1.1")->isPotentiallyTrustworthy());
    EXPECT_TRUE(SecurityOrigin::createFromString("http://127.1.1.1")->isPotentiallyTrustworthy());
    EXPECT_TRUE(SecurityOrigin::createFromString("http://localhost")->isPotentiallyTrustworthy());
    EXPECT_TRUE(SecurityOrigin::createFromString("http://loCALhoST")->isPotentiallyTrustworthy());
    EXPECT_TRUE(SecurityOrigin::createFromString("http://[::1]")->isPotentiallyTrustworthy());
#if PLATFORM(COCOA)
    EXPECT_TRUE(SecurityOrigin::createFromString("applewebdata:a")->isPotentiallyTrustworthy());
#endif
#if PLATFORM(GTK) || PLATFORM(WPE)
    EXPECT_TRUE(SecurityOrigin::createFromString("resource:a")->isPotentiallyTrustworthy());
#endif

    // Not Trustworthy
    EXPECT_FALSE(SecurityOrigin::createFromString("http:/malformed")->isPotentiallyTrustworthy());
    EXPECT_FALSE(SecurityOrigin::createFromString("http://example.com")->isPotentiallyTrustworthy());
    EXPECT_FALSE(SecurityOrigin::createFromString("http://31.13.77.36")->isPotentiallyTrustworthy());
    EXPECT_FALSE(SecurityOrigin::createFromString("http://[2a03:2880:f10d:83:face:b00c::25de]")->isPotentiallyTrustworthy());
    EXPECT_FALSE(SecurityOrigin::createFromString("ws://example.com")->isPotentiallyTrustworthy());
    EXPECT_FALSE(SecurityOrigin::createFromString("blob:http://example.com/3D45F36F-C126-493A-A8AA-457FA495247B")->isPotentiallyTrustworthy());
    EXPECT_FALSE(SecurityOrigin::createFromString("dummy:a")->isPotentiallyTrustworthy());
}

TEST_F(SecurityOriginTest, IsRegistrableDomainSuffix)
{
    auto exampleOrigin = SecurityOrigin::create(URL(URL(), "http://www.example.com"));
    EXPECT_TRUE(exampleOrigin->isMatchingRegistrableDomainSuffix("example.com"));
    EXPECT_TRUE(exampleOrigin->isMatchingRegistrableDomainSuffix("www.example.com"));
#if !ENABLE(PUBLIC_SUFFIX_LIST)
    EXPECT_TRUE(exampleOrigin->isMatchingRegistrableDomainSuffix("com"));
#endif
    EXPECT_FALSE(exampleOrigin->isMatchingRegistrableDomainSuffix(""));
    EXPECT_FALSE(exampleOrigin->isMatchingRegistrableDomainSuffix("."));
    EXPECT_FALSE(exampleOrigin->isMatchingRegistrableDomainSuffix(".example.com"));
    EXPECT_FALSE(exampleOrigin->isMatchingRegistrableDomainSuffix(".www.example.com"));
    EXPECT_FALSE(exampleOrigin->isMatchingRegistrableDomainSuffix("example.com."));
#if ENABLE(PUBLIC_SUFFIX_LIST)
    EXPECT_FALSE(exampleOrigin->isMatchingRegistrableDomainSuffix("com"));
#endif

    auto exampleDotOrigin = SecurityOrigin::create(URL(URL(), "http://www.example.com."));
    EXPECT_TRUE(exampleDotOrigin->isMatchingRegistrableDomainSuffix("example.com."));
    EXPECT_TRUE(exampleDotOrigin->isMatchingRegistrableDomainSuffix("www.example.com."));
#if !ENABLE(PUBLIC_SUFFIX_LIST)
    EXPECT_TRUE(exampleOrigin->isMatchingRegistrableDomainSuffix("com."));
#endif
    EXPECT_FALSE(exampleDotOrigin->isMatchingRegistrableDomainSuffix(""));
    EXPECT_FALSE(exampleDotOrigin->isMatchingRegistrableDomainSuffix("."));
    EXPECT_FALSE(exampleDotOrigin->isMatchingRegistrableDomainSuffix(".example.com."));
    EXPECT_FALSE(exampleDotOrigin->isMatchingRegistrableDomainSuffix(".www.example.com."));
    EXPECT_FALSE(exampleDotOrigin->isMatchingRegistrableDomainSuffix("example.com"));
#if ENABLE(PUBLIC_SUFFIX_LIST)
    EXPECT_FALSE(exampleDotOrigin->isMatchingRegistrableDomainSuffix("com"));
#endif

    auto ipOrigin = SecurityOrigin::create(URL(URL(), "http://127.0.0.1"));
    EXPECT_TRUE(ipOrigin->isMatchingRegistrableDomainSuffix("127.0.0.1", true));
    EXPECT_FALSE(ipOrigin->isMatchingRegistrableDomainSuffix("127.0.0.2", true));

    auto comOrigin = SecurityOrigin::create(URL(URL(), "http://com"));
    EXPECT_TRUE(comOrigin->isMatchingRegistrableDomainSuffix("com"));
}

} // namespace TestWebKitAPI
