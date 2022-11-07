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

#include "Test.h"
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
        m_tempFilePath = FileSystem::openTemporaryFile("tempTestFile"_s, handle);
        FileSystem::closeFile(handle);
        
        m_spaceContainingFilePath = FileSystem::openTemporaryFile("temp Empty Test File"_s, handle);
        FileSystem::closeFile(handle);
        
        m_bangContainingFilePath = FileSystem::openTemporaryFile("temp!Empty!Test!File"_s, handle);
        FileSystem::closeFile(handle);
        
        m_quoteContainingFilePath = FileSystem::openTemporaryFile("temp\"Empty\"TestFile"_s, handle);
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
    auto o1 = SecurityOrigin::create("http"_s, "example.com"_s, std::optional<uint16_t>(80));
    auto o2 = SecurityOrigin::create("http"_s, "example.com"_s, std::optional<uint16_t>());
    auto o3 = SecurityOrigin::createFromString("http://example.com"_s);
    auto o4 = SecurityOrigin::createFromString("http://example.com:80"_s);
    auto o5 = SecurityOrigin::create(URL { "http://example.com"_str });
    auto o6 = SecurityOrigin::create(URL { "http://example.com:80"_str });
    auto o7 = SecurityOrigin::createFromString("http://example.com:81"_s);
    auto o8 = SecurityOrigin::createFromString("unrecognized://host"_s);
#if PLATFORM(COCOA)
    auto o9 = SecurityOrigin::createFromString("x-apple-ql-id://host"_s);
    auto o10 = SecurityOrigin::createFromString("x-apple-ql-magic://host"_s);
    auto o11 = SecurityOrigin::createFromString("x-apple-ql-id2://host"_s);
#endif

    EXPECT_EQ(String("http"_s), o1->protocol());
    EXPECT_EQ(String("http"_s), o2->protocol());
    EXPECT_EQ(String("http"_s), o3->protocol());
    EXPECT_EQ(String("http"_s), o4->protocol());
    EXPECT_EQ(String("http"_s), o5->protocol());
    EXPECT_EQ(String("http"_s), o6->protocol());
    EXPECT_EQ(String("http"_s), o7->protocol());
    EXPECT_EQ(emptyString(), o8->protocol());
#if PLATFORM(COCOA)
    EXPECT_EQ(String("x-apple-ql-id"_s), o9->protocol());
    EXPECT_EQ(String("x-apple-ql-magic"_s), o10->protocol());
    EXPECT_EQ(String("x-apple-ql-id2"_s), o11->protocol());
#endif

    EXPECT_EQ(String("example.com"_s), o1->host());
    EXPECT_EQ(String("example.com"_s), o2->host());
    EXPECT_EQ(String("example.com"_s), o3->host());
    EXPECT_EQ(String("example.com"_s), o4->host());
    EXPECT_EQ(String("example.com"_s), o5->host());
    EXPECT_EQ(String("example.com"_s), o6->host());
    EXPECT_EQ(String("example.com"_s), o7->host());
    EXPECT_EQ(emptyString(), o8->host());
#if PLATFORM(COCOA)
    EXPECT_EQ(String("host"_s), o9->host());
    EXPECT_EQ(String("host"_s), o10->host());
    EXPECT_EQ(String("host"_s), o11->host());
#endif

    EXPECT_FALSE(o1->port());
    EXPECT_FALSE(o2->port());
    EXPECT_FALSE(o3->port());
    EXPECT_FALSE(o4->port());
    EXPECT_FALSE(o5->port());
    EXPECT_FALSE(o6->port());
    EXPECT_EQ(o7->port(), std::optional<uint16_t>(81));
    EXPECT_FALSE(o8->port());
#if PLATFORM(COCOA)
    EXPECT_FALSE(o9->port());
    EXPECT_FALSE(o10->port());
    EXPECT_FALSE(o11->port());
