/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "WebMediaPlayerClientImpl.h"

#include <gtest/gtest.h>
#include <wtf/PassOwnPtr.h>

using namespace WebKit;

namespace {

class FakeWebMediaPlayerClientImpl : public WebMediaPlayerClientImpl {
public:
    static PassOwnPtr<FakeWebMediaPlayerClientImpl> create() { return adoptPtr(new FakeWebMediaPlayerClientImpl()); }

private:
    FakeWebMediaPlayerClientImpl() { }
};

class FakeVideoFrameProviderClient : public WebVideoFrameProvider::Client {
public:
    static PassOwnPtr<FakeVideoFrameProviderClient> create(WebVideoFrameProvider* provider)
    {
        return adoptPtr(new FakeVideoFrameProviderClient(provider));
    }

    virtual ~FakeVideoFrameProviderClient()
    {
        if (m_provider)
            m_provider->setVideoFrameProviderClient(0);
    }

    // WebKit::WebVideoFrameProvider::Client implementation.
    virtual void didReceiveFrame() { }
    virtual void didUpdateMatrix(const float*) { }
    virtual void stopUsingProvider()
    {
        m_provider = 0;
    }

    WebVideoFrameProvider* provider() const { return m_provider; }

private:
    FakeVideoFrameProviderClient(WebVideoFrameProvider* provider)
        : m_provider(provider)
    {
        m_provider->setVideoFrameProviderClient(this);
    }

    WebVideoFrameProvider* m_provider;
};

TEST(WebMediaPlayerClientImplTest, InitialNullVideoClient)
{
    // No explict checks in this test; just make sure it doesn't crash.
    OwnPtr<WebMediaPlayerClientImpl> provider(FakeWebMediaPlayerClientImpl::create());
    provider->setVideoFrameProviderClient(0);
}

TEST(WebMediaPlayerClientImplTest, SetAndUnsetVideoClient)
{
    OwnPtr<WebMediaPlayerClientImpl> provider(FakeWebMediaPlayerClientImpl::create());
    OwnPtr<FakeVideoFrameProviderClient> client(FakeVideoFrameProviderClient::create(provider.get()));

    EXPECT_EQ(client->provider(), provider.get());

    provider->setVideoFrameProviderClient(0);
    ASSERT_FALSE(client->provider());
}

TEST(WebMediaPlayerClientImplTest, DestroyProvider)
{
    OwnPtr<WebMediaPlayerClientImpl> provider(FakeWebMediaPlayerClientImpl::create());
    OwnPtr<FakeVideoFrameProviderClient> client(FakeVideoFrameProviderClient::create(provider.get()));

    EXPECT_EQ(client->provider(), provider);
    provider.clear();
    ASSERT_FALSE(client->provider());
}

TEST(WebMediaPlayerClientImplTest, SetMultipleVideoClients)
{
    OwnPtr<WebMediaPlayerClientImpl> provider(FakeWebMediaPlayerClientImpl::create());
    OwnPtr<FakeVideoFrameProviderClient> firstClient(FakeVideoFrameProviderClient::create(provider.get()));
    EXPECT_EQ(firstClient->provider(), provider);

    OwnPtr<FakeVideoFrameProviderClient> secondClient(FakeVideoFrameProviderClient::create(provider.get()));
    EXPECT_FALSE(firstClient->provider());
    EXPECT_EQ(secondClient->provider(), provider);

    provider.clear();
    ASSERT_FALSE(firstClient->provider());
    ASSERT_FALSE(secondClient->provider());
}

}
