/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#import "HTTPServer.h"
#import "PlatformUtilities.h"
#import "RemoteObjectRegistry.h"
#import "Test.h"
#import "TestAwakener.h"
#import "TestNavigationDelegate.h"
#import "TestWKWebView.h"
#import "WKWebViewConfigurationExtras.h"
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/_WKFeature.h>
#import <WebKit/_WKRemoteObjectInterface.h>
#import <WebKit/_WKRemoteObjectRegistry.h>
#import <wtf/BlockPtr.h>
#import <wtf/RefCounted.h>
#import <wtf/RetainPtr.h>
#import <wtf/WeakObjCPtr.h>

TEST(RemoteObjectRegistry, Basic)
{
    __block bool isDone = false;

    @autoreleasepool {
        NSString * const testPlugInClassName = @"RemoteObjectRegistryPlugIn";
        auto configuration = retainPtr([WKWebViewConfiguration _test_configurationWithTestPlugInClassName:testPlugInClassName]);
        configuration.get()._groupIdentifier = @"testGroupIdentifier";
        auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:configuration.get()]);

        isDone = false;

        _WKRemoteObjectInterface *interface = remoteObjectInterface();
        id <RemoteObjectProtocol> object = [[webView _remoteObjectRegistry] remoteObjectProxyWithInterface:interface];

        [object sayHello:@"Hello, World!"];

        [webView evaluateJavaScript:@"helloString" completionHandler:^(id result, NSError *error) {
            EXPECT_TRUE([result isKindOfClass:[NSString class]]);
            EXPECT_WK_STREQ(result, @"Hello, World!");
            isDone = true;
        }];
        TestWebKitAPI::Util::run(&isDone);

        isDone = false;
        [object sayHello:@"Hello Again!" completionHandler:^(NSString *result) {
            EXPECT_TRUE([result isKindOfClass:[NSString class]]);
            EXPECT_WK_STREQ(result, @"Your string was 'Hello Again!'");
            isDone = true;
        }];
        TestWebKitAPI::Util::run(&isDone);

        isDone = false;
        [object selectionAndClickInformationForClickAtPoint:[NSValue valueWithPoint:NSMakePoint(12, 34)] completionHandler:^(NSDictionary *result) {
            EXPECT_TRUE([result isEqual:@{ @"URL": [NSURL URLWithString:@"http://www.webkit.org/"] }]);
            isDone = true;
        }];
        TestWebKitAPI::Util::run(&isDone);

        isDone = false;
        [object takeRange:NSMakeRange(345, 123) completionHandler:^(NSUInteger location, NSUInteger length) {
            EXPECT_EQ(345U, location);
            EXPECT_EQ(123U, length);
            isDone = true;
        }];
        TestWebKitAPI::Util::run(&isDone);

        isDone = false;
        auto initialAwakener = adoptNS([[TestAwakener alloc] initWithValue:42]);
        [object sendAwakener:initialAwakener.get() completionHandler:^(TestAwakener *awakener) {
            EXPECT_EQ(awakener.value, 42);
            isDone = true;
        }];
        TestWebKitAPI::Util::run(&isDone);

        isDone = false;
        [object takeSize:CGSizeMake(123.45, 678.91) completionHandler:^(CGFloat width, CGFloat height) {
            EXPECT_EQ(123.45, width);
            EXPECT_EQ(678.91, height);
            isDone = true;
        }];
        TestWebKitAPI::Util::run(&isDone);

        isDone = false;

        [object takeUnsignedLongLong:std::numeric_limits<unsigned long long>::max() completionHandler:^(unsigned long long value) {
            EXPECT_EQ(std::numeric_limits<unsigned long long>::max(), value);
            isDone = true;
        }];
        TestWebKitAPI::Util::run(&isDone);

        isDone = false;

        [object takeLongLong:std::numeric_limits<long long>::max() completionHandler:^(long long value) {
            EXPECT_EQ(std::numeric_limits<long long>::max(), value);
            isDone = true;
        }];
        TestWebKitAPI::Util::run(&isDone);

        isDone = false;

        [object takeUnsignedLong:std::numeric_limits<unsigned long>::max() completionHandler:^(unsigned long value) {
            EXPECT_EQ(std::numeric_limits<unsigned long>::max(), value);
            isDone = true;
        }];
        TestWebKitAPI::Util::run(&isDone);

        isDone = false;

        [object takeLong:std::numeric_limits<long>::max() completionHandler:^(long value) {
            EXPECT_EQ(std::numeric_limits<long>::max(), value);
            isDone = true;
        }];
        TestWebKitAPI::Util::run(&isDone);

        isDone = false;

        NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"https://webkit.org/"]];
        auto response = adoptNS([[NSHTTPURLResponse alloc] initWithURL:[NSURL URLWithString:@"https://webkit.org/"] statusCode:200 HTTPVersion:@"HTTP/1.1" headerFields:@{ @"testFieldName" : @"testFieldValue" }]);
        NSError *error = [NSError errorWithDomain:@"testDomain" code:123 userInfo:@{@"a":@"b"}];
        auto protectionSpace = adoptNS([[NSURLProtectionSpace alloc] initWithHost:@"testHost" port:80 protocol:@"testProtocol" realm:@"testRealm" authenticationMethod:NSURLAuthenticationMethodHTTPDigest]);
        NSURLCredential *credential = [NSURLCredential credentialWithUser:@"testUser" password:@"testPassword" persistence:NSURLCredentialPersistenceForSession];
        id<NSURLAuthenticationChallengeSender> sender = nil;
        auto challenge = adoptNS([[NSURLAuthenticationChallenge alloc] initWithProtectionSpace:protectionSpace.get() proposedCredential:credential previousFailureCount:42 failureResponse:response.get() error:error sender:sender]);
        NSUUID *uuid = [NSUUID UUID];
        [object sendRequest:request response:response.get() challenge:challenge.get() error:error nsNull:[NSNull null] uuid:uuid completionHandler:^(NSURLRequest *deserializedRequest, NSURLResponse *deserializedResponse, NSURLAuthenticationChallenge *deserializedChallenge, NSError *deserializedError, id nsNull, id deserializedUUID) {
            EXPECT_WK_STREQ(deserializedRequest.URL.absoluteString, "https://webkit.org/");
            EXPECT_WK_STREQ([(NSHTTPURLResponse *)deserializedResponse allHeaderFields][@"testFieldName"], "testFieldValue");
            EXPECT_WK_STREQ(deserializedChallenge.protectionSpace.realm, "testRealm");
            EXPECT_WK_STREQ(deserializedError.domain, "testDomain");
            EXPECT_WK_STREQ(NSStringFromClass([nsNull class]), @"NSNull");
            EXPECT_WK_STREQ(uuid.UUIDString, [deserializedUUID UUIDString]);
            isDone = true;
        }];
        TestWebKitAPI::Util::run(&isDone);

        isDone = false;
        
        [object getGroupIdentifier:^(NSString *identifier) {
            EXPECT_WK_STREQ(identifier, "testGroupIdentifier");
            isDone = true;
        }];
        TestWebKitAPI::Util::run(&isDone);

        isDone = false;

        bool exceptionThrown = false;
        NSMutableDictionary *child = [NSMutableDictionary dictionaryWithObjectsAndKeys:@"foo", @"name", [NSNumber numberWithInt:1], @"value", nil];
        NSMutableDictionary *dictionaryWithCycle = [NSMutableDictionary dictionaryWithObjectsAndKeys:@"root", @"name", child, @"child", nil];
        [child setValue:dictionaryWithCycle forKey:@"parent"]; // Creates a cycle.
        @try {
            [object takeDictionary:dictionaryWithCycle completionHandler:^(NSDictionary* value) {
                EXPECT_TRUE(!value.count);
                isDone = true;
            }];
            TestWebKitAPI::Util::run(&isDone);
            isDone = false;
        } @catch (NSException *e) {
            exceptionThrown = true;
        }
        EXPECT_FALSE(exceptionThrown);

        class DoneWhenDestroyed : public RefCounted<DoneWhenDestroyed> {
        public:
            DoneWhenDestroyed(bool& isDone)
                : isDone(isDone) { }
            ~DoneWhenDestroyed() { isDone = true; }
        private:
            bool& isDone;
        };

        {
            RefPtr<DoneWhenDestroyed> doneWhenDestroyed = adoptRef(*new DoneWhenDestroyed(isDone));
            [object doNotCallCompletionHandler:[doneWhenDestroyed]() {
            }];
        }

        TestWebKitAPI::Util::run(&isDone);
    }
}

