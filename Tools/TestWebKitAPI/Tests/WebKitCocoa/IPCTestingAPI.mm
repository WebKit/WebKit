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

#import "TestWKWebView.h"
#import "Utilities.h"
#import <WebKit/WKNavigationDelegatePrivate.h>
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKWebView.h>
#import <WebKit/_WKInternalDebugFeature.h>
#import <wtf/RetainPtr.h>

static bool done = false;
static bool didCrash = false;
static RetainPtr<NSString> alertMessage;
static RetainPtr<NSString> promptDefault;
static RetainPtr<NSString> promptResult;

@interface IPCTestingAPIDelegate : NSObject <WKUIDelegate, WKNavigationDelegate>
@end
    
@implementation IPCTestingAPIDelegate

- (void)webView:(WKWebView *)webView runJavaScriptAlertPanelWithMessage:(NSString *)message initiatedByFrame:(WKFrameInfo *)frame completionHandler:(void (^)(void))completionHandler
{
    alertMessage = message;
    done = true;
    completionHandler();
}

- (void)webView:(WKWebView *)webView runJavaScriptTextInputPanelWithPrompt:(NSString *)prompt defaultText:(NSString *)defaultText initiatedByFrame:(WKFrameInfo *)frame completionHandler:(void (^)(NSString * _Nullable))completionHandler
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