#endif

    EXPECT_EQ("http://example.com"_s, o1->toString());
    EXPECT_EQ("http://example.com"_s, o2->toString());
    EXPECT_EQ("http://example.com"_s, o3->toString());
    EXPECT_EQ("http://example.com"_s, o4->toString());
    EXPECT_EQ("http://example.com"_s, o5->toString());
    EXPECT_EQ("http://example.com"_s, o6->toString());
    EXPECT_EQ("http://example.com:81"_s, o7->toString());
    EXPECT_EQ("null"_s, o8->toString());
#if PLATFORM(COCOA)
    EXPECT_EQ("x-apple-ql-id://host"_s, o9->toString());
    EXPECT_EQ("x-apple-ql-magic://host"_s, o10->toString());
    EXPECT_EQ("x-apple-ql-id2://host"_s, o11->toString());
#endif

    EXPECT_TRUE(o1->isSameOriginAs(o2.get()));
    EXPECT_TRUE(o1->isSameOriginAs(o3.get()));
    EXPECT_TRUE(o1->isSameOriginAs(o4.get()));
    EXPECT_TRUE(o1->isSameOriginAs(o5.get()));
    EXPECT_TRUE(o1->isSameOriginAs(o6.get()));
    EXPECT_FALSE(o1->isSameOriginAs(o7.get()));
    EXPECT_FALSE(o1->isSameOriginAs(o8.get()));
#if PLATFORM(COCOA)
    EXPECT_FALSE(o1->isSameOriginAs(o9.get()));
    EXPECT_FALSE(o1->isSameOriginAs(o10.get()));
    EXPECT_FALSE(o1->isSameOriginAs(o11.get()));
#endif
}

TEST_F(SecurityOriginTest, SecurityOriginFileBasedConstructors)
{
    auto tempFileOrigin = SecurityOrigin::create(URL { "file:///" + tempFilePath() });
    auto spaceContainingOrigin = SecurityOrigin::create(URL { "file:///" + spaceContainingFilePath() });
    auto bangContainingOrigin = SecurityOrigin::create(URL { "file:///" + bangContainingFilePath() });
    auto quoteContainingOrigin = SecurityOrigin::create(URL { "file:///" + quoteContainingFilePath() });
    
    EXPECT_EQ(String("file"_s), tempFileOrigin->protocol());
    EXPECT_EQ(String("file"_s), spaceContainingOrigin->protocol());
    EXPECT_EQ(String("file"_s), bangContainingOrigin->protocol());
    EXPECT_EQ(String("file"_s), quoteContainingOrigin->protocol());

    EXPECT_TRUE(tempFileOrigin->isSameOriginAs(spaceContainingOrigin.get()));
    EXPECT_TRUE(tempFileOrigin->isSameOriginAs(bangContainingOrigin.get()));
    EXPECT_TRUE(tempFileOrigin->isSameOriginAs(quoteContainingOrigin.get()));
    
    EXPECT_TRUE(tempFileOrigin->isSameSchemeHostPort(spaceContainingOrigin.get()));
    EXPECT_TRUE(tempFileOrigin->isSameSchemeHostPort(bangContainingOrigin.get()));
    EXPECT_TRUE(tempFileOrigin->isSameSchemeHostPort(quoteContainingOrigin.get()));

    EXPECT_TRUE(tempFileOrigin->isSameOriginDomain(spaceContainingOrigin.get()));
    EXPECT_TRUE(tempFileOrigin->isSameOriginDomain(bangContainingOrigin.get()));
    EXPECT_TRUE(tempFileOrigin->isSameOriginDomain(quoteContainingOrigin.get()));
}