@interface LocalObject : NSObject<LocalObjectProtocol> {
@public
    BlockPtr<void()> completionHandlerFromWebProcess;
    bool hasCompletionHandler;
}
@end

@implementation LocalObject

- (void)doSomethingWithCompletionHandler:(void (^)(void))completionHandler
{
    completionHandlerFromWebProcess = completionHandler;
    hasCompletionHandler = true;
}

@end

TEST(RemoteObjectRegistry, CallReplyBlockAfterOriginatingWebViewDeallocates)
{
    auto localObject = adoptNS([[LocalObject alloc] init]);
    WeakObjCPtr<WKWebView> weakWebViewPtr;

    @autoreleasepool {
        NSString * const testPlugInClassName = @"RemoteObjectRegistryPlugIn";
        auto configuration = retainPtr([WKWebViewConfiguration _test_configurationWithTestPlugInClassName:testPlugInClassName]);
        auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:configuration.get()]);
        weakWebViewPtr = webView.get();

        _WKRemoteObjectInterface *interface = remoteObjectInterface();
        id <RemoteObjectProtocol> object = [[webView _remoteObjectRegistry] remoteObjectProxyWithInterface:interface];

        [[webView _remoteObjectRegistry] registerExportedObject:localObject.get() interface:localObjectInterface()];

        [object callUIProcessMethodWithReplyBlock];

        TestWebKitAPI::Util::run(&localObject->hasCompletionHandler);
    }

    while (true) {
        @autoreleasepool {
            if (!weakWebViewPtr.get())
                break;

            TestWebKitAPI::Util::spinRunLoop();
        }
    }

    localObject->completionHandlerFromWebProcess();
}

