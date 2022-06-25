/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#pragma once

#import "PushAppBundle.h"
#import "PushClientConnection.h"
#import <wtf/CompletionHandler.h>
#import <wtf/OSObjectPtr.h>
#import <wtf/spi/darwin/XPCSPI.h>
#import <wtf/text/WTFString.h>

namespace WebPushD {

class PushAppBundle;
class ClientConnection;

class AppBundleRequest : public PushAppBundleClient {
    WTF_MAKE_FAST_ALLOCATED;
public:
    virtual ~AppBundleRequest();

    virtual const char* transactionDescription() const = 0;

    void start();
    void cancel();

protected:
    AppBundleRequest(ClientConnection&, const String& originString);

    void cleanupAfterCompletionHandler();

    WeakPtr<ClientConnection> m_connection;
    String m_originString;
    RefPtr<PushAppBundle> m_appBundle;
    OSObjectPtr<os_transaction_t> m_transaction;

private:
    virtual void startInternal() = 0;
};

template<typename CompletionType>
class AppBundleRequestImpl : public AppBundleRequest {
protected:
    AppBundleRequestImpl(ClientConnection& connection, const String& originString, CompletionHandler<void(CompletionType)>&& completionHandler)
        : AppBundleRequest(connection, originString)
        , m_completionHandler(WTFMove(completionHandler))
    {
    }

    virtual ~AppBundleRequestImpl()
    {
        ASSERT(!m_appBundle);
    }

    void callCompletionHandlerAndCleanup(CompletionType result)
    {
        ASSERT(m_completionHandler);
        m_completionHandler(result);

        cleanupAfterCompletionHandler();
    }

    virtual void startInternal() = 0;
    virtual const char* transactionDescription() const = 0;

private:
    CompletionHandler<void(CompletionType)> m_completionHandler;
};

class AppBundlePermissionsRequest : public AppBundleRequestImpl<bool> {
public:
    AppBundlePermissionsRequest(ClientConnection& connection, const String& originString, CompletionHandler<void(bool)>&& completionHandler)
        : AppBundleRequestImpl(connection, originString, WTFMove(completionHandler))
    {
    }

private:
    const char* transactionDescription() const final { return "WebPush Permissions Bundle Request"; }

    void startInternal() final;
    void didCheckForExistingBundle(PushAppBundle&, PushAppBundleExists) final;
    void didDeleteExistingBundleWithError(PushAppBundle&, NSError *) final { RELEASE_ASSERT_NOT_REACHED(); }
    void didCreateAppBundle(PushAppBundle&, PushAppBundleCreationResult) final;
};

class AppBundleDeletionRequest : public AppBundleRequestImpl<const String&> {
public:
    AppBundleDeletionRequest(ClientConnection& connection, const String& originString, CompletionHandler<void(const String&)>&& completionHandler)
        : AppBundleRequestImpl(connection, originString,  WTFMove(completionHandler))
    {
    }

private:
    const char* transactionDescription() const final { return "WebPush Bundle Deletion Request"; }

    void startInternal() final;
    void didCheckForExistingBundle(PushAppBundle&, PushAppBundleExists) final  { RELEASE_ASSERT_NOT_REACHED(); }
    void didDeleteExistingBundleWithError(PushAppBundle&, NSError *) final;
    void didCreateAppBundle(PushAppBundle&, PushAppBundleCreationResult) final  { RELEASE_ASSERT_NOT_REACHED(); }
};

} // namespace WebPushD
