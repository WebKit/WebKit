/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#import "config.h"
#import "Test.h"

#import "WKRemoteObjectRegistry_Shared.h"
#import "PlatformUtilities.h"
#import "PlatformWebView.h"
#import <WebKit2/WKRetainPtr.h>
#import <WebKit2/WKRemoteObjectInterface.h>
#import <WebKit2/WKRemoteObjectRegistryPrivate.h>

#if WK_API_ENABLED

namespace TestWebKitAPI {

static bool connectionEstablished;
static WKRemoteObjectRegistry *remoteObjectRegistry;


/* WKConnectionClient */
static void connectionDidReceiveMessage(WKConnectionRef connection, WKStringRef messageName, WKTypeRef messageBody, const void *clientInfo)
{
    // FIXME: Implement this.
}

static void connectionDidClose(WKConnectionRef connection, const void* clientInfo)
{
    // FIXME: Implement this.
}

/* WKContextConnectionClient */
static void didCreateConnection(WKContextRef context, WKConnectionRef connection, const void* clientInfo)
{
    connectionEstablished = true;

    // Setup a client on the connection so we can listen for messages and
    // tear down notifications.
    WKConnectionClient connectionClient;
    memset(&connectionClient, 0, sizeof(connectionClient));
    connectionClient.version = WKConnectionClientCurrentVersion;
    connectionClient.clientInfo = 0;
    connectionClient.didReceiveMessage = connectionDidReceiveMessage;
    connectionClient.didClose = connectionDidClose;
    WKConnectionSetConnectionClient(connection, &connectionClient);

    // Store off the conneciton to use.
    remoteObjectRegistry = [[WKRemoteObjectRegistry alloc] _initWithConnectionRef:connection];
}

TEST(WebKit2, WKRemoteObjectRegistryTest)
{
    WKRetainPtr<WKContextRef> context(AdoptWK, Util::createContextForInjectedBundleTest("WKConnectionTest"));

    // Set up the context's connection client so that we can access the connection when
    // it is created.
    WKContextConnectionClient contextConnectionClient;
    memset(&contextConnectionClient, 0, sizeof(contextConnectionClient));
    contextConnectionClient.version = kWKContextConnectionClientCurrentVersion;
    contextConnectionClient.clientInfo = 0;
    contextConnectionClient.didCreateConnection = didCreateConnection;
    WKContextSetConnectionClient(context.get(), &contextConnectionClient);

    // Load a simple page to start the WebProcess and establish a connection.
    PlatformWebView webView(context.get());
    WKRetainPtr<WKURLRef> url(AdoptWK, Util::createURLForResource("simple", "html"));
    WKPageLoadURL(webView.page(), url.get());

    // Wait until the connection is established.
    Util::run(&connectionEstablished);
    ASSERT_NOT_NULL(remoteObjectRegistry);

    WKRemoteObjectInterface *bundleInterface = [WKRemoteObjectInterface remoteObjectInterfaceWithProtocol:@protocol(BundleInterface)];

    id <BundleInterface> remoteObjectProxy = [remoteObjectRegistry remoteObjectProxyWithInterface:bundleInterface];
    EXPECT_TRUE([remoteObjectProxy conformsToProtocol:@protocol(BundleInterface)]);

    [remoteObjectProxy sayHello];
}

} // namespace TestWebKitAPI

#endif // WK_API_ENABLED