@interface StringReplyObject : NSObject<StringReplyObjectProtocol> {
@public
    bool calledCompletionHandler;
}
@end

@implementation StringReplyObject

- (void) methodWithInteger:(uint64_t)integer
{
}

- (void) methodWithCompletionHandler:(void (^)(id, NSString *))completionHandler
{
    completionHandler(@"a", @"b");
    calledCompletionHandler = true;
}

@end

TEST(RemoteObjectRegistry, CallReplyBlockWithInvalidTypeSignature)
{
    auto completedReplyObject = adoptNS([[LocalObject alloc] init]);
    auto stringReplyObject = adoptNS([[StringReplyObject alloc] init]);

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:[WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"RemoteObjectRegistryPlugIn"]]);

    [[webView _remoteObjectRegistry] registerExportedObject:completedReplyObject.get() interface:localObjectInterface()];
    [[webView _remoteObjectRegistry] registerExportedObject:stringReplyObject.get() interface:stringReplyObjectInterface()];

    _WKRemoteObjectInterface *interface = remoteObjectInterface();
    id<RemoteObjectProtocol> object = [[webView _remoteObjectRegistry] remoteObjectProxyWithInterface:interface];

    [object callUIProcessMethodWithInvalidTypeSignature];
    [object callUIProcessMethodWithReplyBlock];

    TestWebKitAPI::Util::run(&completedReplyObject->hasCompletionHandler);

    EXPECT_FALSE(stringReplyObject->calledCompletionHandler);
}

TEST(RemoteObjectRegistry, SerializeErrorWithCertificates)
{
    TestWebKitAPI::HTTPServer server({ }, TestWebKitAPI::HTTPServer::Protocol::Https);
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:[WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"RemoteObjectRegistryPlugIn"]]);
    auto delegate = adoptNS([TestNavigationDelegate new]);
    webView.get().navigationDelegate = delegate.get();
    [webView loadRequest:server.request()];
    NSError *error = [delegate waitForDidFailProvisionalNavigation];
    NSString *key = @"NSErrorPeerCertificateChainKey";
    EXPECT_WK_STREQ(error.domain, "NSURLErrorDomain");
    EXPECT_TRUE(error.userInfo[key]);
    
    _WKRemoteObjectInterface *interface = [_WKRemoteObjectInterface remoteObjectInterfaceWithProtocol:@protocol(RemoteObjectProtocol)];
    id <RemoteObjectProtocol> object = [[webView _remoteObjectRegistry] remoteObjectProxyWithInterface:interface];
    __block bool roundTripComplete = false;
    [object sendError:error completionHandler:^(NSError *deserializedError) {
        EXPECT_WK_STREQ(deserializedError.domain, "NSURLErrorDomain");
        NSArray *chain = deserializedError.userInfo[key];
        EXPECT_TRUE(chain);
        EXPECT_EQ(CFGetTypeID(chain[0]), SecCertificateGetTypeID());
        roundTripComplete = true;
    }];
    TestWebKitAPI::Util::run(&roundTripComplete);
}

#if ENABLE(IPC_TESTING_API)

TEST(RemoteObjectRegistry, CallReplyBlockWithBadInvocation)
{
    NSString * const testPlugInClassName = @"RemoteObjectRegistryPlugIn";
    auto configuration = retainPtr([WKWebViewConfiguration _test_configurationWithTestPlugInClassName:testPlugInClassName]);

    for (_WKFeature *feature in [WKPreferences _features]) {
        if ([feature.key isEqualToString:@"IPCTestingAPIEnabled"]) {
            [[configuration preferences] _setEnabled:YES forFeature:feature];
            break;
        }
    }

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 300) configuration: configuration.get()]);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:@"RemoteObjectRegistry-BadReplyBlock" withExtension:@"html"]]];

    [webView waitForMessage:@"Expecting InvokeMethod..."];

    __block bool invalidCallReceived = false;

    id stringReplyObjectProxy = [[webView _remoteObjectRegistry] remoteObjectProxyWithInterface:stringReplyObjectInterface()];
    [stringReplyObjectProxy methodWithCompletionHandler:^(id, NSString * reply) {
        NSLog(@"Failed, should not have received: %@", reply);
        invalidCallReceived = true;
    }];

    __block bool validCallReceived = false;

    [stringReplyObjectProxy methodWithCompletionHandler:^(id, NSString * reply) {
        NSLog(@"Success, received: %@", reply);
        validCallReceived = true;
    }];

    TestWebKitAPI::Util::run(&validCallReceived);

    TestWebKitAPI::Util::runFor(0.5_s);
    EXPECT_FALSE(invalidCallReceived);
}

#endif // ENABLE(IPC_TESTING_API)