TEST(IPCTestingAPI, CanSendAlert)
{
    auto webView = createWebViewWithIPCTestingAPI();

    auto delegate = adoptNS([[IPCTestingAPIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];

    done = false;
    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html><script>IPC.sendSyncMessage('UI', IPC.webPageProxyID, IPC.messages.WebPageProxy_RunJavaScriptAlert.name, 100,"
        "[{type: 'uint64_t', value: IPC.frameID}, {type: 'FrameInfoData'}, {'type': 'String', 'value': 'hi'}]);</script>"];
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
        "IPC.sendSyncMessage('UI', IPC.webPageProxyID, IPC.messages.WebPageProxy_RunJavaScriptAlert.name, 100, [{type: 'uint64_t', value: IPC.frameID}]);"
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

TEST(IPCTestingAPI, CanReceiveIPCSemaphore)
{
    auto webView = createWebViewWithIPCTestingAPI();

    auto delegate = adoptNS([[IPCTestingAPIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];

    done = false;
    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html><script>"
        "const semaphore = IPC.createSemaphore();"
        "IPC.sendMessage('GPU', 0, IPC.messages.GPUConnectionToWebProcess_CreateRenderingBackend.name,"
        "    [{type: 'RemoteRenderingBackendCreationParameters', 'identifier': 123, semaphore, 'pageProxyID': IPC.webPageProxyID, 'pageID': IPC.pageID}]);"
        "const result = IPC.sendSyncMessage('GPU', 123, IPC.messages.RemoteRenderingBackend_SemaphoreForGetImageData.name, 100, []);"
        "semaphore.signal();"
        "alert(result.arguments.length + ':' + result.arguments[0].type + ':' + result.arguments[0].value.waitFor(100));"
        "</script>"];
    TestWebKitAPI::Util::run(&done);

    EXPECT_STREQ([alertMessage UTF8String], "1:Semaphore:false");
}

TEST(IPCTestingAPI, CanReceiveSharedMemory)
{
    auto webView = createWebViewWithIPCTestingAPI();

    auto delegate = adoptNS([[IPCTestingAPIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];

    auto* html = @R"HTML(<!DOCTYPE html>
<script>
IPC.sendMessage('GPU', 0, IPC.messages.GPUConnectionToWebProcess_CreateRenderingBackend.name,
    [{type: 'RemoteRenderingBackendCreationParameters', 'identifier': 123, semaphore: IPC.createSemaphore(), 'pageProxyID': IPC.webPageProxyID, 'pageID': IPC.pageID}]);
const result = IPC.sendSyncMessage('GPU', 123, IPC.messages.RemoteRenderingBackend_UpdateSharedMemoryForGetImageData.name, 100, [{type: 'uint32_t', value: 8}]);
alert(result.arguments.length);
</script>)HTML";

    done = false;
    [webView synchronouslyLoadHTMLString:html];
    TestWebKitAPI::Util::run(&done);

    EXPECT_EQ([alertMessage intValue], 1);
    EXPECT_STREQ([webView stringByEvaluatingJavaScript:@"firstReply = result.arguments[0]; firstReply.type"].UTF8String, "SharedMemory");
    EXPECT_STREQ([webView stringByEvaluatingJavaScript:@"firstReply.protection"].UTF8String, "ReadOnly");
    EXPECT_STREQ([webView stringByEvaluatingJavaScript:@"Array.from(new Uint8Array(firstReply.value.readBytes(0, 8))).toString()"].UTF8String, "0,0,0,0,0,0,0,0");

}
#endif

TEST(IPCTestingAPI, CanCreateIPCSemaphore)
{
    auto webView = createWebViewWithIPCTestingAPI();

    auto delegate = adoptNS([[IPCTestingAPIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];

    done = false;
    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html><script>alert(IPC.createSemaphore().waitFor(100));</script>"];
    TestWebKitAPI::Util::run(&done);

    EXPECT_FALSE([alertMessage boolValue]);
}

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

TEST(IPCTestingAPI, CanSendSemaphore)
{
    auto webView = createWebViewWithIPCTestingAPI();

    auto delegate = adoptNS([[IPCTestingAPIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];

    auto* html = @R"HTML(<!DOCTYPE html>
<body>
<script>
const audioContext = new AudioContext;
const destination = audioContext.createMediaStreamDestination();
const semaphore = IPC.createSemaphore();
const result = IPC.sendSyncMessage('GPU', 0, IPC.messages.RemoteAudioDestinationManager_CreateAudioDestination.name, 100,
    [{type: 'String', value: 'some device'},
    {type: 'uint32_t', value: destination.numberOfInputs},
    {type: 'uint32_t', value: destination.channelCount},
    {type: 'float', value: audioContext.sampleRate}, {type: 'float', value: audioContext.sampleRate},
    {type: 'Semaphore', value: semaphore}]);
alert(result.arguments[0].type);
</script>
</body>)HTML";

    done = false;
    [webView synchronouslyLoadHTMLString:html];
    TestWebKitAPI::Util::run(&done);

    EXPECT_STREQ([alertMessage UTF8String], "uint64_t");
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
const result = IPC.sendSyncMessage('UI', 0, IPC.messages.WebPasteboardProxy_SetPasteboardBufferForType.name, 100, [
    {type: 'String', value: 'Apple CFPasteboard general'}, {type: 'String', value: 'text/plain'},
    {type: 'SharedMemory', value: sharedMemory, protection: 'ReadOnly'}, {type: 'bool', value: 1}, {type: 'uint64_t', value: IPC.pageID}]);
alert(result.arguments.length + ':' + JSON.stringify(result.arguments[0]));
</script>
</body>)HTML";

    done = false;
    [webView synchronouslyLoadHTMLString:html];
    TestWebKitAPI::Util::run(&done);

    EXPECT_STREQ([alertMessage UTF8String], "1:{\"type\":\"int64_t\",\"value\":0}");
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
        "[{type: 'uint64_t', value: IPC.frameID}, {type: 'FrameInfoData'}, {'type': 'String', 'value': 'hi'}, {'type': 'String', 'value': 'bar'}]);</script>"];
    TestWebKitAPI::Util::run(&done);

    EXPECT_STREQ([promptDefault UTF8String], "bar");
    EXPECT_STREQ([[webView stringByEvaluatingJavaScript:@"JSON.stringify(result.arguments)"] UTF8String], "[{\"type\":\"String\",\"value\":\"foo\"}]");
}

#if ENABLE(RESOURCE_LOAD_STATISTICS)
TEST(IPCTestingAPI, DecodesReplyArgumentsForAsyncMessage)
{
    auto webView = createWebViewWithIPCTestingAPI();

    auto delegate = adoptNS([[IPCTestingAPIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];

    done = false;
    promptResult = @"foo";
    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html><script>IPC.sendMessage(\"Networking\", 0, IPC.messages.NetworkConnectionToWebProcess_HasStorageAccess.name,"
        "[{type: 'RegistrableDomain', value: 'https://ipctestingapi.com'}, {type: 'RegistrableDomain', value: 'https://webkit.org'}, {type: 'uint64_t', value: IPC.frameID},"
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
    EXPECT_STREQ([webView stringByEvaluatingJavaScript:@"args[0].type"].UTF8String, "uint64_t");
    EXPECT_EQ([webView stringByEvaluatingJavaScript:@"args[0].value"].intValue, [webView stringByEvaluatingJavaScript:@"IPC.frameID.toString()"].intValue);
    EXPECT_EQ([webView stringByEvaluatingJavaScript:@"args[1] instanceof ArrayBuffer"].boolValue, YES);
    EXPECT_STREQ([webView stringByEvaluatingJavaScript:@"args[2].type"].UTF8String, "String");
    EXPECT_STREQ([webView stringByEvaluatingJavaScript:@"args[2].value"].UTF8String, "ok");
    EXPECT_STREQ([webView stringByEvaluatingJavaScript:@"typeof(messages[0].syncRequestID)"].UTF8String, "number");
    EXPECT_EQ([webView stringByEvaluatingJavaScript:@"messages[0].destinationID"].intValue,
        [webView stringByEvaluatingJavaScript:@"IPC.webPageProxyID.toString()"].intValue);
}

#if ENABLE(RESOURCE_LOAD_STATISTICS)
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
        "{type: 'uint64_t', value: IPC.frameID}, {type: 'uint64_t', value: IPC.pageID}]).then((result) => alert(JSON.stringify(result.arguments)));</script>"];
    TestWebKitAPI::Util::run(&done);

    EXPECT_STREQ([alertMessage UTF8String], "[{\"type\":\"bool\",\"value\":false}]");
    EXPECT_STREQ([webView stringByEvaluatingJavaScript:@"targetMessage.description"].UTF8String, "NetworkConnectionToWebProcess_HasStorageAccess");
    EXPECT_EQ([webView stringByEvaluatingJavaScript:@"targetMessage.arguments.length"].intValue, 4);
    EXPECT_STREQ([webView stringByEvaluatingJavaScript:@"targetMessage.arguments[0].type"].UTF8String, "RegistrableDomain");
    EXPECT_STREQ([webView stringByEvaluatingJavaScript:@"targetMessage.arguments[0].value"].UTF8String, "ipctestingapi.com");
    EXPECT_STREQ([webView stringByEvaluatingJavaScript:@"targetMessage.arguments[1].type"].UTF8String, "RegistrableDomain");
    EXPECT_STREQ([webView stringByEvaluatingJavaScript:@"targetMessage.arguments[1].value"].UTF8String, "webkit.org");
    EXPECT_STREQ([webView stringByEvaluatingJavaScript:@"targetMessage.arguments[2].type"].UTF8String, "uint64_t");
    EXPECT_STREQ([webView stringByEvaluatingJavaScript:@"targetMessage.arguments[2].value"].UTF8String, [webView stringByEvaluatingJavaScript:@"IPC.frameID.toString()"].UTF8String);
    EXPECT_STREQ([webView stringByEvaluatingJavaScript:@"targetMessage.arguments[3].type"].UTF8String, "uint64_t");
    EXPECT_EQ([webView stringByEvaluatingJavaScript:@"targetMessage.arguments[3].value"].intValue, [webView stringByEvaluatingJavaScript:@"IPC.pageID.toString()"].intValue);
    EXPECT_STREQ([webView stringByEvaluatingJavaScript:@"typeof(targetMessage.syncRequestID)"].UTF8String, "undefined");
    EXPECT_EQ([webView stringByEvaluatingJavaScript:@"targetMessage.destinationID"].intValue, 0);
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
    EXPECT_EQ([webView stringByEvaluatingJavaScript:@"messages[0].destinationID"].intValue,
        [webView stringByEvaluatingJavaScript:@"IPC.webPageProxyID.toString()"].intValue);
}

#endif
