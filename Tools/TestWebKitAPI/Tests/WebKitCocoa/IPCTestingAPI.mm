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

#import "DeprecatedGlobalValues.h"
#import "RemoteObjectRegistry.h"
#import "TestWKWebView.h"
#import "Utilities.h"
#import <WebKit/WKNavigationDelegatePrivate.h>
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKWebView.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/_WKInternalDebugFeature.h>
#import <WebKit/_WKRemoteObjectInterface.h>
#import <WebKit/_WKRemoteObjectRegistry.h>
#import <wtf/RetainPtr.h>

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
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);

    auto delegate = adoptNS([[IPCTestingAPIDelegate alloc] init]);
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
    for (_WKInternalDebugFeature *feature in [WKPreferences _internalDebugFeatures]) {
        if ([feature.key isEqualToString:@"IPCTestingAPIEnabled"]) {
            [[configuration preferences] _setEnabled:YES forInternalDebugFeature:feature];
            break;
        }
    }
    return adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 300) configuration:configuration.get()]);
}

#if !ASSERT_ENABLED

TEST(IPCTestingAPI, CanDetectNilReplyBlocks)
{
    auto webView = createWebViewWithIPCTestingAPI();

    auto delegate = adoptNS([[IPCTestingAPIDelegate alloc] init]);
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

#endif

TEST(IPCTestingAPI, CanSendAlert)
{
    auto webView = createWebViewWithIPCTestingAPI();

    auto delegate = adoptNS([[IPCTestingAPIDelegate alloc] init]);
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

    auto delegate = adoptNS([[IPCTestingAPIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];

    done = false;
    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html><script>alert(IPC.messages.WebPageProxy_RunJavaScriptAlert.isSync);</script>"];
    TestWebKitAPI::Util::run(&done);

    EXPECT_STREQ([alertMessage UTF8String], "true");
}

TEST(IPCTestingAPI, CanSendInvalidAsyncMessageToUIProcessWithoutTermination)
{
    auto webView = createWebViewWithIPCTestingAPI();

    auto delegate = adoptNS([[IPCTestingAPIDelegate alloc] init]);
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

    auto delegate = adoptNS([[IPCTestingAPIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];

    done = false;
    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html><script>"
        "IPC.sendSyncMessage('UI', IPC.webPageProxyID, IPC.messages.WebPageProxy_RunJavaScriptAlert.name, 100, [{type: 'FrameID', value: IPC.frameID}]);"
        "alert('hi')</script>"];
    TestWebKitAPI::Util::run(&done);

    EXPECT_STREQ([alertMessage UTF8String], "hi");
}

#if ENABLE(GPU_PROCESS)

TEST(IPCTestingAPI, CanSendSyncMessageToGPUProcess)
{
    auto webView = createWebViewWithIPCTestingAPI();

    auto delegate = adoptNS([[IPCTestingAPIDelegate alloc] init]);
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

    auto delegate = adoptNS([[IPCTestingAPIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];

    done = false;
    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html><script>(async function test() {"
        "window.result = await IPC.sendMessage('GPU', 0, IPC.messages.RemoteAudioDestinationManager_StartAudioDestination.name, [{type: 'uint64_t', value: 12345}]);"
        "alert(!!result)"
        "})();</script>"];
    TestWebKitAPI::Util::run(&done);

    EXPECT_TRUE([alertMessage boolValue]);
    EXPECT_STREQ([webView stringByEvaluatingJavaScript:@"result.arguments[0].type"].UTF8String, "bool");
    EXPECT_FALSE([webView stringByEvaluatingJavaScript:@"result.arguments[0].value"].boolValue);
}

TEST(IPCTestingAPI, CanSendInvalidAsyncMessageToGPUProcessWithoutTermination)
{
    auto webView = createWebViewWithIPCTestingAPI();

    auto delegate = adoptNS([[IPCTestingAPIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];

    done = false;
    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html><script>(async function test() {"
        "IPC.sendMessage('GPU', 0, IPC.messages.GPUConnectionToWebProcess_CreateRenderingBackend.name, []);"
        "window.result = await IPC.sendMessage('GPU', 0, IPC.messages.RemoteAudioDestinationManager_StartAudioDestination.name, [{type: 'uint64_t', value: 12345}]);"
        "alert(!!result)"
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

    auto delegate = adoptNS([[IPCTestingAPIDelegate alloc] init]);
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

    auto delegate = adoptNS([[IPCTestingAPIDelegate alloc] init]);
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

    auto delegate = adoptNS([[IPCTestingAPIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];

    done = false;
    promptResult = @"foo";
    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html><script>result = IPC.sendSyncMessage('UI', IPC.webPageProxyID, IPC.messages.WebPageProxy_RunJavaScriptPrompt.name, 100,"
        "[{type: 'FrameID', value: IPC.frameID}, {type: 'FrameInfoData', value: IPC}, {'type': 'String', 'value': 'hi'}, {'type': 'String', 'value': 'bar'}]);</script>"];
    TestWebKitAPI::Util::run(&done);

    EXPECT_STREQ([promptDefault UTF8String], "bar");
    EXPECT_STREQ([[webView stringByEvaluatingJavaScript:@"JSON.stringify(result.arguments)"] UTF8String], "[{\"type\":\"String\",\"value\":\"foo\"}]");
}

#if ENABLE(TRACKING_PREVENTION)
TEST(IPCTestingAPI, DecodesReplyArgumentsForAsyncMessage)
{
    auto webView = createWebViewWithIPCTestingAPI();

    auto delegate = adoptNS([[IPCTestingAPIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];

    done = false;
    promptResult = @"foo";
    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html><script>IPC.sendMessage(\"Networking\", 0, IPC.messages.NetworkConnectionToWebProcess_HasStorageAccess.name,"
        "[{type: 'RegistrableDomain', value: 'https://ipctestingapi.com'}, {type: 'RegistrableDomain', value: 'https://webkit.org'}, {type: 'FrameID', value: IPC.frameID},"
        "{type: 'uint64_t', value: IPC.pageID}]).then((result) => alert(JSON.stringify(result.arguments)));</script>"];
    TestWebKitAPI::Util::run(&done);

    EXPECT_STREQ([alertMessage UTF8String], "[{\"type\":\"bool\",\"value\":false}]");
}
#endif

TEST(IPCTestingAPI, DescribesArguments)
{
    auto webView = createWebViewWithIPCTestingAPI();

    auto delegate = adoptNS([[IPCTestingAPIDelegate alloc] init]);
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

    auto delegate = adoptNS([[IPCTestingAPIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];

    done = false;
    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html><script>messages = []; IPC.addOutgoingMessageListener('UI', (message) => messages.push(message)); alert('ok');</script>"];
    TestWebKitAPI::Util::run(&done);

    EXPECT_STREQ([alertMessage UTF8String], "ok");
    EXPECT_STREQ([webView stringByEvaluatingJavaScript:@"messages = messages.filter((message) => message.name == IPC.messages.WebPageProxy_RunJavaScriptAlert.name); messages.length"].UTF8String, "1");
    EXPECT_STREQ([webView stringByEvaluatingJavaScript:@"messages[0].description"].UTF8String, "WebPageProxy_RunJavaScriptAlert");
    EXPECT_EQ([webView stringByEvaluatingJavaScript:@"args = messages[0].arguments; args.length"].intValue, 3);
    EXPECT_STREQ([webView stringByEvaluatingJavaScript:@"args[0].type"].UTF8String, "(null)");
    EXPECT_EQ([webView stringByEvaluatingJavaScript:@"args[0].value"].intValue, 0);
    EXPECT_EQ([webView stringByEvaluatingJavaScript:@"args[1] instanceof ArrayBuffer"].boolValue, YES);
    EXPECT_STREQ([webView stringByEvaluatingJavaScript:@"args[2].type"].UTF8String, "String");
    EXPECT_STREQ([webView stringByEvaluatingJavaScript:@"args[2].value"].UTF8String, "ok");
    EXPECT_STREQ([webView stringByEvaluatingJavaScript:@"typeof(messages[0].syncRequestID)"].UTF8String, "number");
    EXPECT_EQ([webView stringByEvaluatingJavaScript:@"messages[0].destinationID[0]"].intValue,
        [webView stringByEvaluatingJavaScript:@"IPC.webPageProxyID.toString()"].intValue);
}

#if ENABLE(TRACKING_PREVENTION)
TEST(IPCTestingAPI, CanInterceptHasStorageAccess)
{
    auto webView = createWebViewWithIPCTestingAPI();

    auto delegate = adoptNS([[IPCTestingAPIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];

    done = false;
    promptResult = @"foo";
    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html><script>let targetMessage = {}; const messageName = IPC.messages.NetworkConnectionToWebProcess_HasStorageAccess.name;"
        "IPC.addOutgoingMessageListener('Networking', (currentMessage) => { if (currentMessage.name == messageName) targetMessage = currentMessage; });"
        "IPC.sendMessage('Networking', 0, messageName, [{type: 'RegistrableDomain', value: 'https://ipctestingapi.com'}, {type: 'RegistrableDomain', value: 'https://webkit.org'},"
        "{type: 'FrameID', value: IPC.frameID}, {type: 'uint64_t', value: IPC.pageID}]).then((result) => alert(JSON.stringify(result.arguments)));</script>"];
    TestWebKitAPI::Util::run(&done);

    EXPECT_STREQ([alertMessage UTF8String], "[{\"type\":\"bool\",\"value\":false}]");
    EXPECT_STREQ([webView stringByEvaluatingJavaScript:@"targetMessage.description"].UTF8String, "NetworkConnectionToWebProcess_HasStorageAccess");
    EXPECT_EQ([webView stringByEvaluatingJavaScript:@"targetMessage.arguments.length"].intValue, 4);
    EXPECT_STREQ([webView stringByEvaluatingJavaScript:@"targetMessage.arguments[0].type"].UTF8String, "RegistrableDomain");
    EXPECT_STREQ([webView stringByEvaluatingJavaScript:@"targetMessage.arguments[0].value"].UTF8String, "ipctestingapi.com");
    EXPECT_STREQ([webView stringByEvaluatingJavaScript:@"targetMessage.arguments[1].type"].UTF8String, "RegistrableDomain");
    EXPECT_STREQ([webView stringByEvaluatingJavaScript:@"targetMessage.arguments[1].value"].UTF8String, "webkit.org");
    EXPECT_STREQ([webView stringByEvaluatingJavaScript:@"targetMessage.arguments[2].type"].UTF8String, "(null)");
    EXPECT_STREQ([webView stringByEvaluatingJavaScript:@"targetMessage.arguments[2].value"].UTF8String, "(null)");
    EXPECT_STREQ([webView stringByEvaluatingJavaScript:@"targetMessage.arguments[3].type"].UTF8String, "uint64_t");
    EXPECT_EQ([webView stringByEvaluatingJavaScript:@"targetMessage.arguments[3].value"].intValue, [webView stringByEvaluatingJavaScript:@"IPC.pageID.toString()"].intValue);
    EXPECT_STREQ([webView stringByEvaluatingJavaScript:@"typeof(targetMessage.syncRequestID)"].UTF8String, "undefined");
    EXPECT_EQ([webView stringByEvaluatingJavaScript:@"targetMessage.destinationID[0]"].intValue, 0);
}
#endif

TEST(IPCTestingAPI, CanInterceptFindString)
{
    auto webView = createWebViewWithIPCTestingAPI();

    auto delegate = adoptNS([[IPCTestingAPIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];

    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html><body><p>hello</p><script>messages = []; IPC.addIncomingMessageListener('UI', (message) => messages.push(message));</script>"];

    done = false;
    auto findConfiguration = adoptNS([[WKFindConfiguration alloc] init]);
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
    EXPECT_EQ([webView stringByEvaluatingJavaScript:@"messages[0].destinationID[0]"].intValue,
        [webView stringByEvaluatingJavaScript:@"IPC.webPageProxyID.toString()"].intValue);
}

static NSSet<NSString *> *splitTypeFromList(NSString *input, bool firstTypeOnly)
{
    NSMutableSet *set = NSMutableSet.set;
    size_t nestedTypeDepth { 0 };
    bool atComma { false };
    size_t previousTypeEnd { 0 };
    for (size_t i = 0; i < input.length; i++) {
        auto c = [input characterAtIndex:i];
        if (c == '<')
            nestedTypeDepth++;
        if (c == '>')
            nestedTypeDepth--;
        if (c == ',') {
            atComma = true;
            continue;
        }
        if (c == ' ' && !nestedTypeDepth && atComma) {
            [set addObject:[input substringWithRange:NSMakeRange(previousTypeEnd, i - 1 - previousTypeEnd)]];
            if (firstTypeOnly)
                return set;
            previousTypeEnd = i + 1;
        }
        atComma = false;
    }
    [set addObject:[input substringWithRange:NSMakeRange(previousTypeEnd, input.length - previousTypeEnd)]];
    return set;
}

static NSMutableSet<NSString *> *extractTypesFromContainers(NSSet<NSString *> *inputSet)
{
    NSMutableSet *outputSet = NSMutableSet.set;
    for (NSString *input in inputSet) {
        BOOL foundContainer = NO;
        NSArray<NSString *> *containerTypes = @[
            @"Expected",
            @"HashMap",
            @"std::pair",
            @"IPC::ArrayReferenceTuple",
            @"IPC::ArrayReference",
            @"std::variant",
            @"std::unique_ptr",
            @"Vector",
            @"HashSet",
            @"Ref",
            @"RefPtr",
            @"std::optional",
            @"OptionSet",
            @"RetainPtr",
            @"WebCore::RectEdges"
        ];
        for (NSString *container in containerTypes) {
            if ([input hasPrefix:[container stringByAppendingString:@"<"]]
                && [input hasSuffix:@">"]) {
                NSString *containedTypes = [input substringWithRange:NSMakeRange(container.length + 1, input.length - container.length - 2)];
                for (NSString *type : extractTypesFromContainers(splitTypeFromList(containedTypes, [container isEqualToString:@"IPC::ArrayReference"])))
                    [outputSet addObject:type];
                foundContainer = YES;
            }
        }
        if (!foundContainer)
            [outputSet addObject:input];
    }
    return outputSet;
}

TEST(IPCTestingAPI, SerializedTypeInfo)
{
    auto webView = createWebViewWithIPCTestingAPI();
    NSDictionary *typeInfo = [webView objectByEvaluatingJavaScript:@"IPC.serializedTypeInfo"];
    NSArray *expectedArray = @[@"bool", @"bool", @"bool"];
    EXPECT_TRUE([typeInfo[@"WebCore::CacheQueryOptions"] isEqualToArray:expectedArray]);
    NSDictionary *expectedDictionary = @{
        @"isOptionSet" : @1,
        @"size" : @1,
        @"validValues" : @[@1, @2]
    };

    NSDictionary *enumInfo = [webView objectByEvaluatingJavaScript:@"IPC.serializedEnumInfo"];
    EXPECT_TRUE([enumInfo[@"WebKit::WebsiteDataFetchOption"] isEqualToDictionary:expectedDictionary]);

    NSArray *objectIdentifiers = [webView objectByEvaluatingJavaScript:@"IPC.objectIdentifiers"];
    EXPECT_TRUE([objectIdentifiers containsObject:@"WebCore::PageIdentifier"]);

    NSMutableSet<NSString *> *typesNeedingDescriptions = NSMutableSet.set;
    NSDictionary *messages = [webView objectByEvaluatingJavaScript:@"IPC.messages"];
    for (NSDictionary *message in messages.allValues) {
        if (![message isKindOfClass:NSDictionary.class])
            continue;
        for (NSString *key in @[@"arguments", @"replyArguments"]) {
            NSArray *arguments = message[key];
            if (![arguments isKindOfClass:NSArray.class])
                continue;
            for (NSDictionary *argument in arguments) {
                if (![argument isKindOfClass:NSDictionary.class])
                    continue;
                [typesNeedingDescriptions addObject:argument[@"type"]];
            }
        }
    }
    for (NSArray *memberTypes in typeInfo.allValues) {
        for (NSString *memberType in memberTypes)
            [typesNeedingDescriptions addObject:memberType];
    }

    typesNeedingDescriptions = extractTypesFromContainers(typesNeedingDescriptions);

    NSMutableSet<NSString *> *typesHavingDescriptions = NSMutableSet.set;
    for (NSString *describedType in typeInfo.keyEnumerator)
        [typesHavingDescriptions addObject:describedType];
    for (NSString *describedType in enumInfo.keyEnumerator)
        [typesHavingDescriptions addObject:describedType];
    for (NSString *objectIdentifier in objectIdentifiers)
        [typesHavingDescriptions addObject:objectIdentifier];

    [typesNeedingDescriptions minusSet:typesHavingDescriptions];

}

#endif

TEST(IPCTestingAPI, CGColorInNSSecureCoding)
{
    auto archiver = adoptNS([[NSKeyedArchiver alloc] initRequiringSecureCoding:YES]);

    RetainPtr<id<NSKeyedArchiverDelegate, NSKeyedUnarchiverDelegate>> delegate = adoptNS([[NSClassFromString(@"WKSecureCodingArchivingDelegate") alloc] init]);
    archiver.get().delegate = delegate.get();

    NSString *key = @"SomeString";
    auto value = adoptCF(CGColorCreateSRGB(0.2, 0.3, 0.4, 0.5));
    auto payload = @{ key : static_cast<id>(value.get()) };
    [archiver encodeObject:payload forKey:NSKeyedArchiveRootObjectKey];
    [archiver finishEncoding];
    [archiver setDelegate:nil];

    auto data = [archiver encodedData];

    auto unarchiver = adoptNS([[NSKeyedUnarchiver alloc] initForReadingFromData:data error:nullptr]);
    unarchiver.get().decodingFailurePolicy = NSDecodingFailurePolicyRaiseException;
    unarchiver.get().delegate = delegate.get();

    auto allowedClassSet = adoptNS([NSMutableSet new]);
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
    auto resultValueColorSpace = adoptCF(CGColorGetColorSpace(resultValue));
    auto resultValueColorSpaceName = adoptCF(CGColorSpaceCopyName(resultValueColorSpace.get()));
    EXPECT_NE(CFStringFind(resultValueColorSpaceName.get(), CFSTR("SRGB"), 0).location, kCFNotFound);
    ASSERT_EQ(CGColorGetNumberOfComponents(resultValue), CGColorGetNumberOfComponents(value.get()));
    for (size_t i = 0; i < CGColorGetNumberOfComponents(resultValue); ++i)
        EXPECT_EQ(CGColorGetComponents(resultValue)[i], CGColorGetComponents(value.get())[i]);
    [unarchiver finishDecoding];
    unarchiver.get().delegate = nil;
}
