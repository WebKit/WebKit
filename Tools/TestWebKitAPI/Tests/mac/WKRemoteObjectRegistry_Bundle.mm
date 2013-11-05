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
#import "InjectedBundleTest.h"

#import "PlatformUtilities.h"
#import "WKRemoteObjectRegistry_Shared.h"
#import <WebKit2/WKRemoteObjectRegistryPrivate.h>
#import <WebKit2/WKRemoteObjectInterface.h>
#import <WebKit2/WKRetainPtr.h>
#import <wtf/RetainPtr.h>

#if WK_API_ENABLED

@interface BundleObject : NSObject <BundleInterface>
@end

@implementation BundleObject

- (void)sayHello
{
    // FIXME: Implement.
}

- (void)testMethodWithString:(NSString *)string double:(double)d integer:(int)i
{
    // FIXME: Implement.
}

- (void)testMethodWithArray:(NSArray *)array dictionary:(NSDictionary *)dictinoary request:(NSURLRequest *)request
{
    // FIXME: Implement.
}

@end

namespace TestWebKitAPI {

class WKRemoteObjectRegistryTest : public InjectedBundleTest {
public:
    WKRemoteObjectRegistryTest(const std::string& identifier)
        : InjectedBundleTest(identifier)
    {
    }

    virtual void initialize(WKBundleRef bundle, WKTypeRef) override
    {
        m_objectRegistry = adoptNS([[WKRemoteObjectRegistry alloc] _initWithConnectionRef:WKBundleGetApplicationConnection(bundle)]);

        WKRemoteObjectInterface *bundleInterface = [WKRemoteObjectInterface remoteObjectInterfaceWithProtocol:@protocol(BundleInterface)];

        BundleObject *bundleObject = [[BundleObject alloc] init];
        [m_objectRegistry registerExportedObject:bundleObject interface:bundleInterface];

        WKConnectionClient connectionClient;
        memset(&connectionClient, 0, sizeof(connectionClient));
        connectionClient.version = WKConnectionClientCurrentVersion;
        connectionClient.clientInfo = this;
        connectionClient.didReceiveMessage = [](WKConnectionRef connection, WKStringRef messageName, WKTypeRef messageBody, const void *clientInfo) {
            const WKRemoteObjectRegistryTest* test = static_cast<const WKRemoteObjectRegistryTest*>(clientInfo);

            [test->m_objectRegistry.get() _handleMessageWithName:messageName body:messageBody];
        };

        WKConnectionSetConnectionClient(WKBundleGetApplicationConnection(bundle), &connectionClient);
    }

private:
    RetainPtr<WKRemoteObjectRegistry> m_objectRegistry;
};

static InjectedBundleTest::Register<WKRemoteObjectRegistryTest> registrar("WKRemoteObjectRegistry");

} // namespace TestWebKitAPI

#endif // WK_API_ENABLED