TEST_F(SecurityOriginTest, IsPotentiallyTrustworthy)
{
    // Potentially Trustworthy
    EXPECT_TRUE(SecurityOrigin::create(URL { "file:///" + tempFilePath() })->isPotentiallyTrustworthy());
    EXPECT_TRUE(SecurityOrigin::createFromString("blob:http://127.0.0.1/3D45F36F-C126-493A-A8AA-457FA495247B"_s)->isPotentiallyTrustworthy());
    EXPECT_TRUE(SecurityOrigin::createFromString("blob:http://localhost/3D45F36F-C126-493A-A8AA-457FA495247B"_s)->isPotentiallyTrustworthy());
    EXPECT_TRUE(SecurityOrigin::createFromString("blob:http://[::1]/3D45F36F-C126-493A-A8AA-457FA495247B"_s)->isPotentiallyTrustworthy());
    EXPECT_TRUE(SecurityOrigin::createFromString("blob:https://example.com/3D45F36F-C126-493A-A8AA-457FA495247B"_s)->isPotentiallyTrustworthy());
    EXPECT_TRUE(SecurityOrigin::createFromString("wss://example.com"_s)->isPotentiallyTrustworthy());
    EXPECT_TRUE(SecurityOrigin::createFromString("https://example.com"_s)->isPotentiallyTrustworthy());
    EXPECT_TRUE(SecurityOrigin::createFromString("http://127.0.0.0"_s)->isPotentiallyTrustworthy());
    EXPECT_TRUE(SecurityOrigin::createFromString("http://127.0.0.1"_s)->isPotentiallyTrustworthy());
    EXPECT_TRUE(SecurityOrigin::createFromString("http://127.0.0.2"_s)->isPotentiallyTrustworthy());
    EXPECT_TRUE(SecurityOrigin::createFromString("http://127.0.1.1"_s)->isPotentiallyTrustworthy());
    EXPECT_TRUE(SecurityOrigin::createFromString("http://127.1.1.1"_s)->isPotentiallyTrustworthy());
    EXPECT_TRUE(SecurityOrigin::createFromString("http://localhost:8000"_s)->isPotentiallyTrustworthy());
    EXPECT_TRUE(SecurityOrigin::createFromString("http://localhost"_s)->isPotentiallyTrustworthy());
    EXPECT_TRUE(SecurityOrigin::createFromString("http://loCALhoST"_s)->isPotentiallyTrustworthy());
    EXPECT_TRUE(SecurityOrigin::createFromString("http://foo.localhost"_s)->isPotentiallyTrustworthy());
    EXPECT_TRUE(SecurityOrigin::createFromString("http://Foo.loCaLhOsT"_s)->isPotentiallyTrustworthy());
    EXPECT_TRUE(SecurityOrigin::createFromString("http://foo.localhost:8000"_s)->isPotentiallyTrustworthy());
    EXPECT_TRUE(SecurityOrigin::createFromString("http://foo.bar.localhost:8000"_s)->isPotentiallyTrustworthy());
    EXPECT_FALSE(SecurityOrigin::createFromString("http://localhost.com"_s)->isPotentiallyTrustworthy());
    EXPECT_FALSE(SecurityOrigin::createFromString("http://foo.localhost.com"_s)->isPotentiallyTrustworthy());
    EXPECT_TRUE(SecurityOrigin::createFromString("http://[::1]"_s)->isPotentiallyTrustworthy());
#if PLATFORM(COCOA)
    EXPECT_TRUE(SecurityOrigin::createFromString("applewebdata:a"_s)->isPotentiallyTrustworthy());
#endif
#if PLATFORM(GTK) || PLATFORM(WPE)
    EXPECT_TRUE(SecurityOrigin::createFromString("resource:a"_s)->isPotentiallyTrustworthy());
#endif

    // Not Trustworthy
    EXPECT_FALSE(SecurityOrigin::createFromString("http:/malformed"_s)->isPotentiallyTrustworthy());
    EXPECT_FALSE(SecurityOrigin::createFromString("http://example.com"_s)->isPotentiallyTrustworthy());
    EXPECT_FALSE(SecurityOrigin::createFromString("http://31.13.77.36"_s)->isPotentiallyTrustworthy());
    EXPECT_FALSE(SecurityOrigin::createFromString("http://[2a03:2880:f10d:83:face:b00c::25de]"_s)->isPotentiallyTrustworthy());
    EXPECT_FALSE(SecurityOrigin::createFromString("ws://example.com"_s)->isPotentiallyTrustworthy());
    EXPECT_FALSE(SecurityOrigin::createFromString("blob:http://example.com/3D45F36F-C126-493A-A8AA-457FA495247B"_s)->isPotentiallyTrustworthy());
    EXPECT_FALSE(SecurityOrigin::createFromString("dummy:a"_s)->isPotentiallyTrustworthy());
    EXPECT_FALSE(SecurityOrigin::createFromString("blob:null/3D45F36F-C126-493A-A8AA-457FA495247B"_s)->isPotentiallyTrustworthy());
    EXPECT_FALSE(SecurityOrigin::createFromString("data:,a"_s)->isPotentiallyTrustworthy());
    EXPECT_FALSE(SecurityOrigin::createFromString("about:"_s)->isPotentiallyTrustworthy());
    EXPECT_FALSE(SecurityOrigin::createFromString("about:blank"_s)->isPotentiallyTrustworthy());
    EXPECT_FALSE(SecurityOrigin::createFromString("about:srcdoc"_s)->isPotentiallyTrustworthy());
    EXPECT_FALSE(SecurityOrigin::createFromString("javascript:srcdoc"_s)->isPotentiallyTrustworthy());
}

