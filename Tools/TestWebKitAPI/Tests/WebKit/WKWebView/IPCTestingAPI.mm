/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#import "Helpers/DeprecatedGlobalValues.h"
#import "Helpers/PlatformUtilities.h"
#import "RemoteObjectRegistry.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import "Helpers/Utilities.h"
#import "TestNavigationDelegate.h"
#import "TestUIDelegate.h"
#import <WebKit/WKFrameInfoPrivate.h>
#import <WebKit/WKHTTPCookieStore.h>
#import <WebKit/WKNavigationDelegatePrivate.h>
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKWebView.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <WebKit/_WKFeature.h>
#import <WebKit/_WKFrameTreeNode.h>
#import <WebKit/_WKRemoteObjectInterface.h>
#import <WebKit/_WKRemoteObjectRegistry.h>
#import <WebKit/_WKWebsiteDataStoreConfiguration.h>
#import <wtf/RetainPtr.h>
#import <wtf/cocoa/TypeCastsCocoa.h>
#import <wtf/text/MakeString.h>

static bool didCrash = false;
static RetainPtr<NSString> alertMessage;
static RetainPtr<NSString> promptDefault;
static RetainPtr<NSString> promptResult;

@interface IPCTestingAPIDelegate : NSObject <WKUIDelegate, WKNavigationDelegate>
- (BOOL)sayHelloWasCalled;
@end

@implementation IPCTestingAPIDelegate {
    BOOL _didCallSayHello;
}

- (void)webView:(WKWebView *)webView runJavaScriptAlertPanelWithMessage:(NSString *)message initiatedByFrame:(WKFrameInfo *)frame completionHandler:(void (^)(void))completionHandler
{
    alertMessage = message;
    done = true;
    completionHandler();
}

- (void)webView:(WKWebView *)webView runJavaScriptTextInputPanelWithPrompt:(NSString *)prompt defaultText:(NSString *)defaultText initiatedByFrame:(WKFrameInfo *)frame completionHandler:(void (^)(NSString *))completionHandler
{
    promptDefault = defaultText;
    done = true;
    completionHandler(promptResult.get());
}

- (void)_webView:(WKWebView *)webView webContentProcessDidTerminateWithReason:(_WKProcessTerminationReason)reason
{
    didCrash = false;
    done = true;
}

- (void)sayHello:(NSString *)hello completionHandler:(void (^)(NSString *))completionHandler
{
    _didCallSayHello = YES;
}

- (BOOL)sayHelloWasCalled
{
    return _didCallSayHello;
}

@end

TEST(IPCTestingAPI, IsDisabledByDefault)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);

    RetainPtr delegate = adoptNS([[IPCTestingAPIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];

    done = false;
    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html><script>alert(typeof(IPC));</script>"];
    TestWebKitAPI::Util::run(&done);
    EXPECT_STREQ([alertMessage UTF8String], "undefined");
}

// Note: There are more IPC tests using IPC testing API in `LayoutTests/ipc`.

#if ENABLE(IPC_TESTING_API)

static RetainPtr<TestWKWebView> createWebViewWithIPCTestingAPI()
{
    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    for (_WKFeature *feature in [WKPreferences _features]) {
        if ([feature.key isEqualToString:@"IPCTestingAPIEnabled"]) {
            [[configuration preferences] _setEnabled:YES forFeature:feature];
            break;
        }
    }
    return adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 300) configuration:configuration.get()]);
}

static RetainPtr<TestWKWebView> createWebViewWithIPCTestingAPIAndLockdownMode(bool lockdownModeEnabled)
{
    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

    for (_WKFeature *feature in [WKPreferences _features]) {
        if ([feature.key isEqualToString:@"IPCTestingAPIEnabled"]) {
            [[configuration preferences] _setEnabled:YES forFeature:feature];
            break;
        }
    }

    if (lockdownModeEnabled) {
        [WKProcessPool _setCaptivePortalModeEnabledGloballyForTesting:YES];

        RetainPtr<WKWebpagePreferences> webpagePreferences = adoptNS([[WKWebpagePreferences alloc] init]);
        [webpagePreferences setLockdownModeEnabled:YES];
        [configuration setDefaultWebpagePreferences:webpagePreferences.get()];
    } else {
        [WKProcessPool _setCaptivePortalModeEnabledGloballyForTesting:NO];

        RetainPtr<WKWebpagePreferences> webpagePreferences = adoptNS([[WKWebpagePreferences alloc] init]);
        [webpagePreferences setLockdownModeEnabled:NO];
        [configuration setDefaultWebpagePreferences:webpagePreferences.get()];
    }

    return adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 300) configuration:configuration.get()]);
}