TEST_F(SecurityOriginTest, IsRegistrableDomainSuffix)
{
    auto exampleOrigin = SecurityOrigin::create(URL { "http://www.example.com"_str });
    EXPECT_TRUE(exampleOrigin->isMatchingRegistrableDomainSuffix("example.com"_s));
    EXPECT_TRUE(exampleOrigin->isMatchingRegistrableDomainSuffix("www.example.com"_s));
#if !ENABLE(PUBLIC_SUFFIX_LIST)
    EXPECT_TRUE(exampleOrigin->isMatchingRegistrableDomainSuffix("com"_s));
#endif
    EXPECT_FALSE(exampleOrigin->isMatchingRegistrableDomainSuffix(emptyString()));
    EXPECT_FALSE(exampleOrigin->isMatchingRegistrableDomainSuffix("."_s));
    EXPECT_FALSE(exampleOrigin->isMatchingRegistrableDomainSuffix(".example.com"_s));
    EXPECT_FALSE(exampleOrigin->isMatchingRegistrableDomainSuffix(".www.example.com"_s));
    EXPECT_FALSE(exampleOrigin->isMatchingRegistrableDomainSuffix("example.com."_s));
#if ENABLE(PUBLIC_SUFFIX_LIST)
    EXPECT_FALSE(exampleOrigin->isMatchingRegistrableDomainSuffix("com"_s));
#endif

    auto exampleDotOrigin = SecurityOrigin::create(URL { "http://www.example.com."_str });
    EXPECT_TRUE(exampleDotOrigin->isMatchingRegistrableDomainSuffix("example.com."_s));
    EXPECT_TRUE(exampleDotOrigin->isMatchingRegistrableDomainSuffix("www.example.com."_s));
#if !ENABLE(PUBLIC_SUFFIX_LIST)
    EXPECT_TRUE(exampleOrigin->isMatchingRegistrableDomainSuffix("com."_s));
#endif
    EXPECT_FALSE(exampleDotOrigin->isMatchingRegistrableDomainSuffix(emptyString()));
    EXPECT_FALSE(exampleDotOrigin->isMatchingRegistrableDomainSuffix("."_s));
    EXPECT_FALSE(exampleDotOrigin->isMatchingRegistrableDomainSuffix(".example.com."_s));
    EXPECT_FALSE(exampleDotOrigin->isMatchingRegistrableDomainSuffix(".www.example.com."_s));
    EXPECT_FALSE(exampleDotOrigin->isMatchingRegistrableDomainSuffix("example.com"_s));
#if ENABLE(PUBLIC_SUFFIX_LIST)
    EXPECT_FALSE(exampleDotOrigin->isMatchingRegistrableDomainSuffix("com"_s));
#endif

    auto ipOrigin = SecurityOrigin::create(URL { "http://127.0.0.1"_str });
    EXPECT_TRUE(ipOrigin->isMatchingRegistrableDomainSuffix("127.0.0.1"_s, true));
    EXPECT_FALSE(ipOrigin->isMatchingRegistrableDomainSuffix("127.0.0.2"_s, true));

    auto comOrigin = SecurityOrigin::create(URL { "http://com"_str });
    EXPECT_TRUE(comOrigin->isMatchingRegistrableDomainSuffix("com"_s));
}

} // namespace TestWebKitAPI