// FIX ME: Re-enable this test once https://bugs.webkit.org/show_bug.cgi?id=300930 is resolved
#if PLATFORM(MAC) && CPU(X86_64) && !defined(NDEBUG)
TEST(IPCTestingAPI, DISABLED_CanDetectNilReplyBlocks)
#else
TEST(IPCTestingAPI, CanDetectNilReplyBlocks)
#endif
{
    auto webView = createWebViewWithIPCTestingAPI();

    RetainPtr delegate = adoptNS([[IPCTestingAPIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];

    _WKRemoteObjectInterface *interface = remoteObjectInterface();
    [[webView _remoteObjectRegistry] remoteObjectProxyWithInterface:interface];
    [[webView _remoteObjectRegistry] registerExportedObject:delegate.get() interface:interface];

    done = false;
    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html><script>buf = new Uint8Array(["
        // Strings in this buffer are encoded as follows:
        // string length, 3 NUL bytes, 0x1 byte, then string contents
        // For example, this string is 0x14 length (20 bytes), 3 NUL bytes + 0x1, then "RemoteObjectProtocol"
        "0x14,0x0,0x0,0x0,0x1,0x52,0x65,0x6d,0x6f,0x74,0x65,0x4f,0x62,0x6a,0x65,0x63,0x74,0x50,0x72,0x6f,0x74,0x6f,0x63,0x6f,0x6c,"
        // padding + "invocation"
        "0x0,0x0,0x0,0x9,0x0,0x0,0x0,0x2,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0xa,0x0,0x0,0x0,0x1,0x69,0x6e,0x76,0x6f,0x63,0x61,0x74,0x69,0x6f,0x6e,"
        // a serialized object + "typeString"
        "0x0,0x9,0x0,0x0,0x0,0xf5,0xeb,0x54,0xa9,0x3,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0xa,0x0,0x0,0x0,0x1,0x74,0x79,0x70,0x65,0x53,0x74,0x72,0x69,0x6e,0x67,0x0,"
        // a zeroed object + "$string"
        "0x9,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x2,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x7,0x0,0x0,0x0,0x1,0x24,0x73,0x74,0x72,0x69,0x6e,0x67,0x15,0x0,0x0,0x0,"
        // "v@:@.@.?" (an objective-C method signature) + "class"
        "0x6,0x0,0x0,0x0,0x1,0x76,0x40,0x3a,0x40,0x40,0x3f,0x0,0x6,0x0,0x0,0x0,0x1,0x24,0x63,0x6c,0x61,0x73,0x73,0x0,"
        // "NSString" + "selector"
        "0x15,0x0,0x0,0x0,0x8,0x0,0x0,0x0,0x1,0x4e,0x53,0x53,0x74,0x72,0x69,0x6e,0x67,0x0,0x0,0x0,0x8,0x0,0x0,0x0,0x1,0x73,0x65,0x6c,0x65,0x63,0x74,0x6f,0x72,0x0,0x0,0x0,"
        // a zeroed object + "$string"
        "0x9,0x0,0x0,0x0,0x2,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x7,0x0,0x0,0x0,0x1,0x24,0x73,0x74,0x72,0x69,0x6e,0x67,0x15,0x0,0x0,0x0,"
        // "sayHello:completionHandler:" (method name we're trying to call)
        "0x1b,0x0,0x0,0x0,0x1,0x73,0x61,0x79,0x48,0x65,0x6c,0x6c,0x6f,0x3a,0x63,0x6f,0x6d,0x70,0x6c,0x65,0x74,0x69,0x6f,0x6e,0x48,0x61,0x6e,0x64,0x6c,0x65,0x72,0x3a,"
        // "$class" + "NSString"
        "0x6,0x0,0x0,0x0,0x1,0x24,0x63,0x6c,0x61,0x73,0x73,0x0,0x15,0x0,0x0,0x0,0x8,0x0,0x0,0x0,0x1,0x4e,0x53,0x53,0x74,0x72,0x69,0x6e,0x67,0x0,0x0,0x0,"
        // "$class" + "NSInvocation"
        "0x6,0x0,0x0,0x0,0x1,0x24,0x63,0x6c,0x61,0x73,0x73,0x0,0x15,0x0,0x0,0x0,0xc,0x0,0x0,0x0,0x1,0x4e,0x53,0x49,0x6e,0x76,0x6f,0x63,0x61,0x74,0x69,0x6f,0x6e,0x0,0x0,0x0,"
        // "$objectStam" + zero object
        "0xd,0x0,0x0,0x0,0x1,0x24,0x6f,0x62,0x6a,0x65,0x63,0x74,0x53,0x74,0x61,0x6d,0x0,0x0,0x1,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x2,0x0,0x0,0x0,0x0,0x0,0x0,0x0,"
        // zeroed objects + ".NS.uuidbytes"
        "0x9,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x2,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0xc,0x0,0x0,0x0,0x91,0x4e,0x53,0x2e,0x75,0x75,0x69,0x64,0x62,0x79,0x74,0x65,0x73,0x0,0x0,0x0,"
        // some zeroed objects
        "0x8,0x0,0x0,0x0,0x10,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x29,0xc5,0x6d,0x2,0x13,0xa,0x4e,0xe7,0xaa,0xac,0x8,0x55,0xf2,0x66,0x2c,0x7c,"
        // "$class" + "NSUUID"
        "0x6,0x0,0x0,0x0,0x1,0x24,0x63,0x6c,0x61,0x73,0x73,0x0,0x15,0x0,0x0,0x0,0x6,0x0,0x0,0x0,0x1,0x4e,0x53,0x55,0x55,0x49,0x44,0x0,0x0,0x0,"
        // mostly zero objects + "v@?c" (objective-C method signature)
        "0x0,0x0,0x1,0x0,0x0,0x0,0x2c,0x0,0x0,0x0,0x59,0x1,0x0,0x0,0x0,0x9b,0x0,0x0,0x4,0x0,0x0,0x0,0x1,0x76,0x40,0x3f,0x63,0x0,]);"
        "for(var x=0; x<100; x++) IPC.sendMessage('UI', x, IPC.messages.RemoteObjectRegistry_InvokeMethod.name, [buf]);</script>"];
    TestWebKitAPI::Util::runFor(&done, 1_s);

    // Make sure sayHello was not called, as the reply block was nil.
    EXPECT_FALSE([delegate.get() sayHelloWasCalled]);
}

TEST(IPCTestingAPI, CanSendAlert)
{
    auto webView = createWebViewWithIPCTestingAPI();

    RetainPtr delegate = adoptNS([[IPCTestingAPIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];

    done = false;
    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html><script>IPC.sendSyncMessage('UI', IPC.webPageProxyID, IPC.messages.WebPageProxy_RunJavaScriptAlert.name, 100,"
        "[{type: 'FrameID', value: IPC.frameID}, {type: 'FrameInfoData', value: IPC}, {'type': 'String', 'value': 'hi'}]);</script>"];
    TestWebKitAPI::Util::run(&done);

    EXPECT_STREQ([alertMessage UTF8String], "hi");
}

TEST(IPCTestingAPI, AlertIsSyncMessage)
{
    auto webView = createWebViewWithIPCTestingAPI();

    RetainPtr delegate = adoptNS([[IPCTestingAPIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];

    done = false;
    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html><script>alert(IPC.messages.WebPageProxy_RunJavaScriptAlert.isSync);</script>"];
    TestWebKitAPI::Util::run(&done);

    EXPECT_STREQ([alertMessage UTF8String], "true");
}

TEST(IPCTestingAPI, CanSendInvalidAsyncMessageToUIProcessWithoutTermination)
{
    auto webView = createWebViewWithIPCTestingAPI();

    RetainPtr delegate = adoptNS([[IPCTestingAPIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];

    done = false;
    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html><script>"
        "IPC.sendMessage('UI', IPC.webPageProxyID, IPC.messages.WebPageProxy_ShowShareSheet.name, []);"
        "alert('hi')</script>"];
    TestWebKitAPI::Util::run(&done);

    EXPECT_STREQ([alertMessage UTF8String], "hi");
}

TEST(IPCTestingAPI, CanSendInvalidSyncMessageToUIProcessWithoutTermination)
{
    auto webView = createWebViewWithIPCTestingAPI();

    RetainPtr delegate = adoptNS([[IPCTestingAPIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];

    done = false;
    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html><script>"
        "try{IPC.sendSyncMessage('UI', IPC.webPageProxyID, IPC.messages.WebPageProxy_RunJavaScriptAlert.name, 100, [{type: 'FrameID', value: IPC.frameID}]);}catch(e){alert(e.message)}"
        "</script>"];
    TestWebKitAPI::Util::run(&done);

    EXPECT_STREQ([alertMessage UTF8String], "Receiver cancelled the reply due to invalid destination or deserialization error");
}

#if ENABLE(GPU_PROCESS)

TEST(IPCTestingAPI, CanSendSyncMessageToGPUProcess)
{
    auto webView = createWebViewWithIPCTestingAPI();

    RetainPtr delegate = adoptNS([[IPCTestingAPIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];

    done = false;
    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html><script>"
        "result = !!IPC.sendSyncMessage('GPU', 0, IPC.messages.GPUConnectionToWebProcess_EnsureAudioSession.name, 100, []);"
        "alert(result)</script>"];
    TestWebKitAPI::Util::run(&done);

    EXPECT_TRUE([alertMessage boolValue]);
}

TEST(IPCTestingAPI, CanSendAsyncMessageToGPUProcess)
{
    auto webView = createWebViewWithIPCTestingAPI();

    RetainPtr delegate = adoptNS([[IPCTestingAPIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];

    done = false;
    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html><script>(function test() {"
        "let c = IPC.connectionForProcessTarget('GPU');"
        "let cb = (result) => { window.result = result; alert(!!result); };"
        "c.sendWithAsyncReply(0, IPC.messages.RemoteAudioDestinationManager_StartAudioDestination.name, [{type: 'uint64_t', value: 12345}], cb);"
        "})();</script>"];
    TestWebKitAPI::Util::run(&done);

    EXPECT_TRUE([alertMessage boolValue]);
    EXPECT_STREQ([webView stringByEvaluatingJavaScript:@"result.arguments[0].type"].UTF8String, "bool");
    EXPECT_FALSE([webView stringByEvaluatingJavaScript:@"result.arguments[0].value"].boolValue);
}

TEST(IPCTestingAPI, CanSendInvalidAsyncMessageToGPUProcessWithoutTermination)
{
    auto webView = createWebViewWithIPCTestingAPI();

    RetainPtr delegate = adoptNS([[IPCTestingAPIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];

    done = false;
    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html><script>(function test() {"
        "let c = IPC.connectionForProcessTarget('GPU');"
        "c.sendMessage(0, IPC.messages.GPUConnectionToWebProcess_CreateRenderingBackend.name, []);"
        "let cb = (result) => { window.result = result; alert(!!result); };"
        "c.sendWithAsyncReply(0, IPC.messages.RemoteAudioDestinationManager_StartAudioDestination.name, [{type: 'uint64_t', value: 12345}], cb);"
        "})();</script>"];
    TestWebKitAPI::Util::run(&done);

    EXPECT_TRUE([alertMessage boolValue]);
    EXPECT_STREQ([webView stringByEvaluatingJavaScript:@"result.arguments[0].type"].UTF8String, "bool");
    EXPECT_FALSE([webView stringByEvaluatingJavaScript:@"result.arguments[0].value"].boolValue);
}

#endif // ENABLE(GPU_PROCESS)

TEST(IPCTestingAPI, CanCreateSharedMemory)
{
    auto webView = createWebViewWithIPCTestingAPI();

    RetainPtr delegate = adoptNS([[IPCTestingAPIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];

    done = false;
    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html><script>const sharedMemory = IPC.createSharedMemory(8); alert(sharedMemory.toString());</script>"];
    TestWebKitAPI::Util::run(&done);

    EXPECT_STREQ([alertMessage UTF8String], "[object SharedMemory]");
    EXPECT_EQ([webView stringByEvaluatingJavaScript:@"new Int8Array(sharedMemory.readBytes(0))[0]"].intValue, 0);
    EXPECT_EQ([webView stringByEvaluatingJavaScript:@"sharedMemory.writeBytes(new Int8Array([1, 2, 4, 8, 16, 32]))"].intValue, 0);
    EXPECT_STREQ([webView stringByEvaluatingJavaScript:@"Array.from(new Int8Array(sharedMemory.readBytes(1, 3))).toString()"].UTF8String, "2,4,8");
    EXPECT_EQ([webView stringByEvaluatingJavaScript:@"sharedMemory.writeBytes(new Int8Array([101, 102, 103, 104, 105, 106]), 2, 3)"].intValue, 0);
    EXPECT_STREQ([webView stringByEvaluatingJavaScript:@"Array.from(new Int8Array(sharedMemory.readBytes())).toString()"].UTF8String, "1,2,101,102,103,32,0,0");
}

#if PLATFORM(COCOA)
TEST(IPCTestingAPI, CanSendSharedMemory)
{
    auto webView = createWebViewWithIPCTestingAPI();

    RetainPtr delegate = adoptNS([[IPCTestingAPIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];

    auto* html = @R"HTML(<!DOCTYPE html>
<body>
<script>
const sharedMemory = IPC.createSharedMemory(8);
sharedMemory.writeBytes(new Uint8Array(Array.from('hello').map((char) => char.charCodeAt(0))));
const result = IPC.sendSyncMessage('UI', 0, IPC.messages.WebPasteboardProxy_TestIPCSharedMemory.name, 100, [
    {type: 'String', value: 'Apple CFPasteboard general'},
    {type: 'String', value: 'text/plain'},
    {type: 'SharedMemory', value: sharedMemory, protection: 'ReadOnly'},
    {type: 'bool', value: 1}, {type: 'uint64_t', value: IPC.pageID}]);
alert(result.arguments.length + ':' + JSON.stringify(result.arguments[0]) + ',' + JSON.stringify(result.arguments[1]));
</script>
</body>)HTML";

    done = false;
    [webView synchronouslyLoadHTMLString:html];
    TestWebKitAPI::Util::run(&done);

    EXPECT_STREQ([alertMessage UTF8String], "2:{\"type\":\"int64_t\",\"value\":8},{\"type\":\"String\",\"value\":\"hello\\u0000\\u0000\\u0000\"}");
}
#endif

TEST(IPCTestingAPI, DecodesReplyArgumentsForPrompt)
{
    auto webView = createWebViewWithIPCTestingAPI();

    RetainPtr delegate = adoptNS([[IPCTestingAPIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];

    done = false;
    promptResult = @"foo";
    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html><script>result = IPC.sendSyncMessage('UI', IPC.webPageProxyID, IPC.messages.WebPageProxy_RunJavaScriptPrompt.name, 100,"
        "[{type: 'FrameID', value: IPC.frameID}, {type: 'FrameInfoData', value: IPC}, {'type': 'String', 'value': 'hi'}, {'type': 'String', 'value': 'bar'}]);</script>"];
    TestWebKitAPI::Util::run(&done);

    EXPECT_STREQ([promptDefault UTF8String], "bar");
    EXPECT_STREQ([[webView stringByEvaluatingJavaScript:@"JSON.stringify(result.arguments)"] UTF8String], "[{\"type\":\"String\",\"value\":\"foo\"}]");
}

TEST(IPCTestingAPI, DecodesReplyArgumentsForAsyncMessage)
{
    auto webView = createWebViewWithIPCTestingAPI();

    RetainPtr delegate = adoptNS([[IPCTestingAPIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];

    done = false;
    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html><script>"
        "let c = IPC.connectionForProcessTarget('Networking');"
        "let cb = (result) => alert(JSON.stringify(result.arguments));"
        "c.sendWithAsyncReply(0, IPC.messages.NetworkConnectionToWebProcess_HasStorageAccess.name,"
        "[{type: 'RegistrableDomain', value: 'https://ipctestingapi.com'}, {type: 'RegistrableDomain', value: 'https://webkit.org'}, {type: 'FrameID', value: IPC.frameID},"
        "{type: 'uint64_t', value: IPC.pageID}], cb);</script>"];
    TestWebKitAPI::Util::run(&done);

    EXPECT_STREQ([alertMessage UTF8String], "[{\"type\":\"bool\",\"value\":false}]");
}

TEST(IPCTestingAPI, EmptyParametersDeleteCookie)
{
    auto webView = createWebViewWithIPCTestingAPI();

    RetainPtr delegate = adoptNS([[IPCTestingAPIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];

    done = false;
    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html><script>"
        "let c = IPC.connectionForProcessTarget('Networking');"
        "let cb = (result) => alert(JSON.stringify(result.arguments));"
        "c.sendWithAsyncReply(0, IPC.messages.NetworkConnectionToWebProcess_DeleteCookie.name,"
        "[{type: 'URL', value: ''},"
        "{type: 'URL', value: 'https://www.url.com'},"
        "{type: 'String', value: 'a=b'}], cb);</script>"];
    TestWebKitAPI::Util::run(&done);

    EXPECT_STREQ([alertMessage UTF8String], "[]");

    done = false;
    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html><script>"
        "let c = IPC.connectionForProcessTarget('Networking');"
        "let cb = (result) => alert(JSON.stringify(result.arguments));"
        "c.sendWithAsyncReply(0, IPC.messages.NetworkConnectionToWebProcess_DeleteCookie.name,"
        "[{type: 'URL', value: 'https://www.firstparty.com'},"
        "{type: 'URL', value: ''},"
        "{type: 'String', value: 'a=b'}], cb);</script>"];
    TestWebKitAPI::Util::run(&done);

    EXPECT_STREQ([alertMessage UTF8String], "[]");

    done = false;
    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html><script>"
        "let c = IPC.connectionForProcessTarget('Networking');"
        "let cb = (result) => alert(JSON.stringify(result.arguments));"
        "c.sendWithAsyncReply(0, IPC.messages.NetworkConnectionToWebProcess_DeleteCookie.name,"
        "[{type: 'URL', value: 'https://www.firstparty.com'},"
        "{type: 'URL', value: 'https://www.url.com'},"
        "{type: 'String', value: ''}], cb);</script>"];
    TestWebKitAPI::Util::run(&done);

    EXPECT_STREQ([alertMessage UTF8String], "[]");
}

TEST(IPCTestingAPI, InvalidURLsDeleteCookie)
{
    auto webView = createWebViewWithIPCTestingAPI();

    RetainPtr delegate = adoptNS([[IPCTestingAPIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];

    done = false;
    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html><script>"
        "let c = IPC.connectionForProcessTarget('Networking');"
        "let cb = (result) => alert(JSON.stringify(result.arguments));"
        "c.sendWithAsyncReply(0, IPC.messages.NetworkConnectionToWebProcess_DeleteCookie.name,"
        "[{type: 'URL', value: 'firstparty.com'},"
        "{type: 'URL', value: 'url.com'},"
        "{type: 'String', value: 'a=b'}], cb);</script>"];
    TestWebKitAPI::Util::run(&done);

    EXPECT_STREQ([alertMessage UTF8String], "[]");
}

TEST(IPCTestingAPI, EmptyFirstPartyForCookiesCookieRequestHeaderFieldValueDigest)
{
    RetainPtr webView = createWebViewWithIPCTestingAPI();
    RetainPtr delegate = adoptNS([[IPCTestingAPIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];

    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html><script>document.cookie='a=b';</script>" baseURL:[NSURL URLWithString:@"https://webkit.org/"]];
    auto sendMessage = @"const connection = IPC.connectionForProcessTarget('Networking');"
        "const result = connection.sendSyncMessage("
        "    0,"
        "    IPC.messages.NetworkConnectionToWebProcess_CookieRequestHeaderFieldValueDigest.name,"
        "    1000,"
        "    ["
        "        {type: 'String', value: null},"
        "        {type: 'uint8_t', value: 1},"
        "        {type: 'uint8_t', value: 1},"
        "        {type: 'uint8_t', value: 1},"
        "        {type: 'URL', value: location.href},"
        "        {type: 'uint8_t', value: 1},"
        "    ]"
        ");";
    [webView evaluateJavaScript:sendMessage completionHandler:nil];
    while (![webView objectByEvaluatingJavaScript:@"result"])
        TestWebKitAPI::Util::spinRunLoop();
    // std::nullopt decodes to undefined.
    EXPECT_STREQ([[webView stringByEvaluatingJavaScript:@"typeof result.arguments[0]"] UTF8String], "undefined");
}

TEST(IPCTestingAPI, InvalidSameSiteInfoCookieRequestHeaderFieldValueDigest)
{
    RetainPtr webView = createWebViewWithIPCTestingAPI();
    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html><script>document.cookie='a=b';</script>" baseURL:[NSURL URLWithString:@"https://webkit.org/"]];
    [webView synchronouslyLoadHTMLString:@"" baseURL:[NSURL URLWithString:@"https://apple.com/"]];
    auto sendMessage = @"const connection = IPC.connectionForProcessTarget('Networking');"
        "const result = connection.sendSyncMessage("
        "    0,"
        "    IPC.messages.NetworkConnectionToWebProcess_CookieRequestHeaderFieldValueDigest.name,"
        "    1000,"
        "    ["
        "        {type: 'URL', value: location.href},"
        "        {type: 'uint8_t', value: 1},"
        "        {type: 'uint8_t', value: 1},"
        "        {type: 'uint8_t', value: 1},"
        "        {type: 'URL', value: 'https://webkit.org'},"
        "        {type: 'uint8_t', value: 1},"
        "    ]"
        ");";
    [webView evaluateJavaScript:sendMessage completionHandler:nil];
    while (![webView objectByEvaluatingJavaScript:@"result"])
        TestWebKitAPI::Util::spinRunLoop();
    // std::nullopt decodes to undefined.
    EXPECT_STREQ([[webView stringByEvaluatingJavaScript:@"typeof result.arguments[0]"] UTF8String], "undefined");
}

TEST(IPCTestingAPI, DescribesArguments)
{
    auto webView = createWebViewWithIPCTestingAPI();

    RetainPtr delegate = adoptNS([[IPCTestingAPIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];

    done = false;
    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html><script>window.args = IPC.messages.WebPageProxy_RunJavaScriptAlert.arguments; alert('ok')</script>"];
    TestWebKitAPI::Util::run(&done);

    EXPECT_STREQ([[webView stringByEvaluatingJavaScript:@"args.length"] UTF8String], "3");
    EXPECT_STREQ([[webView stringByEvaluatingJavaScript:@"args[0].type"] UTF8String], "WebCore::FrameIdentifier");
    EXPECT_STREQ([[webView stringByEvaluatingJavaScript:@"args[1].type"] UTF8String], "WebKit::FrameInfoData");
    EXPECT_STREQ([[webView stringByEvaluatingJavaScript:@"args[2].name"] UTF8String], "message");
    EXPECT_STREQ([[webView stringByEvaluatingJavaScript:@"args[2].type"] UTF8String], "String");
}

TEST(IPCTestingAPI, CanInterceptAlert)
{
    auto webView = createWebViewWithIPCTestingAPI();

    RetainPtr delegate = adoptNS([[IPCTestingAPIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];

    done = false;
    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html><script>messages = []; IPC.addOutgoingMessageListener('UI', (message) => messages.push(message)); alert('ok');</script>"];
    TestWebKitAPI::Util::run(&done);

    EXPECT_STREQ([alertMessage UTF8String], "ok");
    EXPECT_STREQ([webView stringByEvaluatingJavaScript:@"messages = messages.filter((message) => message.name == IPC.messages.WebPageProxy_RunJavaScriptAlert.name); messages.length"].UTF8String, "1");
    EXPECT_STREQ([webView stringByEvaluatingJavaScript:@"messages[0].description"].UTF8String, "WebPageProxy_RunJavaScriptAlert");
    EXPECT_EQ([webView stringByEvaluatingJavaScript:@"args = messages[0].arguments; args.length"].intValue, 3);
    EXPECT_STREQ([webView stringByEvaluatingJavaScript:@"args[0].type"].UTF8String, "uint64_t");
    EXPECT_NE([webView stringByEvaluatingJavaScript:@"args[0].value"].intValue, 0);
    EXPECT_EQ([webView stringByEvaluatingJavaScript:@"args[1] instanceof ArrayBuffer"].boolValue, YES);
    EXPECT_STREQ([webView stringByEvaluatingJavaScript:@"args[2].type"].UTF8String, "String");
    EXPECT_STREQ([webView stringByEvaluatingJavaScript:@"args[2].value"].UTF8String, "ok");
    EXPECT_STREQ([webView stringByEvaluatingJavaScript:@"typeof(messages[0].syncRequestID)"].UTF8String, "number");
    EXPECT_EQ([webView stringByEvaluatingJavaScript:@"messages[0].destinationID"].intValue,
        [webView stringByEvaluatingJavaScript:@"IPC.webPageProxyID.toString()"].intValue);
}

TEST(IPCTestingAPI, CanInterceptHasStorageAccess)
{
    auto webView = createWebViewWithIPCTestingAPI();

    RetainPtr delegate = adoptNS([[IPCTestingAPIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];

    done = false;
    promptResult = @"foo";
    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html><script>let targetMessage = {}; const messageName = IPC.messages.NetworkConnectionToWebProcess_HasStorageAccess.name;"
        "IPC.addOutgoingMessageListener('Networking', (currentMessage) => { if (currentMessage.name == messageName) targetMessage = currentMessage; });"
        "let c = IPC.connectionForProcessTarget('Networking');"
        "let cb = (result) => alert(JSON.stringify(result.arguments));"
        "c.sendWithAsyncReply(0, messageName, [{type: 'RegistrableDomain', value: 'https://ipctestingapi.com'}, {type: 'RegistrableDomain', value: 'https://webkit.org'},"
        "{type: 'FrameID', value: IPC.frameID}, {type: 'uint64_t', value: IPC.pageID}], cb);</script>"];
    TestWebKitAPI::Util::run(&done);

    EXPECT_STREQ([alertMessage UTF8String], "[{\"type\":\"bool\",\"value\":false}]");
    EXPECT_STREQ([webView stringByEvaluatingJavaScript:@"targetMessage.description"].UTF8String, "NetworkConnectionToWebProcess_HasStorageAccess");
    EXPECT_EQ([webView stringByEvaluatingJavaScript:@"targetMessage.arguments.length"].intValue, 4);
    EXPECT_STREQ([webView stringByEvaluatingJavaScript:@"targetMessage.arguments[0].type"].UTF8String, "RegistrableDomain");
    EXPECT_STREQ([webView stringByEvaluatingJavaScript:@"targetMessage.arguments[0].value"].UTF8String, "ipctestingapi.com");
    EXPECT_STREQ([webView stringByEvaluatingJavaScript:@"targetMessage.arguments[1].type"].UTF8String, "RegistrableDomain");
    EXPECT_STREQ([webView stringByEvaluatingJavaScript:@"targetMessage.arguments[1].value"].UTF8String, "webkit.org");
    EXPECT_STREQ([webView stringByEvaluatingJavaScript:@"targetMessage.arguments[2].type"].UTF8String, "uint64_t");
    EXPECT_NE([webView stringByEvaluatingJavaScript:@"targetMessage.arguments[2].value"].intValue, 0);
    EXPECT_STREQ([webView stringByEvaluatingJavaScript:@"targetMessage.arguments[3].type"].UTF8String, "uint64_t");
    EXPECT_EQ([webView stringByEvaluatingJavaScript:@"targetMessage.arguments[3].value"].intValue, [webView stringByEvaluatingJavaScript:@"IPC.pageID.toString()"].intValue);
    EXPECT_STREQ([webView stringByEvaluatingJavaScript:@"typeof(targetMessage.syncRequestID)"].UTF8String, "undefined");
    EXPECT_EQ([webView stringByEvaluatingJavaScript:@"targetMessage.destinationID"].intValue, 0);
}

TEST(IPCTestingAPI, CanInterceptFindString)
{
    auto webView = createWebViewWithIPCTestingAPI();

    RetainPtr delegate = adoptNS([[IPCTestingAPIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];

    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html><body><p>hello</p><script>messages = []; IPC.addIncomingMessageListener('UI', (message) => messages.push(message));</script>"];

    done = false;
    RetainPtr findConfiguration = adoptNS([[WKFindConfiguration alloc] init]);
    [webView findString:@"hello" withConfiguration:findConfiguration.get() completionHandler:^(WKFindResult *result) {
        EXPECT_TRUE(result.matchFound);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:0 endOffset:5]);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    EXPECT_STREQ([webView stringByEvaluatingJavaScript:@"messages = messages.filter((message) => message.name == IPC.messages.WebPage_FindString.name); messages.length"].UTF8String, "1");
    EXPECT_STREQ([webView stringByEvaluatingJavaScript:@"messages[0].description"].UTF8String, "WebPage_FindString");
    EXPECT_EQ([webView stringByEvaluatingJavaScript:@"args = messages[0].arguments; args.length"].intValue, 3);
    EXPECT_STREQ([webView stringByEvaluatingJavaScript:@"args[0].type"].UTF8String, "String");
    EXPECT_STREQ([webView stringByEvaluatingJavaScript:@"args[0].value"].UTF8String, "hello");
    EXPECT_STREQ([webView stringByEvaluatingJavaScript:@"args[1].type"].UTF8String, "uint16_t");
    EXPECT_EQ([webView stringByEvaluatingJavaScript:@"args[1].value"].intValue, 0x11);
    EXPECT_EQ([webView stringByEvaluatingJavaScript:@"args[1].isOptionSet"].boolValue, YES);
    EXPECT_STREQ([webView stringByEvaluatingJavaScript:@"args[2].type"].UTF8String, "uint32_t");
    EXPECT_EQ([webView stringByEvaluatingJavaScript:@"args[2].value"].intValue, 1);

    EXPECT_STREQ([webView stringByEvaluatingJavaScript:@"typeof(messages[0].syncRequestID)"].UTF8String, "undefined");
    EXPECT_EQ([webView stringByEvaluatingJavaScript:@"messages[0].destinationID"].intValue,
        [webView stringByEvaluatingJavaScript:@"IPC.webPageProxyID.toString()"].intValue);
}

TEST(IPCTestingAPI, SerializedTypeInfo)
{
    auto webView = createWebViewWithIPCTestingAPI();
    NSDictionary *typeInfo = [webView objectByEvaluatingJavaScript:@"IPC.serializedTypeInfo"];
    NSArray *expectedArray = @[@{
        @"name": @"ignoreSearch",
        @"type": @"bool"
    }, @{
        @"name": @"ignoreMethod",
        @"type": @"bool"
    }, @{
        @"name": @"ignoreVary",
        @"type": @"bool"
    }];
    EXPECT_TRUE([typeInfo[@"WebCore::CacheQueryOptions"] isEqualToArray:expectedArray]);

    NSDictionary *expectedDictionary = @{
        @"isOptionSet" : @1,
        @"size" : @1,
        @"validValues" : @[@1, @2, @4],
        @"valueMap" : @[@{@"value": @1, @"name": @"ComputeSizes"}, @{@"value": @2, @"name": @"DoNotCreateProcesses"}, @{@"value": @4, @"name": @"IncludeAllOrigins"}]
    };
    NSDictionary *enumInfo = [webView objectByEvaluatingJavaScript:@"IPC.serializedEnumInfo"];
    EXPECT_TRUE([enumInfo[@"WebKit::WebsiteDataFetchOption"] isEqualToDictionary:expectedDictionary]);
    NSDictionary *expectedMouseEventButtonDictionary = @{
        @"isOptionSet" : @NO,
        @"size" : @1,
        @"validValues" : @[@0, @1, @2, @3, @4, @254],
        @"valueMap" : @[@{@"value": @0, @"name": @"Left"}, @{@"value": @1, @"name": @"Middle"}, @{@"value": @2, @"name": @"Right"}, @{@"value": @3, @"name": @"Back"}, @{@"value": @4, @"name": @"Forward"}, @{@"value": @254, @"name": @"None"}]
    };
    EXPECT_TRUE([enumInfo[@"WebKit::WebMouseEventButton"] isEqualToDictionary:expectedMouseEventButtonDictionary]);

    NSArray *objectIdentifiers = [webView objectByEvaluatingJavaScript:@"IPC.objectIdentifiers"];
    EXPECT_TRUE([objectIdentifiers containsObject:@"WebCore::PageIdentifier"]);
}

TEST(IPCTestingAPI, LockdownModeDisablesWebGL)
{
    auto webView = createWebViewWithIPCTestingAPIAndLockdownMode(true);

    RetainPtr delegate = adoptNS([[IPCTestingAPIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];

    done = false;
    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html><script>"
        "const canvas = document.createElement('canvas');"
        "const gl = canvas.getContext('webgl');"
        "alert(gl === null ? 'webgl_disabled' : 'webgl_enabled');"
        "</script>"];
    TestWebKitAPI::Util::run(&done);

    EXPECT_TRUE([alertMessage isEqualToString:@"webgl_disabled"]);

    [WKProcessPool _clearCaptivePortalModeEnabledGloballyForTesting];
}

TEST(IPCTestingAPI, LockdownModeDisabledAllowsWebGL)
{
    auto webView = createWebViewWithIPCTestingAPIAndLockdownMode(false);

    RetainPtr delegate = adoptNS([[IPCTestingAPIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];

    done = false;
    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html><script>"
        "const canvas = document.createElement('canvas');"
        "const gl = canvas.getContext('webgl');"
        "alert(gl !== null ? 'webgl_enabled' : 'webgl_disabled');"
        "</script>"];
    TestWebKitAPI::Util::run(&done);

    EXPECT_TRUE([alertMessage isEqualToString:@"webgl_enabled"]);
}

TEST(IPCTestingAPI, LockdownModeDetection)
{
    // Test with lockdown mode enabled
    {
        auto webViewLockdown = createWebViewWithIPCTestingAPIAndLockdownMode(true);
        RetainPtr delegate = adoptNS([[IPCTestingAPIDelegate alloc] init]);
        [webViewLockdown setUIDelegate:delegate.get()];

        [webViewLockdown synchronouslyLoadHTMLString:@"<!DOCTYPE html><html><body>Test</body></html>"];

        NSString *webglResult = [webViewLockdown stringByEvaluatingJavaScript:@"(function() { try { const canvas = document.createElement('canvas'); return canvas.getContext('webgl') === null; } catch(e) { return true; } })()"];
        NSLog(@"WebGL disabled in lockdown mode: %@", webglResult);

        NSString *webgpuResult = [webViewLockdown stringByEvaluatingJavaScript:@"(function() { try { return typeof navigator.gpu === 'undefined'; } catch(e) { return true; } })()"];
        NSLog(@"WebGPU disabled in lockdown mode: %@", webgpuResult);

        NSString *speechResult = [webViewLockdown stringByEvaluatingJavaScript:@"(function() { try { return typeof webkitSpeechRecognition === 'undefined' && typeof SpeechRecognition === 'undefined'; } catch(e) { return true; } })()"];
        NSLog(@"Speech Recognition disabled in lockdown mode: %@", speechResult);

        NSString *disabledCountResult = [webViewLockdown stringByEvaluatingJavaScript:@"(function() { "
            "let count = 0; "
            "try { const canvas = document.createElement('canvas'); if (canvas.getContext('webgl') === null) count++; } catch(e) { count++; } "
            "try { if (typeof navigator.gpu === 'undefined') count++; } catch(e) { count++; } "
            "try { if (typeof webkitSpeechRecognition === 'undefined' && typeof SpeechRecognition === 'undefined') count++; } catch(e) { count++; } "
            "return count; "
            "})()"];

        int disabledCount = [disabledCountResult intValue];
        NSLog(@"Total disabled APIs in lockdown mode: %d", disabledCount);

        EXPECT_GT(disabledCount, 0);
    }

    // Test with lockdown mode disabled
    {
        auto webViewNormal = createWebViewWithIPCTestingAPIAndLockdownMode(false);
        RetainPtr delegate = adoptNS([[IPCTestingAPIDelegate alloc] init]);
        [webViewNormal setUIDelegate:delegate.get()];

        [webViewNormal synchronouslyLoadHTMLString:@"<!DOCTYPE html><html><body>Test</body></html>"];

        NSLog(@"Testing normal mode - checking API availability directly...");

        NSString *webglResult = [webViewNormal stringByEvaluatingJavaScript:@"(function() { try { const canvas = document.createElement('canvas'); return canvas.getContext('webgl') !== null; } catch(e) { return false; } })()"];
        NSLog(@"WebGL available in normal mode: %@", webglResult);

        NSString *webgpuResult = [webViewNormal stringByEvaluatingJavaScript:@"(function() { try { return typeof navigator.gpu !== 'undefined'; } catch(e) { return false; } })()"];
        NSLog(@"WebGPU available in normal mode: %@", webgpuResult);

        NSString *speechResult = [webViewNormal stringByEvaluatingJavaScript:@"(function() { try { return typeof webkitSpeechRecognition !== 'undefined' || typeof SpeechRecognition !== 'undefined'; } catch(e) { return false; } })()"];
        NSLog(@"Speech Recognition available in normal mode: %@", speechResult);

        NSString *availableCountResult = [webViewNormal stringByEvaluatingJavaScript:@"(function() { "
            "let count = 0; "
            "try { const canvas = document.createElement('canvas'); if (canvas.getContext('webgl') !== null) count++; } catch(e) { } "
            "try { if (typeof navigator.gpu !== 'undefined') count++; } catch(e) { } "
            "try { if (typeof webkitSpeechRecognition !== 'undefined' || typeof SpeechRecognition !== 'undefined') count++; } catch(e) { } "
            "return count; "
            "})()"];

        int availableCount = [availableCountResult intValue];
        NSLog(@"Total available APIs in normal mode: %d", availableCount);

        NSLog(@"Normal mode API availability check completed (count: %d)", availableCount);
    }

    [WKProcessPool _setCaptivePortalModeEnabledGloballyForTesting:NO];
}

TEST(IPCTestingAPI, SpeechSynthesisWithFeatureFlag)
{
    // Test 1: Feature flag enabled - message should succeed
    {
        RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
        for (_WKFeature *feature in [WKPreferences _features]) {
            if ([feature.key isEqualToString:@"IPCTestingAPIEnabled"])
                [[configuration preferences] _setEnabled:YES forFeature:feature];
            if ([feature.key isEqualToString:@"SpeechSynthesisAPIEnabled"])
                [[configuration preferences] _setEnabled:YES forFeature:feature];
        }
        RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

        RetainPtr delegate = adoptNS([[IPCTestingAPIDelegate alloc] init]);
        [webView setUIDelegate:delegate.get()];

        NSURL *htmlURL = [NSBundle.test_resourcesBundle URLForResource:@"speechsynthesis_feature_test" withExtension:@"html"];
        [webView loadRequest:[NSURLRequest requestWithURL:htmlURL]];

        done = false;
        TestWebKitAPI::Util::runFor(&done, 10_s);

        NSLog(@"SpeechSynthesis feature test (enabled) result: %@", alertMessage.get());

        EXPECT_TRUE(alertMessage.get() != nil);
        EXPECT_TRUE([alertMessage containsString:@"speechsynthesis_message_sent_successfully"]);
    }

    // Test 2: Feature flag disabled - message should fail with cancel error
    {
        RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
        for (_WKFeature *feature in [WKPreferences _features]) {
            if ([feature.key isEqualToString:@"IPCTestingAPIEnabled"])
                [[configuration preferences] _setEnabled:YES forFeature:feature];
            if ([feature.key isEqualToString:@"SpeechSynthesisAPIEnabled"])
                [[configuration preferences] _setEnabled:NO forFeature:feature];
        }
        RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

        RetainPtr delegate = adoptNS([[IPCTestingAPIDelegate alloc] init]);
        [webView setUIDelegate:delegate.get()];

        NSURL *htmlURL = [NSBundle.test_resourcesBundle URLForResource:@"speechsynthesis_feature_test" withExtension:@"html"];
        [webView loadRequest:[NSURLRequest requestWithURL:htmlURL]];

        done = false;
        TestWebKitAPI::Util::runFor(&done, 10_s);

        NSLog(@"SpeechSynthesis feature test (disabled) result: %@", alertMessage.get());

        EXPECT_TRUE(alertMessage.get() != nil);
        EXPECT_TRUE([alertMessage containsString:@"speechsynthesis_enabledby_blocked"]
            && [alertMessage containsString:@"Receiver cancelled the reply due to invalid destination or deserialization error"]);
    }
}

TEST(IPCTestingAPI, SpeechSynthesisWithLockdownMode)
{
    [WKProcessPool _setCaptivePortalModeEnabledGloballyForTesting:YES];

    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    for (_WKFeature *feature in [WKPreferences _features]) {
        if ([feature.key isEqualToString:@"IPCTestingAPIEnabled"])
            [[configuration preferences] _setEnabled:YES forFeature:feature];
        if ([feature.key isEqualToString:@"SpeechSynthesisAPIEnabled"]) {
            // Even with feature enabled, lockdown mode should disable it
            [[configuration preferences] _setEnabled:YES forFeature:feature];
        }
    }

    RetainPtr<WKWebpagePreferences> webpagePreferences = adoptNS([[WKWebpagePreferences alloc] init]);
    [webpagePreferences setLockdownModeEnabled:YES];
    [configuration setDefaultWebpagePreferences:webpagePreferences.get()];

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    RetainPtr delegate = adoptNS([[IPCTestingAPIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];

    NSURL *htmlURL = [NSBundle.test_resourcesBundle URLForResource:@"speechsynthesis_lockdown_test" withExtension:@"html"];
    [webView loadRequest:[NSURLRequest requestWithURL:htmlURL]];

    done = false;
    TestWebKitAPI::Util::runFor(&done, 10_s);

    NSLog(@"SpeechSynthesis lockdown test result: %@", alertMessage.get());

    EXPECT_TRUE(alertMessage.get() != nil);
    EXPECT_TRUE([alertMessage containsString:@"speechsynthesis_lockdown_correctly_blocked"]
        && [alertMessage containsString:@"Receiver cancelled the reply due to invalid destination or deserialization error"]);

    [WKProcessPool _setCaptivePortalModeEnabledGloballyForTesting:NO];
}

static RetainPtr<NSString> sendOriginAccessAllowListEntryAndFetchCrossOrigin(bool allowOriginAccessAllowListIPC)
{
    using namespace TestWebKitAPI;

    HTTPServer server({
        { "/pageA"_s, { "<!DOCTYPE html>"_s } },
        { "/pageB"_s, { "<!DOCTYPE html>"_s } },
        { "/target"_s, { {{ "Content-Type"_s, "text/plain"_s }}, "cross-origin-data"_s } },
    });
    auto serverPort = server.port();

    RetainPtr configA = adoptNS([[WKWebViewConfiguration alloc] init]);
    for (_WKFeature *feature in [WKPreferences _features]) {
        if ([feature.key isEqualToString:@"IPCTestingAPIEnabled"])
            [[configA preferences] _setEnabled:YES forFeature:feature];
        if ([feature.key isEqualToString:@"AllowTestOnlyOriginAccessAllowListIPC"])
            [[configA preferences] _setEnabled:allowOriginAccessAllowListIPC forFeature:feature];
    }

    RetainPtr configB = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configB setProcessPool:[configA processPool]];

    RetainPtr webViewA = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configA.get()]);
    RetainPtr webViewB = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configB.get()]);

    [webViewA loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:[NSString stringWithFormat:@"http://127.0.0.1:%u/pageA", serverPort]]]];
    [webViewA _test_waitForDidFinishNavigation];

    [webViewB loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:[NSString stringWithFormat:@"http://localhost:%u/pageB", serverPort]]]];
    [webViewB _test_waitForDidFinishNavigation];

    [webViewA stringByEvaluatingJavaScript:[NSString stringWithFormat:
        @"IPC.sendMessage('Networking', 0,"
        "  IPC.messages.NetworkConnectionToWebProcess_AddOriginAccessAllowListEntry.name,"
        "  ["
        "    { type: 'String', value: 'http://localhost:%u' },"
        "    { type: 'String', value: 'http' },"
        "    { type: 'String', value: '127.0.0.1' },"
        "    { type: 'bool', value: 1 }"
        "  ]"
        ")", serverPort]];

    Util::runFor(0.5_s);

    [webViewB evaluateJavaScript:[NSString stringWithFormat:
        @"try {"
        "  var xhr = new XMLHttpRequest();"
        "  xhr.open('GET', 'http://127.0.0.1:%u/target', false);"
        "  xhr.send();"
        "  alert('FETCHED:' + xhr.responseText);"
        "} catch(e) {"
        "  alert('BLOCKED:' + e);"
        "}", serverPort] completionHandler:nil];

    return [webViewB _test_waitForAlert];
}

TEST(IPCTestingAPI, AddOriginAccessAllowListEntryRequiresTestOnlyIPC)
{
    auto result = sendOriginAccessAllowListEntryAndFetchCrossOrigin(false);
    EXPECT_TRUE([result hasPrefix:@"BLOCKED:"]);
}

TEST(IPCTestingAPI, AddOriginAccessAllowListEntryAllowedWithTestOnlyIPC)
{
    auto result = sendOriginAccessAllowListEntryAndFetchCrossOrigin(true);
    EXPECT_WK_STREQ(result, "FETCHED:cross-origin-data");
}

#if ENABLE(CONTENT_FILTERING)

static NSString *installMockContentFilterAndNavigateVictim(bool allowMockContentFilterIPC)
{
    using namespace TestWebKitAPI;

    HTTPServer attackerServer({
        { "/attacker"_s, { "<!DOCTYPE html>"_s } },
        { "/evil"_s, { "<!DOCTYPE html><body>REDIRECTED</body>"_s } },
    });
    HTTPServer victimServer({
        { "/victim"_s, { "<!DOCTYPE html><body>ORIGINAL</body>"_s } },
    });

    RetainPtr configA = adoptNS([[WKWebViewConfiguration alloc] init]);
    for (_WKFeature *feature in [WKPreferences _features]) {
        if ([feature.key isEqualToString:@"IPCTestingAPIEnabled"])
            [[configA preferences] _setEnabled:YES forFeature:feature];
        if ([feature.key isEqualToString:@"AllowTestOnlyMockContentFilterIPC"])
            [[configA preferences] _setEnabled:allowMockContentFilterIPC forFeature:feature];
    }

    RetainPtr configB = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configB setProcessPool:[configA processPool]];

    RetainPtr webViewA = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configA.get()]);
    RetainPtr webViewB = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configB.get()]);

    [webViewA loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:[NSString stringWithFormat:@"http://127.0.0.1:%u/attacker", attackerServer.port()]]]];
    [webViewA _test_waitForDidFinishNavigation];

    [webViewA stringByEvaluatingJavaScript:[NSString stringWithFormat:
        @"IPC.sendMessage('Networking', 0,"
        "  IPC.messages.NetworkConnectionToWebProcess_InstallMockContentFilter.name,"
        "  ["
        "    { type: 'bool', value: 1 },"
        "    { type: 'uint8_t', value: 0 },"
        "    { type: 'bool', value: 0 },"
        "    { type: 'bool', value: 1 },"
        "    { type: 'String', value: '' },"
        "    { type: 'String', value: 'http://127.0.0.1:%u/evil' },"
        "    { type: 'double', value: 0 }"
        "  ]"
        ")", attackerServer.port()]];

    Util::runFor(0.5_s);

    [webViewB loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:[NSString stringWithFormat:@"http://127.0.0.1:%u/victim", victimServer.port()]]]];
    [webViewB _test_waitForDidFinishNavigation];

    NSString *bodyText = [webViewB stringByEvaluatingJavaScript:@"document.body.innerText"];

    if (allowMockContentFilterIPC) {
        // Reset MockContentFilterSettings since it is a process-global singleton in the NetworkProcess.
        [webViewA stringByEvaluatingJavaScript:
            @"IPC.sendMessage('Networking', 0,"
            "  IPC.messages.NetworkConnectionToWebProcess_InstallMockContentFilter.name,"
            "  ["
            "    { type: 'bool', value: 0 },"
            "    { type: 'uint8_t', value: 0 },"
            "    { type: 'bool', value: 0 },"
            "    { type: 'bool', value: 0 },"
            "    { type: 'String', value: '' },"
            "    { type: 'String', value: '' },"
            "    { type: 'double', value: 0 }"
            "  ]"
            ")"];

        Util::runFor(0.5_s);
    }

    return bodyText;
}

TEST(IPCTestingAPI, InstallMockContentFilterRequiresTestOnlyIPC)
{
    EXPECT_WK_STREQ(installMockContentFilterAndNavigateVictim(false), "ORIGINAL");
}

TEST(IPCTestingAPI, InstallMockContentFilterRedirectsWithTestOnlyIPC)
{
    EXPECT_WK_STREQ(installMockContentFilterAndNavigateVictim(true), "REDIRECTED");
}

#endif // ENABLE(CONTENT_FILTERING)

static constexpr auto fileSystemGoodPageHTML = R"TESTRESOURCE(
<script>
var capturedIdentifier = null;
IPC.addOutgoingMessageListener('Networking', function(msg) {
    if (msg.name === IPC.messages.NetworkStorageManager_GetHandleNames.name && !capturedIdentifier) {
        var buf = new DataView(msg.buffer);
        capturedIdentifier = buf.getBigUint64(16, true);
    }
});

var run = async() => {
    var root = await navigator.storage.getDirectory();
    await root.getFileHandle('test.txt', { create: true });
    for await (var entry of root.entries()) { }
    if (capturedIdentifier !== null)
        alert('id:' + capturedIdentifier.toString());
    else
        alert('error:no-identifier-captured');
};
run();
</script>
)TESTRESOURCE"_s;

static constexpr auto fileSystemBadPageHTML = R"TESTRESOURCE(
<script>
var attack = (stolenId) => {
    var net = IPC.connectionForProcessTarget('Networking');
    var onReply = (reply) => {
        var buf = new DataView(reply.buffer);
        var hasValue = !!buf.getUint8(16);
        if (hasValue)
            alert('FAIL:access-granted');
        else
            alert('PASS:access-denied');
    };
    net.sendWithAsyncReply(0, IPC.messages.NetworkStorageManager_GetHandleNames.name, [ { type: 'uint64_t', value: BigInt(stolenId) } ], onReply);
};
</script>
)TESTRESOURCE"_s;

TEST(IPCTestingAPI, FileSystemForgedHandleIdentifierRejected)
{
    using namespace TestWebKitAPI;

    RetainPtr tempDir = retainPtr([[NSFileManager defaultManager] URLForDirectory:NSItemReplacementDirectory inDomain:NSUserDomainMask appropriateForURL:[NSURL fileURLWithPath:NSTemporaryDirectory()] create:YES error:nil]);
    RetainPtr dataStoreConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] init]);
    [dataStoreConfiguration setGeneralStorageDirectory:[tempDir URLByAppendingPathComponent:@"Storage"]];
    RetainPtr dataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:dataStoreConfiguration.get()]);
    [dataStore _setStorageSiteValidationEnabled:YES];

    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    for (_WKFeature *feature in [WKPreferences _features]) {
        if ([feature.key isEqualToString:@"IPCTestingAPIEnabled"]) {
            [[configuration preferences] _setEnabled:YES forFeature:feature];
            break;
        }
    }
    [configuration setWebsiteDataStore:dataStore.get()];

    RetainPtr goodUIDelegate = adoptNS([TestUIDelegate new]);
    RetainPtr goodView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [goodView setUIDelegate:goodUIDelegate.get()];
    [goodView synchronouslyLoadHTMLString:[NSString stringWithUTF8String:fileSystemGoodPageHTML.characters()] baseURL:[NSURL URLWithString:@"https://good.example/"]];

    NSString *goodMessage = [goodUIDelegate waitForAlert];
    EXPECT_TRUE([goodMessage hasPrefix:@"id:"]);
    NSString *stolenIdentifier = [goodMessage substringFromIndex:3];

    auto goodPID = [goodView _webProcessIdentifier];

    RetainPtr badUIDelegate = adoptNS([TestUIDelegate new]);
    RetainPtr badView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [badView setUIDelegate:badUIDelegate.get()];
    [badView synchronouslyLoadHTMLString:[NSString stringWithUTF8String:fileSystemBadPageHTML.characters()] baseURL:[NSURL URLWithString:@"https://bad.example/"]];

    auto badPID = [badView _webProcessIdentifier];
    EXPECT_NE(goodPID, badPID);

    [badView evaluateJavaScript:[NSString stringWithFormat:@"attack('%@')", stolenIdentifier] completionHandler:nil];
    EXPECT_WK_STREQ(@"PASS:access-denied", [badUIDelegate waitForAlert]);
}

// A compromised https://siteb.example iframe inside a https://sitea.example page forges cookie IPC naming
// firstParty=https://sitea.example/main and url=https://sitea.example/cookie-target, which used to return
// sitea's cookies including the HttpOnly one. Every read below must now come back empty.
//
// IgnoreInvalidMessageWhenIPCTestingAPIEnabled is deliberately not enabled, so a denial that regressed into a
// MESSAGE_CHECK terminates the sender and is caught by the liveness assertions rather than swallowed.

// A terminated web process never alerts, so -_test_waitForAlert's unbounded wait would hang instead of
// failing. Same shape as SiteIsolation.mm's BoundedAlertRecorder, renamed because both files link into one
// binary. Installed before the first load so an early alert is not missed; each wait consumes one message.
// The bound is much longer than the 10s timeout each page arms, so the page's own diagnostic wins.
@interface IPCTestingAPIAlertRecorder : NSObject <WKUIDelegate>
- (NSString *)waitForAlert;
@end

@implementation IPCTestingAPIAlertRecorder {
    RetainPtr<NSString> _message;
}

- (void)webView:(WKWebView *)webView runJavaScriptAlertPanelWithMessage:(NSString *)message initiatedByFrame:(WKFrameInfo *)frame completionHandler:(void (^)(void))completionHandler
{
    _message = message;
    completionHandler();
}

- (NSString *)waitForAlert
{
    EXPECT_TRUE(TestWebKitAPI::Util::waitFor([&] {
        return !!_message;
    }, 300));
    RetainPtr<NSString> message = _message;
    _message = nullptr;
    return message.autorelease();
}

@end

static RetainPtr<TestWKWebView> createSiteIsolatedIPCTestWebView(TestWebKitAPI::HTTPServer& server, RetainPtr<TestNavigationDelegate>& navigationDelegate, RetainPtr<IPCTestingAPIAlertRecorder>& alertRecorder)
{
    RetainPtr configuration = server.httpsProxyConfiguration();
    for (_WKFeature *feature in [WKPreferences _features]) {
        if ([feature.key isEqualToString:@"IPCTestingAPIEnabled"] || [feature.key isEqualToString:@"SiteIsolationEnabled"] || [feature.key isEqualToString:@"CookieStoreAPIEnabled"])
            [[configuration preferences] _setEnabled:YES forFeature:feature];
    }

    // On the iOS family, where USE(ITP_TCC_CHECK) makes tracking prevention default to on, a cross-site read
    // would otherwise come back empty for a reason unrelated to the check under test.
    [[configuration websiteDataStore] _setResourceLoadStatisticsEnabled:NO];

    navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    webView.get().navigationDelegate = navigationDelegate.get();
    alertRecorder = adoptNS([IPCTestingAPIAlertRecorder new]);
    webView.get().UIDelegate = alertRecorder.get();
    return webView;
}

// Reads the cookie store rather than document.cookie: it also sees HttpOnly cookies and cannot be masked by
// WebCookieCache. Returns the empty string when absent, for a readable EXPECT_WK_STREQ diff.
static NSString *cookieValueInStore(WKWebsiteDataStore *dataStore, NSString *name)
{
    __block RetainPtr<NSArray<NSHTTPCookie *>> cookies;
    __block bool gotCookies = false;
    [dataStore.httpCookieStore getAllCookies:^(NSArray<NSHTTPCookie *> *allCookies) {
        cookies = allCookies;
        gotCookies = true;
    }];
    TestWebKitAPI::Util::run(&gotCookies);
    for (NSHTTPCookie *cookie in cookies.get()) {
        if ([cookie.name isEqualToString:name])
            return cookie.value;
    }
    return @"";
}

// Every page below is served as these shared helpers followed by its own script. The reporter's proof
// defined its helpers once per page; keeping exactly one copy is what keeps the pages honest about being
// the same proof, and is why the comments explaining each helper only have to be right once.
static constexpr auto siteIsolationCookieIPCHelperBytes = R"TESTRESOURCE(
<!DOCTYPE html>
<script>
function currentFrameIDString()
{
    return (Array.isArray(IPC.frameID) ? IPC.frameID[0] : IPC.frameID).toString();
}

function frameIDArg(frameID)
{
    return { type: 'FrameID', value: [ BigInt(frameID) ] };
}

// The engaged byte before every std::optional is encoded explicitly, exactly as the reporter encoded it.
// Without it the message would be rejected as undecodable and its test would pass for the wrong reason.
function optionalFrameIDArgs(frameID)
{
    return [ { type: 'uint8_t', value: 1 }, frameIDArg(frameID) ];
}

function optionalPageIDArgs(pageID)
{
    return [ { type: 'uint8_t', value: 1 }, { type: 'uint64_t', value: BigInt(pageID) } ];
}

function optionalWebPageProxyIDArgs(webPageProxyID)
{
    return [ { type: 'uint8_t', value: 1 }, { type: 'uint64_t', value: BigInt(webPageProxyID) } ];
}

function sameSiteInfoArgs()
{
    return [
        { type: 'uint8_t', value: 1 }, // isSameSite
        { type: 'uint8_t', value: 1 }, // isTopSite
        { type: 'uint8_t', value: 1 } // isSafeHTTPMethod
    ];
}

function sendSync(connection, message, args)
{
    try {
        return connection.sendSyncMessage(0, message, 10000, args);
    } catch (e) {
        return { exception: String(e) };
    }
}

function sendAsync(connection, message, args)
{
    return new Promise(resolve => {
        try {
            connection.sendWithAsyncReply(0, message, args, result => resolve(result));
        } catch (e) {
            resolve({ exception: String(e) });
        }
    });
}

// A decoded reply has an arguments array; one that threw, timed out or failed to decode has none. Assertions
// about a reply's contents need this beside them, or "no reply" looks like "a reply with nothing in it".
// A failed send yields a bare undefined, so reply arguments must be read through replyArgument() rather than
// result.arguments[i]: reading a property off that undefined throws inside the async listener, and the page
// then never posts its payload at all, turning an informative failure into a bare timeout.
function decodedReply(result)
{
    return Array.isArray(result && result.arguments);
}

function replyArgument(result, index)
{
    return result && result.arguments ? result.arguments[index] : undefined;
}

function replyArgumentCount(result)
{
    return decodedReply(result) ? result.arguments.length : -1;
}

// Reply arguments are { type, value } objects, so an argument that is not there at all reads as
// '<missing>' rather than as a value.
function stringValue(result, index)
{
    return result && result.arguments && result.arguments[index] ? result.arguments[index].value : '<missing>';
}

// A denied read replies with a null String, which the IPC testing API decodes as JS null rather than as a
// string, so the reporter's stringValue() cannot be used directly for the cookie strings any more.
function cookieStringValue(result, index)
{
    const value = stringValue(result, index);
    return value === null || value === undefined ? '<null>' : String(value);
}

function safeJSONString(value)
{
    return JSON.stringify(value, (_, item) => typeof item === 'bigint' ? item.toString() : item);
}

// The IPC testing API has no JS representation for Vector<WebCore::Cookie>, so it hands back that argument's
// raw bytes as an ArrayBuffer. A disengaged std::optional and a missing argument both answer null here.
function bytesForDecodedArgument(value)
{
    if (!value)
        return null;
    if (value instanceof ArrayBuffer)
        return new Uint8Array(value);
    if (ArrayBuffer.isView(value))
        return new Uint8Array(value.buffer, value.byteOffset, value.byteLength);
    if (value.value instanceof ArrayBuffer)
        return new Uint8Array(value.value);
    return null;
}

function containsBytes(haystack, needle)
{
    if (!haystack)
        return false;
    outer:
    for (let i = 0; i + needle.length <= haystack.length; ++i) {
        for (let j = 0; j < needle.length; ++j) {
            if (haystack[i + j] !== needle[j])
                continue outer;
        }
        return true;
    }
    return false;
}

function utf16Bytes(text, littleEndian)
{
    const result = [];
    for (let i = 0; i < text.length; ++i) {
        const code = text.charCodeAt(i);
        if (littleEndian)
            result.push(code & 0xff, code >> 8);
        else
            result.push(code >> 8, code & 0xff);
    }
    return new Uint8Array(result);
}

function decodedArgumentContains(value, text)
{
    const bytes = bytesForDecodedArgument(value);
    if (!bytes)
        return false;
    const ascii = new TextEncoder().encode(text);
    return containsBytes(bytes, ascii)
        || containsBytes(bytes, utf16Bytes(text, true))
        || containsBytes(bytes, utf16Bytes(text, false));
}

function decodedArgumentByteLength(value)
{
    const bytes = bytesForDecodedArgument(value);
    return bytes ? bytes.byteLength : -1;
}

// A denied read replies with a null String, which the IPC testing API surfaces as '<null>', and an allowed
// read that finds nothing replies with an empty string. '<missing>' is deliberately not accepted: that
// means no such reply argument was decoded, which is a broken message rather than a denial.
function isEmptyCookieString(value)
{
    return value === '' || value === '<null>';
}

// The reply is std::optional<std::array<uint8_t, 20>>. std::array has no JS conversion, so an engaged optional
// arrives as an ArrayBuffer of the consumed bytes - the engaged byte plus 20 digest bytes - and a disengaged
// one as undefined. Anything but 21 bytes means the send failed rather than being denied. The salt is new on
// every network process launch, so only the digest's presence and its stability across a write can be asserted.
function cookieHeaderDigestBytes(result)
{
    const bytes = bytesForDecodedArgument(result && result.arguments && result.arguments[0]);
    return bytes && bytes.byteLength === 21 ? bytes.subarray(1) : null;
}

// False for a null operand, so a pair of failed reads cannot make a "the write did not land" assertion pass
// by comparing nothing to nothing.
function cookieHeaderDigestsEqual(a, b)
{
    if (!a || !b || a.byteLength !== b.byteLength)
        return false;
    for (let i = 0; i < a.byteLength; ++i) {
        if (a[i] !== b[i])
            return false;
    }
    return true;
}

function cookieHeaderDigestHex(bytes)
{
    return bytes ? Array.from(bytes, (byte) => byte.toString(16).padStart(2, '0')).join('') : '<no digest>';
}

// This message returns only a salted digest of the header, so a caller can assert that a digest came back and
// that two digests taken across a forged write are equal.
function readCookieHeaderDigest(firstParty, url)
{
    const result = sendSync(IPC.connectionForProcessTarget('Networking'), IPC.messages.NetworkConnectionToWebProcess_CookieRequestHeaderFieldValueDigest.name, [
        { type: 'URL', value: firstParty },
        ...sameSiteInfoArgs(),
        { type: 'URL', value: url },
        { type: 'uint8_t', value: 1 }
    ]);
    const digestBytes = cookieHeaderDigestBytes(result);
    return {
        digest: cookieHeaderDigestHex(digestBytes),
        isDigest: !!digestBytes,
        replied: decodedReply(result),
        raw: safeJSONString(result)
    };
}

// SetCookiesFromDOM has no reply, so nothing here can observe whether it landed; a caller detects it by
// comparing cookie header digests taken before and after. Returning sent:true matters: a caller that
// asserts "the write did not become visible" has to be able to assert that a write was sent at all.
function setCookieFromDOM(firstParty, url, frameID, pageID, webPageProxyID, cookieString)
{
    if (!frameID || !pageID || !webPageProxyID)
        return { skipped: true };

    try {
        IPC.connectionForProcessTarget('Networking').sendMessage(0, IPC.messages.NetworkConnectionToWebProcess_SetCookiesFromDOM.name, [
            { type: 'URL', value: firstParty },
            ...sameSiteInfoArgs(),
            { type: 'URL', value: url },
            frameIDArg(frameID),
            { type: 'uint64_t', value: BigInt(pageID) },
            { type: 'String', value: cookieString },
            { type: 'uint8_t', value: 0 },
            { type: 'uint64_t', value: BigInt(webPageProxyID) }
        ]);
        return { sent: true };
    } catch (e) {
        return { exception: String(e) };
    }
}
</script>
)TESTRESOURCE"_s;

static String siteIsolationCookieIPCPage(ASCIILiteral pageScript)
{
    return makeString(siteIsolationCookieIPCHelperBytes, pageScript);
}

static constexpr auto siteIsolationCookieIPCMainBytes = R"TESTRESOURCE(
<title>forged cookie IPC proof main</title>
<script>
window.addEventListener('message', event => {
    if (!event.data || event.data.type !== 'cookie-proof-result')
        return;
    event.data.parentDocumentCookieAfterProof = document.cookie;
    alert('CookieIPCProof ' + JSON.stringify(event.data, (_, value) => typeof value === 'bigint' ? value.toString() : value));
});

setTimeout(() => alert('CookieIPCProof TIMEOUT document.cookie=' + document.cookie), 10000);

async function setCookiesAndStart()
{
    for (const path of [ '/set-normal', '/set-httponly', '/set-secure', '/set-strict' ])
        await fetch(path, { credentials: 'include', cache: 'no-store' });

    const attacker = document.createElement('iframe');
    attacker.src = 'https://siteb.example/cookie-attacker';
    attacker.addEventListener('load', () => {
        attacker.contentWindow.postMessage({
            type: 'start-cookie-proof',
            aFrameID: currentFrameIDString(),
            aPageID: IPC.pageID.toString(),
            aWebPageProxyID: IPC.webPageProxyID.toString(),
            firstParty: 'https://sitea.example/main',
            url: 'https://sitea.example/cookie-target'
        }, 'https://siteb.example');
    });
    document.body.appendChild(attacker);
}

setCookiesAndStart().catch(error => alert('CookieIPCProof setup failed: ' + error));
</script>
<body>
</body>
)TESTRESOURCE"_s;

static constexpr auto siteIsolationCookieIPCAttackerBytes = R"TESTRESOURCE(
<title>forged cookie IPC proof attacker</title>
<script>
window.addEventListener('message', async event => {
    const data = event.data;
    if (!data || data.type !== 'start-cookie-proof')
        return;

    const connection = IPC.connectionForProcessTarget('Networking');
    const firstPartyArg = { type: 'URL', value: data.firstParty };
    const urlArg = { type: 'URL', value: data.url };
    const includeSecureCookiesArg = { type: 'uint8_t', value: 1 };
    const noScriptTrackingPrivacyArg = { type: 'uint8_t', value: 0 };
    const nonOptionalFrameAndPage = [
        frameIDArg(data.aFrameID),
        { type: 'uint64_t', value: BigInt(data.aPageID) }
    ];
    const optionalFrameAndPage = [
        ...optionalFrameIDArgs(data.aFrameID),
        ...optionalPageIDArgs(data.aPageID)
    ];
    const optionalWebPageProxyID = optionalWebPageProxyIDArgs(data.aWebPageProxyID);

    const cookiesForDOM = sendSync(connection, IPC.messages.NetworkConnectionToWebProcess_CookiesForDOM.name, [
        firstPartyArg,
        ...sameSiteInfoArgs(),
        urlArg,
        ...nonOptionalFrameAndPage,
        includeSecureCookiesArg,
        { type: 'uint64_t', value: BigInt(data.aWebPageProxyID) }
    ]);

    // CookieRequestHeaderFieldValue, which the proof of concept read HttpOnly cookie values from, no longer
    // exists. Its replacement takes no frame, page or webPageProxy identifier and replies with one String.
    const cookieRequestHeaderFieldValueDigest = sendSync(connection, IPC.messages.NetworkConnectionToWebProcess_CookieRequestHeaderFieldValueDigest.name, [
        firstPartyArg,
        ...sameSiteInfoArgs(),
        urlArg,
        includeSecureCookiesArg
    ]);

    const getRawCookies = sendSync(connection, IPC.messages.NetworkConnectionToWebProcess_GetRawCookies.name, [
        firstPartyArg,
        ...sameSiteInfoArgs(),
        urlArg,
        ...optionalFrameAndPage,
        ...optionalWebPageProxyID
    ]);

    const cookiesForDOMAsync = await sendAsync(connection, IPC.messages.NetworkConnectionToWebProcess_CookiesForDOMAsync.name, [
        firstPartyArg,
        ...sameSiteInfoArgs(),
        urlArg,
        ...optionalFrameAndPage,
        includeSecureCookiesArg,
        { type: 'String', value: null },
        { type: 'String', value: '' },
        ...optionalWebPageProxyID
    ]);

    connection.sendMessage(0, IPC.messages.NetworkConnectionToWebProcess_SetCookiesFromDOM.name, [
        firstPartyArg,
        ...sameSiteInfoArgs(),
        urlArg,
        ...nonOptionalFrameAndPage,
        { type: 'String', value: 'normal_cookie=fromB; Path=/' },
        noScriptTrackingPrivacyArg,
        { type: 'uint64_t', value: BigInt(data.aWebPageProxyID) }
    ]);

    // Sent after the write on the same connection, so the network process handles it after the write: if the
    // write had landed, this digest would differ from the one above.
    const cookieRequestHeaderFieldValueDigestAfterSet = sendSync(connection, IPC.messages.NetworkConnectionToWebProcess_CookieRequestHeaderFieldValueDigest.name, [
        firstPartyArg,
        ...sameSiteInfoArgs(),
        urlArg,
        includeSecureCookiesArg
    ]);

    // Sent last, and for this frame's own site rather than the embedder's. Its reply arriving proves the
    // process was not terminated for any of the forged messages above, and that its own cookie access still
    // works. isSameSite is 0 because this document is cross-site to its first party for cookies.
    const ownSiteRead = sendSync(connection, IPC.messages.NetworkConnectionToWebProcess_CookiesForDOM.name, [
        firstPartyArg,
        { type: 'uint8_t', value: 0 }, // isSameSite
        { type: 'uint8_t', value: 0 }, // isTopSite
        { type: 'uint8_t', value: 1 }, // isSafeHTTPMethod
        { type: 'URL', value: location.href },
        frameIDArg(currentFrameIDString()),
        { type: 'uint64_t', value: BigInt(IPC.pageID) },
        includeSecureCookiesArg,
        { type: 'uint64_t', value: BigInt(IPC.webPageProxyID) }
    ]);

    const domCookieString = cookieStringValue(cookiesForDOM, 0);
    const httpCookieDigest = cookieHeaderDigestBytes(cookieRequestHeaderFieldValueDigest);
    const httpCookieDigestAfterSet = cookieHeaderDigestBytes(cookieRequestHeaderFieldValueDigestAfterSet);
    const rawCookiesArgument = replyArgument(getRawCookies, 0);
    const asyncCookiesArgument = replyArgument(cookiesForDOMAsync, 0);
    const rawCookiesByteLength = decodedArgumentByteLength(rawCookiesArgument);
    const asyncCookiesByteLength = decodedArgumentByteLength(asyncCookiesArgument);

    parent.postMessage({
        type: 'cookie-proof-result',
        attackerOrigin: location.origin,
        firstParty: data.firstParty,
        url: data.url,
        aFrameID: data.aFrameID,
        aPageID: data.aPageID,
        aWebPageProxyID: data.aWebPageProxyID,
        cookiesForDOM: domCookieString,
        cookieRequestHeaderFieldValueDigest: cookieHeaderDigestHex(httpCookieDigest),
        cookieRequestHeaderFieldValueDigestAfterSet: cookieHeaderDigestHex(httpCookieDigestAfterSet),
        // Reported so that a reply whose shape is not the expected engaged byte plus 20 digest bytes says what
        // it actually was, rather than only failing the assertion below.
        httpDigestByteLength: decodedArgumentByteLength(replyArgument(cookieRequestHeaderFieldValueDigest, 0)),
        getRawCookies: safeJSONString(getRawCookies.arguments || getRawCookies),
        getRawCookiesByteLength: rawCookiesByteLength,
        cookiesForDOMAsync: safeJSONString(cookiesForDOMAsync.arguments || cookiesForDOMAsync),
        cookiesForDOMAsyncByteLength: asyncCookiesByteLength,
        // CookiesForDOM's second reply argument. The proof of concept read it from the header message,
        // which no longer reports it. Its being there at all is also what proves a two argument reply to
        // CookiesForDOM was decoded, so that domReadEmpty below is a denial rather than a missing reply.
        didAccessSecureCookies: stringValue(cookiesForDOM, 1),
        observed: {
            domReadEmpty: isEmptyCookieString(domCookieString),
            domReadNormal: domCookieString.includes('normal_cookie=normalA'),
            domReadSecure: domCookieString.includes('secure_cookie=secureA'),
            domReadSameSiteStrict: domCookieString.includes('strict_cookie=strictA'),
            // GetRawCookies replies with a Vector<WebCore::Cookie>, which a denial makes empty rather than
            // absent, so its reply argument is still there and still decodes to bytes. Without this the
            // rawRead flags below would also all be false if the message had never been received at all.
            rawCookiesReplyDecoded: replyArgumentCount(getRawCookies) === 1 && rawCookiesByteLength >= 0,
            // The only HttpOnly flag worth asserting: NetworkStorageSession skips HttpOnly cookies for every
            // CookiesFor::DOM read, so the DOM and async equivalents are false on any build, fixed or not.
            rawReadNormal: decodedArgumentContains(rawCookiesArgument, 'normal_cookie'),
            rawReadHttpOnly: decodedArgumentContains(rawCookiesArgument, 'httponly_cookie'),
            rawReadSecure: decodedArgumentContains(rawCookiesArgument, 'secure_cookie'),
            rawReadSameSiteStrict: decodedArgumentContains(rawCookiesArgument, 'strict_cookie'),
            rawReadAnyCookie: decodedArgumentContains(rawCookiesArgument, '_cookie'),
            // A denied CookiesForDOMAsync replies std::nullopt, which surfaces as an undefined argument, so
            // one disengaged argument is itself the denial. The asyncRead flags cannot show that: they are
            // equally false when no reply was decoded at all.
            asyncCookiesReplyDecoded: replyArgumentCount(cookiesForDOMAsync) === 1,
            asyncCookiesDisengaged: asyncCookiesArgument === undefined,
            asyncReadNormal: decodedArgumentContains(asyncCookiesArgument, 'normal_cookie'),
            asyncReadSecure: decodedArgumentContains(asyncCookiesArgument, 'secure_cookie'),
            asyncReadSameSiteStrict: decodedArgumentContains(asyncCookiesArgument, 'strict_cookie'),
            asyncReadAnyCookie: decodedArgumentContains(asyncCookiesArgument, '_cookie'),
            // The proof of concept found normal_cookie=fromB in the header it read back. Only a digest is
            // available now, so the write is detected by the digest changing across it, which is salt
            // independent. That both are really digests is asserted separately, since a failed read would
            // otherwise make this comparison - the only proof of the write denial - meaningless.
            overwriteNormalCookie: !cookieHeaderDigestsEqual(httpCookieDigest, httpCookieDigestAfterSet),
            httpDigestReplied: decodedReply(cookieRequestHeaderFieldValueDigest) && decodedReply(cookieRequestHeaderFieldValueDigestAfterSet),
            httpDigestIsDigest: !!httpCookieDigest && !!httpCookieDigestAfterSet,
            attackerStillRunningAfterDenials: decodedReply(ownSiteRead)
        }
    }, 'https://sitea.example');
});
</script>
)TESTRESOURCE"_s;

TEST(IPCTestingAPI, SiteIsolationForgedNetworkCookieIPCReadWriteIsDenied)
{
    using namespace TestWebKitAPI;

    HTTPServer server({
        { "/main"_s, { { { "Content-Type"_s, "text/html"_s } }, siteIsolationCookieIPCPage(siteIsolationCookieIPCMainBytes) } },
        { "/set-normal"_s, { { { "Content-Type"_s, "text/plain"_s }, { "Set-Cookie"_s, "normal_cookie=normalA; Path=/"_s }, { "Cache-Control"_s, "no-store"_s } }, "normal"_s } },
        { "/set-httponly"_s, { { { "Content-Type"_s, "text/plain"_s }, { "Set-Cookie"_s, "httponly_cookie=httponlyA; Path=/; HttpOnly"_s }, { "Cache-Control"_s, "no-store"_s } }, "httponly"_s } },
        { "/set-secure"_s, { { { "Content-Type"_s, "text/plain"_s }, { "Set-Cookie"_s, "secure_cookie=secureA; Path=/; Secure; SameSite=None"_s }, { "Cache-Control"_s, "no-store"_s } }, "secure"_s } },
        { "/set-strict"_s, { { { "Content-Type"_s, "text/plain"_s }, { "Set-Cookie"_s, "strict_cookie=strictA; Path=/; SameSite=Strict"_s }, { "Cache-Control"_s, "no-store"_s } }, "strict"_s } },
        { "/cookie-attacker"_s, { { { "Content-Type"_s, "text/html"_s } }, siteIsolationCookieIPCPage(siteIsolationCookieIPCAttackerBytes) } },
        { "/cookie-target"_s, { { { "Content-Type"_s, "text/html"_s } }, "cookie target"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr<TestNavigationDelegate> navigationDelegate;
    RetainPtr<IPCTestingAPIAlertRecorder> alertRecorder;
    RetainPtr webView = createSiteIsolatedIPCTestWebView(server, navigationDelegate, alertRecorder);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://sitea.example/main"]]];

    NSString *result = [alertRecorder waitForAlert];
    ASSERT_TRUE(result) << "no payload alert; the attacker page never reported";
    EXPECT_TRUE([result containsString:@"\"attackerOrigin\":\"https://siteb.example\""]) << result.UTF8String;

    // The cookies the proof reads must actually exist, or every assertion that they did not leak passes for
    // the wrong reason. httponly_cookie needs this most: it can never appear in document.cookie, so nothing
    // else in this test would notice if /set-httponly had silently failed.
    RetainPtr proofDataStore = webView.get().configuration.websiteDataStore;
    EXPECT_WK_STREQ("httponlyA", cookieValueInStore(proofDataStore.get(), @"httponly_cookie"));
    EXPECT_WK_STREQ("normalA", cookieValueInStore(proofDataStore.get(), @"normal_cookie"));

    // Every read of sitea's cookies from siteb's process is denied, and denial is an empty reply. Each
    // group is preceded by the assertion that the reply it is about was decoded at all.
    EXPECT_TRUE([result containsString:@"\"didAccessSecureCookies\":false"]) << result.UTF8String;
    EXPECT_TRUE([result containsString:@"\"domReadEmpty\":true"]) << result.UTF8String;
    EXPECT_TRUE([result containsString:@"\"domReadNormal\":false"]) << result.UTF8String;
    EXPECT_TRUE([result containsString:@"\"domReadSecure\":false"]) << result.UTF8String;
    EXPECT_TRUE([result containsString:@"\"domReadSameSiteStrict\":false"]) << result.UTF8String;
    EXPECT_TRUE([result containsString:@"\"rawCookiesReplyDecoded\":true"]) << result.UTF8String;
    EXPECT_TRUE([result containsString:@"\"rawReadNormal\":false"]) << result.UTF8String;
    EXPECT_TRUE([result containsString:@"\"rawReadHttpOnly\":false"]) << result.UTF8String;
    EXPECT_TRUE([result containsString:@"\"rawReadSecure\":false"]) << result.UTF8String;
    EXPECT_TRUE([result containsString:@"\"rawReadSameSiteStrict\":false"]) << result.UTF8String;
    EXPECT_TRUE([result containsString:@"\"rawReadAnyCookie\":false"]) << result.UTF8String;
    EXPECT_TRUE([result containsString:@"\"asyncCookiesReplyDecoded\":true"]) << result.UTF8String;
    EXPECT_TRUE([result containsString:@"\"asyncCookiesDisengaged\":true"]) << result.UTF8String;
    EXPECT_TRUE([result containsString:@"\"asyncReadNormal\":false"]) << result.UTF8String;
    EXPECT_TRUE([result containsString:@"\"asyncReadSecure\":false"]) << result.UTF8String;
    EXPECT_TRUE([result containsString:@"\"asyncReadSameSiteStrict\":false"]) << result.UTF8String;
    EXPECT_TRUE([result containsString:@"\"asyncReadAnyCookie\":false"]) << result.UTF8String;

    // The reply to the header message is a salted digest, never a cookie header. The digest value itself is
    // not asserted: the salt is new on every network process launch. That message is deliberately not one
    // of the denied ones, so its reply arriving is asserted too, and only a 20-byte digest counts as one.
    EXPECT_TRUE([result containsString:@"\"httpDigestReplied\":true"]) << result.UTF8String;
    EXPECT_TRUE([result containsString:@"\"httpDigestIsDigest\":true"]) << result.UTF8String;

    // The forged write does not land: the digest for sitea's URL is unchanged across it, and sitea's own
    // document.cookie still has its original values, which is also what keeps the two EXPECT_FALSEs below
    // from being satisfied by an empty payload.
    EXPECT_TRUE([result containsString:@"\"overwriteNormalCookie\":false"]) << result.UTF8String;
    EXPECT_TRUE([result containsString:@"\"parentDocumentCookieAfterProof\":\""]) << result.UTF8String;
    EXPECT_TRUE([result containsString:@"normal_cookie=normalA"]) << result.UTF8String;
    EXPECT_TRUE([result containsString:@"secure_cookie=secureA"]) << result.UTF8String;
    EXPECT_TRUE([result containsString:@"strict_cookie=strictA"]) << result.UTF8String;
    EXPECT_FALSE([result containsString:@"normal_cookie=fromB"]) << result.UTF8String;
    // httponly_cookie is never in document.cookie and no reply here carries a cookie name, so the name
    // alone appearing anywhere in the payload is a leak.
    EXPECT_FALSE([result containsString:@"httponly_cookie"]) << result.UTF8String;

    // A denial is not a MESSAGE_CHECK: the process that sent the forged messages is still running and still
    // gets replies. Not that its own cookie access works - a denied reply decodes too, and siteb has no
    // cookies here. ThirdPartyIframeCookiesWithSiteIsolation is what covers an iframe reading its own site.
    EXPECT_TRUE([result containsString:@"\"attackerStillRunningAfterDenials\":true"]) << result.UTF8String;

    RetainPtr mainFrame = [webView mainFrame];
    EXPECT_EQ(1u, mainFrame.get().childFrames.count);
    RetainPtr<WKFrameInfo> attackerFrameInfo = mainFrame.get().childFrames.firstObject.info;
    pid_t mainFramePid = mainFrame.get().info._processIdentifier;
    pid_t attackerFramePid = attackerFrameInfo.get()._processIdentifier;
    // Without site isolation one process hosts every frame of the page, so it hosts a document for sitea
    // too and naming sitea's URL is legitimate. These reads are only expected to be denied because the
    // attacker really is in a process of its own.
    EXPECT_NE(mainFramePid, attackerFramePid);
    EXPECT_NE(0, attackerFramePid);
    if (attackerFrameInfo)
        EXPECT_WK_STREQ("https://siteb.example", [webView stringByEvaluatingJavaScript:@"location.origin" inFrame:attackerFrameInfo.get()]);
}

static constexpr auto siteIsolationCookieIPCScopeSetCookieBytes = R"TESTRESOURCE(
<title>cookie IPC scope setup</title>
<script>
alert('CookieIPCScopeSet ' + JSON.stringify({
    origin: location.origin,
    frameID: currentFrameIDString(),
    pageID: IPC.pageID.toString(),
    webPageProxyID: IPC.webPageProxyID.toString(),
    documentCookie: document.cookie
}, (_, value) => typeof value === 'bigint' ? value.toString() : value));
</script>
)TESTRESOURCE"_s;

static constexpr auto siteIsolationCookieIPCScopeEmbedMainBytes = R"TESTRESOURCE(
<title>cookie IPC scope embed main</title>
<script>
let completed = false;
function finishWithTimeout()
{
    if (!completed)
        alert('CookieIPCScopeProof TIMEOUT origin=' + location.origin + ' cookie=' + document.cookie);
}
setTimeout(finishWithTimeout, 10000);

window.addEventListener('message', event => {
    const data = event.data;
    if (!data || typeof data !== 'object')
        return;

    if (data.type === 'scope-embedded-ready') {
        event.source.postMessage({
            type: 'scope-embedded-start',
            firstPartyA: 'https://sitea.example/scope-set-a',
            urlA: 'https://sitea.example/scope-target'
        }, 'https://siteb.example');
        return;
    }

    if (data.type !== 'scope-embedded-result')
        return;

    completed = true;
    const params = new URLSearchParams;
    params.set('label', 'after-embed');
    params.set('firstPartyA', 'https://sitea.example/scope-set-a');
    params.set('urlA', 'https://sitea.example/scope-target');
    params.set('aFrameID', currentFrameIDString());
    params.set('aPageID', IPC.pageID.toString());
    params.set('aWebPageProxyID', IPC.webPageProxyID.toString());
    params.set('embeddedDigestIsDigest', data.readA && data.readA.replied && data.readA.isDigest ? '1' : '0');
    location.href = 'https://siteb.example/scope-top-b#' + params.toString();
});

const attacker = document.createElement('iframe');
attacker.src = 'https://siteb.example/scope-embedded-b';
document.documentElement.appendChild(attacker);
</script>
)TESTRESOURCE"_s;

static constexpr auto siteIsolationCookieIPCScopeEmbeddedAttackerBytes = R"TESTRESOURCE(
<title>cookie IPC scope embedded attacker</title>
<script>
window.addEventListener('message', event => {
    const data = event.data;
    if (!data || data.type !== 'scope-embedded-start')
        return;

    // CookieRequestHeaderFieldValueDigest is not one of the denied messages, so what has to hold for this
    // read of sitea's cookie header from siteb's process is that the reply is a digest and nothing else.
    const readA = readCookieHeaderDigest(data.firstPartyA, data.urlA);
    parent.postMessage({
        type: 'scope-embedded-result',
        origin: location.origin,
        readA
    }, 'https://sitea.example');
});

parent.postMessage({ type: 'scope-embedded-ready', origin: location.origin }, 'https://sitea.example');
</script>
)TESTRESOURCE"_s;

static constexpr auto siteIsolationCookieIPCScopeTopAttackerBytes = R"TESTRESOURCE(
<title>cookie IPC scope top attacker</title>
<script>
const params = new URLSearchParams(location.hash.substring(1));
const firstPartyA = params.get('firstPartyA') || 'https://sitea.example/scope-set-a';
const urlA = params.get('urlA') || 'https://sitea.example/scope-target';
const readABeforeSet = readCookieHeaderDigest(firstPartyA, urlA);
const setA = setCookieFromDOM(firstPartyA, urlA, params.get('aFrameID'), params.get('aPageID'), params.get('aWebPageProxyID'), 'scope_visible=topB; Path=/');
// The write has no reply, but it and this read travel the same connection, so the network process handles
// the read after it: a write that had landed would show up as a different digest here.
const readAAfterSet = readCookieHeaderDigest(firstPartyA, urlA);

alert('CookieIPCScopeProof ' + JSON.stringify({
    label: params.get('label') || 'no-embed',
    origin: location.origin,
    embeddedDigestIsDigest: params.get('embeddedDigestIsDigest') === '1',
    readABeforeSet,
    setA,
    readAAfterSet,
    // The proof of concept found scope_visible=topB in the cookie header it read back. Only a digest of
    // that header is available now, so the write is detected by the digest changing across it. Both
    // digests being digests is asserted separately, or a pair of failed reads would make this comparison
    // pass without anything having been compared.
    setAVisibleCookie: readAAfterSet.digest !== readABeforeSet.digest,
    digestsAreDigests: readABeforeSet.isDigest && readAAfterSet.isDigest,
    // Both reads replying at all, the second one after a denied write, is this process not having been
    // terminated for any of it.
    stillRunningAfterDenials: readABeforeSet.replied && readAAfterSet.replied
}, (_, value) => typeof value === 'bigint' ? value.toString() : value));
</script>
)TESTRESOURCE"_s;

static constexpr auto siteIsolationCookieIPCScopeVerifyBytes = R"TESTRESOURCE(
<title>cookie IPC scope verify</title>
<script>
alert('CookieIPCScopeVerify ' + JSON.stringify({
    origin: location.origin,
    documentCookie: document.cookie
}));
</script>
)TESTRESOURCE"_s;

TEST(IPCTestingAPI, SiteIsolationForgedNetworkCookieIPCScopeExpansionIsDenied)
{
    using namespace TestWebKitAPI;

    HTTPServer server({
        { "/scope-set-a"_s, { { { "Content-Type"_s, "text/html"_s }, { "Set-Cookie"_s, "scope_session=scopeA; Path=/; HttpOnly; Secure; SameSite=Strict"_s }, { "Cache-Control"_s, "no-store"_s } }, siteIsolationCookieIPCPage(siteIsolationCookieIPCScopeSetCookieBytes) } },
        { "/scope-embed-a"_s, { { { "Content-Type"_s, "text/html"_s }, { "Cache-Control"_s, "no-store"_s } }, siteIsolationCookieIPCPage(siteIsolationCookieIPCScopeEmbedMainBytes) } },
        { "/scope-embedded-b"_s, { { { "Content-Type"_s, "text/html"_s }, { "Cache-Control"_s, "no-store"_s } }, siteIsolationCookieIPCPage(siteIsolationCookieIPCScopeEmbeddedAttackerBytes) } },
        { "/scope-top-b"_s, { { { "Content-Type"_s, "text/html"_s }, { "Cache-Control"_s, "no-store"_s } }, siteIsolationCookieIPCPage(siteIsolationCookieIPCScopeTopAttackerBytes) } },
        { "/scope-target"_s, { { { "Content-Type"_s, "text/html"_s }, { "Cache-Control"_s, "no-store"_s } }, "scope target"_s } },
        { "/scope-verify"_s, { { { "Content-Type"_s, "text/html"_s }, { "Cache-Control"_s, "no-store"_s } }, siteIsolationCookieIPCPage(siteIsolationCookieIPCScopeVerifyBytes) } },
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr<TestNavigationDelegate> navigationDelegate;
    RetainPtr<IPCTestingAPIAlertRecorder> alertRecorder;
    RetainPtr webView = createSiteIsolatedIPCTestWebView(server, navigationDelegate, alertRecorder);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://sitea.example/scope-set-a"]]];
    NSString *setAResult = [alertRecorder waitForAlert];
    ASSERT_TRUE(setAResult) << "no alert from the set-cookie step";
    EXPECT_TRUE([setAResult containsString:@"\"origin\":\"https://sitea.example\""]) << setAResult.UTF8String;

    // This step's alert comes from a top level https://siteb.example page naming
    // firstParty=https://sitea.example/scope-set-a. It runs only because /scope-top-b is same site for the
    // siteb subframe's process and reuses it, and that process's allowed first party set is add only. Were
    // that set ever subtractive, allowsFirstPartyForCookies() would answer Terminate and kill the process
    // before it alerted; the bounded wait below is what reports that.
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://sitea.example/scope-embed-a"]]];
    NSString *afterEmbedResult = [alertRecorder waitForAlert];
    ASSERT_TRUE(afterEmbedResult) << "no alert from the embedded-attacker step";
    EXPECT_TRUE([afterEmbedResult containsString:@"\"label\":\"after-embed\""]) << afterEmbedResult.UTF8String;
    EXPECT_TRUE([afterEmbedResult containsString:@"\"origin\":\"https://siteb.example\""]) << afterEmbedResult.UTF8String;

    // Neither while embedded in sitea's page nor after that embedding relationship ended does siteb's
    // process get anything but a digest for sitea's cookie header, or manage to write a cookie that
    // becomes visible to sitea. The write really was sent, so setAVisibleCookie:false is about a denied
    // write rather than about a write that never happened.
    EXPECT_TRUE([afterEmbedResult containsString:@"\"embeddedDigestIsDigest\":true"]) << afterEmbedResult.UTF8String;
    EXPECT_TRUE([afterEmbedResult containsString:@"\"digestsAreDigests\":true"]) << afterEmbedResult.UTF8String;
    EXPECT_TRUE([afterEmbedResult containsString:@"\"setA\":{\"sent\":true}"]) << afterEmbedResult.UTF8String;
    EXPECT_TRUE([afterEmbedResult containsString:@"\"setAVisibleCookie\":false"]) << afterEmbedResult.UTF8String;

    // A denial is not a MESSAGE_CHECK: the process that sent the forged messages is still running.
    EXPECT_TRUE([afterEmbedResult containsString:@"\"stillRunningAfterDenials\":true"]) << afterEmbedResult.UTF8String;
    EXPECT_WK_STREQ("https://siteb.example", [webView stringByEvaluatingJavaScript:@"location.origin"]);

    // The forged write is not in sitea's cookie store either. scope_session is HttpOnly, so a document of
    // sitea's own only ever sees scope_visible=topB here, and only if the write landed.
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://sitea.example/scope-verify"]]];
    NSString *verifyResult = [alertRecorder waitForAlert];
    ASSERT_TRUE(verifyResult) << "no alert from the verify step";
    EXPECT_TRUE([verifyResult containsString:@"\"origin\":\"https://sitea.example\""]) << verifyResult.UTF8String;

    // Asserted from the cookie store as well, which is the assertion the third proof of concept in this
    // series contributed: it is the store the forged write would have reached, it also holds the HttpOnly
    // cookie a document of sitea's cannot see, and reading scope_session back proves the store read works
    // rather than the absence of scope_visible being an empty answer.
    RetainPtr dataStore = webView.get().configuration.websiteDataStore;
    EXPECT_WK_STREQ("scopeA", cookieValueInStore(dataStore.get(), @"scope_session"));
    EXPECT_WK_STREQ("", cookieValueInStore(dataStore.get(), @"scope_visible"));
}

#endif

#if !HAVE(WK_SECURE_CODING_NSURLREQUEST)
TEST(IPCTestingAPI, CGColorInNSSecureCoding)
{
    RetainPtr archiver = adoptNS([[NSKeyedArchiver alloc] initRequiringSecureCoding:YES]);

    RetainPtr<id<NSKeyedArchiverDelegate, NSKeyedUnarchiverDelegate>> delegate = adoptNS([[NSClassFromString(@"WKSecureCodingArchivingDelegate") alloc] init]);
    archiver.get().delegate = delegate.get();

    NSString *key = @"SomeString";
    RetainPtr value = adoptCF(CGColorCreateSRGB(0.2, 0.3, 0.4, 0.5));
    auto payload = @{ key : static_cast<id>(value.get()) };
    [archiver encodeObject:payload forKey:NSKeyedArchiveRootObjectKey];
    [archiver finishEncoding];
    [archiver setDelegate:nil];

    auto data = [archiver encodedData];

    RetainPtr unarchiver = adoptNS([[NSKeyedUnarchiver alloc] initForReadingFromData:data error:nullptr]);
    unarchiver.get().decodingFailurePolicy = NSDecodingFailurePolicyRaiseException;
    unarchiver.get().delegate = delegate.get();

    RetainPtr allowedClassSet = adoptNS([NSMutableSet new]);
    [allowedClassSet addObject:NSDictionary.class];
    [allowedClassSet addObject:NSString.class];
    [allowedClassSet addObject:NSClassFromString(@"WKSecureCodingCGColorWrapper")];

    NSDictionary *result = [unarchiver decodeObjectOfClasses:allowedClassSet.get() forKey:NSKeyedArchiveRootObjectKey];
    // Round-tripping the color can slightly change the representation, causing [payload isEqual:result] to report NO.
    EXPECT_EQ(result.count, static_cast<NSUInteger>(1));
    NSString *resultKey = result.allKeys[0];
    EXPECT_TRUE([key isEqual:resultKey]);
    CGColorRef resultValue = static_cast<CGColorRef>(result.allValues[0]);
    ASSERT_EQ(CFGetTypeID(resultValue), CGColorGetTypeID());
    RetainPtr resultValueColorSpace = CGColorGetColorSpace(resultValue);
    RetainPtr resultValueColorSpaceName = adoptCF(CGColorSpaceCopyName(resultValueColorSpace.get()));
    EXPECT_NE(CFStringFind(resultValueColorSpaceName.get(), CFSTR("SRGB"), 0).location, kCFNotFound);
    ASSERT_EQ(CGColorGetNumberOfComponents(resultValue), CGColorGetNumberOfComponents(value.get()));
    for (size_t i = 0; i < CGColorGetNumberOfComponents(resultValue); ++i)
        EXPECT_EQ(CGColorGetComponents(resultValue)[i], CGColorGetComponents(value.get())[i]);
    [unarchiver finishDecoding];
    unarchiver.get().delegate = nil;
}

TEST(IPCTestingAPI, NSURLWithBaseURLInNSSecureCoding)
{
    RetainPtr archiver = adoptNS([[NSKeyedArchiver alloc] initRequiringSecureCoding:YES]);

    RetainPtr<id<NSKeyedArchiverDelegate, NSKeyedUnarchiverDelegate>> delegate = adoptNS([[NSClassFromString(@"WKSecureCodingArchivingDelegate") alloc] init]);
    archiver.get().delegate = delegate.get();

    NSString *key = @"SomeString";
    NSURL *value = [NSURL URLWithString:@"/garden_home.html" relativeToURL:[NSURL URLWithString:@"amcomponent://com.xunmeng.pinduoduo/"]];
    EXPECT_WK_STREQ(value.baseURL.absoluteString, @"amcomponent://com.xunmeng.pinduoduo/");
    EXPECT_WK_STREQ(value.relativeString, @"/garden_home.html");
    EXPECT_WK_STREQ(value.absoluteString, @"amcomponent://com.xunmeng.pinduoduo/garden_home.html");

    auto payload = @{ key : static_cast<id>(value) };
    [archiver encodeObject:payload forKey:NSKeyedArchiveRootObjectKey];
    [archiver finishEncoding];
    [archiver setDelegate:nil];

    auto data = [archiver encodedData];

    RetainPtr unarchiver = adoptNS([[NSKeyedUnarchiver alloc] initForReadingFromData:data error:nullptr]);
    unarchiver.get().decodingFailurePolicy = NSDecodingFailurePolicyRaiseException;
    unarchiver.get().delegate = delegate.get();

    RetainPtr allowedClassSet = adoptNS([NSMutableSet new]);
    [allowedClassSet addObject:NSDictionary.class];
    [allowedClassSet addObject:NSString.class];
    [allowedClassSet addObject:NSClassFromString(@"WKSecureCodingURLWrapper")];

    NSDictionary *result = [unarchiver decodeObjectOfClasses:allowedClassSet.get() forKey:NSKeyedArchiveRootObjectKey];

    EXPECT_EQ(result.count, static_cast<NSUInteger>(1));
    NSString *resultKey = result.allKeys[0];
    EXPECT_TRUE([key isEqual:resultKey]);
    RetainPtr resultValue = checked_objc_cast<NSURL>(result.allValues[0]);

    // Our coder resolves the URL so we end up with an absolute URL instead of base URL + relative string.
    EXPECT_WK_STREQ(resultValue.get().baseURL.absoluteString, @"");
    EXPECT_WK_STREQ(resultValue.get().baseURL.relativeString, @"");
    EXPECT_WK_STREQ(resultValue.get().absoluteString, @"amcomponent://com.xunmeng.pinduoduo/garden_home.html");
    [unarchiver finishDecoding];
    unarchiver.get().delegate = nil;
}
#endif // !HAVE(WK_SECURE_CODING_NSURLREQUEST)
