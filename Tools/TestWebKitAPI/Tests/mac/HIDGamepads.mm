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

#if USE(APPLE_INTERNAL_SDK)

#import "InstanceMethodSwizzler.h"
#import "PlatformUtilities.h"
#import "Test.h"
#import "TestURLSchemeHandler.h"
#import "TestWKWebView.h"
#import "VirtualGamepad.h"
#import <WebCore/GameControllerSoftLink.h>
#import <WebKit/WKProcessPoolPrivate.h>

@interface GamepadMessageHandler : NSObject <WKScriptMessageHandler>
@property (readonly, nonatomic) Vector<RetainPtr<NSString>> messages;
@end

static bool didReceiveMessage = false;

@implementation GamepadMessageHandler

- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message
{
    _messages.append(message.body);
    didReceiveMessage = true;
}
@end

namespace TestWebKitAPI {

static const char* mainBytes = R"GAMEPADRESOURCE(
<script>

function handleGamepadConnect(evt)
{
    window.webkit.messageHandlers.gamepad.postMessage(JSON.stringify(evt.gamepad.id));
}

function handleGamepadDisconnect(evt)
{
    window.webkit.messageHandlers.gamepad.postMessage("Disconnect: " + JSON.stringify(evt.gamepad.id));
}

addEventListener("gamepadconnected", handleGamepadConnect);
addEventListener("gamepaddisconnected", handleGamepadDisconnect);

</script>
)GAMEPADRESOURCE";

static NSWindow *keyWindowForTesting;
static NSWindow *getKeyWindowForTesting()
{
    return keyWindowForTesting;
}

TEST(Gamepad, Basic)
{
    [WebCore::getGCControllerClass() setShouldMonitorBackgroundEvents:YES];

    auto keyWindowSwizzler = makeUnique<InstanceMethodSwizzler>([NSApplication class], @selector(keyWindow), reinterpret_cast<IMP>(getKeyWindowForTesting));

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto messageHandler = adoptNS([[GamepadMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"gamepad"];

    auto schemeHandler = adoptNS([[TestURLSchemeHandler alloc] init]);
    [configuration setURLSchemeHandler:schemeHandler.get() forURLScheme:@"gamepad"];

    [schemeHandler setStartURLSchemeTaskHandler:^(WKWebView *, id<WKURLSchemeTask> task) {
        auto response = adoptNS([[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"text/html" expectedContentLength:0 textEncodingName:nil]);
        [task didReceiveResponse:response.get()];
        [task didReceiveData:[NSData dataWithBytes:mainBytes length:strlen(mainBytes)]];
        [task didFinish];
    }];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    keyWindowForTesting = [webView window];
    [webView.get().configuration.processPool _setUsesOnlyHIDGamepadProviderForTesting:YES];

    [webView synchronouslyLoadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"gamepad://host/main.html"]]];

    [[webView window] makeFirstResponder:webView.get()];

    // Resigning/reinstating the key window state triggers the "key window did change" notification that WKWebView currently
    // needs to convince it to monitor gamepad devices
    [[webView window] resignKeyWindow];
    [[webView window] makeKeyWindow];

    // Create a gamepad with a fake vendorID
    auto mapping = VirtualGamepad::steelSeriesNimbusMapping();
    mapping.vendorID = HIDVendorID::Fake;
    auto gamepad = makeUnique<VirtualGamepad>(mapping);
    while (![webView.get().configuration.processPool _numberOfConnectedGamepadsForTesting])
        Util::runFor(0.1_s);

    // Press a button on it, waiting for the web page to see it
    gamepad->setButtonValue(0, 1.0);
    gamepad->publishReport();

    Util::run(&didReceiveMessage);
    didReceiveMessage = false;

    EXPECT_EQ(messageHandler.get().messages.size(), 1u);

#if HAVE(WIDE_GAMECONTROLLER_SUPPORT)
    EXPECT_TRUE([messageHandler.get().messages[0] isEqualToString:@"\"Virtual Nimbus Extended Gamepad\""]);
#else
    EXPECT_TRUE([messageHandler.get().messages[0] isEqualToString:@"\"ffff-1420-Virtual Nimbus\""]);
#endif

    EXPECT_EQ([webView.get().configuration.processPool _numberOfConnectedGamepadsForTesting], 1u);
}

#if !HAVE(WIDE_GAMECONTROLLER_SUPPORT)
TEST(Gamepad, GCFVersusHID)
{
    auto keyWindowSwizzler = makeUnique<InstanceMethodSwizzler>([NSApplication class], @selector(keyWindow), reinterpret_cast<IMP>(getKeyWindowForTesting));

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto messageHandler = adoptNS([[GamepadMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"gamepad"];

    auto schemeHandler = adoptNS([[TestURLSchemeHandler alloc] init]);
    [configuration setURLSchemeHandler:schemeHandler.get() forURLScheme:@"gamepad"];

    [schemeHandler setStartURLSchemeTaskHandler:^(WKWebView *, id<WKURLSchemeTask> task) {
        auto response = adoptNS([[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"text/html" expectedContentLength:0 textEncodingName:nil]);
        [task didReceiveResponse:response.get()];
        [task didReceiveData:[NSData dataWithBytes:mainBytes length:strlen(mainBytes)]];
        [task didFinish];
    }];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    keyWindowForTesting = [webView window];
    [webView synchronouslyLoadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"gamepad://host/main.html"]]];

    [[webView window] makeFirstResponder:webView.get()];

    // Resigning/reinstating the key window state triggers the "key window did change" notification that WKWebView currently
    // needs to convince it to monitor gamepad devices
    [[webView window] resignKeyWindow];
    [[webView window] makeKeyWindow];

    static const size_t TestGamepadCount = 5;
    std::unique_ptr<VirtualGamepad> gamepads[TestGamepadCount];

    // Create gamepads that should go through GameController.framework
    gamepads[0] = makeUnique<VirtualGamepad>(VirtualGamepad::steelSeriesNimbusMapping());
    gamepads[1] = makeUnique<VirtualGamepad>(VirtualGamepad::microsoftXboxOneMapping());
    gamepads[2] = makeUnique<VirtualGamepad>(VirtualGamepad::sonyDualshock4Mapping());

    // Create one that should go through HID
    gamepads[3] = makeUnique<VirtualGamepad>(VirtualGamepad::shenzhenLongshengweiTechnologyGamepadMapping());

    // Create one that GameController framework tries to handle, but shouldn't:
    // Make sure HID handles it instead.
    gamepads[4] = makeUnique<VirtualGamepad>(VirtualGamepad::sunLightApplicationGenericNESMapping());

    while ([webView.get().configuration.processPool _numberOfConnectedGamepadsForTesting] != TestGamepadCount)
        Util::runFor(0.1_s);

    EXPECT_EQ([webView.get().configuration.processPool _numberOfConnectedHIDGamepadsForTesting], 2u);
    EXPECT_EQ([webView.get().configuration.processPool _numberOfConnectedGameControllerFrameworkGamepadsForTesting], 3u);

    // Press buttons on them, waiting for the web page to see them
    for (size_t i = 0; i < TestGamepadCount; ++i) {
        gamepads[i]->setButtonValue(0, 1.0);
        gamepads[i]->publishReport();
    }

    // The page messages back once for each controller
    while (messageHandler.get().messages.size() < TestGamepadCount) {
        didReceiveMessage = false;
        Util::run(&didReceiveMessage);
    }

    NSSet *expectedGamepadNames = [NSSet setWithArray:@[
        @"\"Virtual Nimbus Extended Gamepad\"",
        @"\"Virtual Dualshock4 Extended Gamepad\"",
        @"\"Virtual Xbox One Gamepad\"",
        @"\"Virtual Xbox One Extended Gamepad\"",
        @"\"79-11-Virtual Shenzhen Longshengwei Technology Gamepad\"",
        @"\"12bd-d015-Virtual Sun Light Application Generic NES Controller\""]];

    for (size_t i = 0; i < TestGamepadCount; ++i)
        EXPECT_TRUE([expectedGamepadNames containsObject:messageHandler.get().messages[i].get()]);

    EXPECT_EQ([webView.get().configuration.processPool _numberOfConnectedHIDGamepadsForTesting], 2u);
    EXPECT_EQ([webView.get().configuration.processPool _numberOfConnectedGameControllerFrameworkGamepadsForTesting], 3u);
}
#endif // !HAVE(WIDE_GAMECONTROLLER_SUPPORT)

static const char* pollGamepadStateFunction = R"GAMEPADRESOURCE(
var result = new Object();
var gamepads = navigator.getGamepads();
result.gamepadCount = gamepads.length;
result.gamepadButtons = new Array;
result.gamepadAxes = new Array;
result.gamepadMapping = new Array;

for (var i = 0; i < gamepads.length; ++i) {
    result.gamepadButtons[i] = new Array;
    for (var j = 0; j < gamepads[i].buttons.length; ++j)
        result.gamepadButtons[i][j] = gamepads[i].buttons[j].value;

    result.gamepadAxes[i] = new Array;
    for (var j = 0; j < gamepads[i].axes.length; ++j)
        result.gamepadAxes[i][j] = gamepads[i].axes[j];

    result.gamepadMapping[i] = gamepads[i].mapping;
}

return result;

)GAMEPADRESOURCE";

TEST(Gamepad, GamepadState)
{
#if HAVE(WIDE_GAMECONTROLLER_SUPPORT)
    NSString *expectedName = @"\"Virtual Shenzhen Longshengwei Technology Gamepad Gamepad\"";
    
    // This particular device we're emulating is kind of a mess;
    // It's understood differently by GameController.framework and our old HID code path.
    // That's fine. We're just looking for reliable changes.
    Vector<double> setButtons1 = { 1.0, 1.0, 1.0, 1.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
    Vector<double> expectedButtons1 = { 1.0, 1.0, 1.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
    Vector<double> setButtons2 = { 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0, 1.0, 1.0 };
    Vector<double> expectedButtons2 = { 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 1.0, 0.0, 1.0 };
#else
    NSString *expectedName = @"\"79-11-Virtual Shenzhen Longshengwei Technology Gamepad\"";
    
    Vector<double> setButtons1 = { 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
    Vector<double> expectedButtons1 = { 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
    Vector<double> setButtons2 = { 0.0, 1.0, 1.0, 1.0, 1.0, 1.0, 0.0, 0.0, 1.0, 1.0 };
    Vector<double> expectedButtons2 = { 0.0, 1.0, 1.0, 1.0, 1.0, 1.0, 0.0, 0.0, 1.0, 1.0 };
#endif

    [WebCore::getGCControllerClass() setShouldMonitorBackgroundEvents:YES];

    auto keyWindowSwizzler = makeUnique<InstanceMethodSwizzler>([NSApplication class], @selector(keyWindow), reinterpret_cast<IMP>(getKeyWindowForTesting));

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto messageHandler = adoptNS([[GamepadMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"gamepad"];

    auto schemeHandler = adoptNS([[TestURLSchemeHandler alloc] init]);
    [configuration setURLSchemeHandler:schemeHandler.get() forURLScheme:@"gamepad"];

    [schemeHandler setStartURLSchemeTaskHandler:^(WKWebView *, id<WKURLSchemeTask> task) {
        auto response = adoptNS([[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"text/html" expectedContentLength:0 textEncodingName:nil]);
        [task didReceiveResponse:response.get()];
        [task didReceiveData:[NSData dataWithBytes:mainBytes length:strlen(mainBytes)]];
        [task didFinish];
    }];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    keyWindowForTesting = [webView window];
    [webView synchronouslyLoadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"gamepad://host/main.html"]]];

    [[webView window] makeFirstResponder:webView.get()];

    // Resigning/reinstating the key window state triggers the "key window did change" notification that WKWebView currently
    // needs to convince it to monitor gamepad devices
    [[webView window] resignKeyWindow];
    [[webView window] makeKeyWindow];

    // Connect a gamepad and make it visible to the page
    auto gamepad = makeUnique<VirtualGamepad>(VirtualGamepad::shenzhenLongshengweiTechnologyGamepadMapping());
    while (![webView.get().configuration.processPool _numberOfConnectedGamepadsForTesting])
        Util::runFor(0.01_s);

    auto updateStateAndPublish = [&] (Vector<double>& setButtons) {
        for (size_t i = 0; i < setButtons.size(); ++i)
            gamepad->setButtonValue(i, setButtons[i]);
        gamepad->publishReport();
    };

    Vector<double> *expectedButtons = &expectedButtons1;
    updateStateAndPublish(setButtons1);

    // Wait for the page to tell us a gamepad connected
    Util::run(&didReceiveMessage);
    didReceiveMessage = false;

    EXPECT_EQ(messageHandler.get().messages.size(), 1u);
    EXPECT_TRUE([messageHandler.get().messages[0] isEqualToString:expectedName]);

    bool done = false;
    bool gotNewValues = false;
    auto resultBlock = [&] (id result, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_TRUE([result[@"gamepadCount"] isEqualToNumber:@(1)]);

        bool areEqual = true;
        
        for (size_t i = 0; i < expectedButtons->size(); ++i) {
            if (!WTF::areEssentiallyEqual([(NSNumber *)result[@"gamepadButtons"][0][i] doubleValue], (*expectedButtons)[i]))
                areEqual = false;
        }

        if (areEqual)
            gotNewValues = true;

        done = true;
    };

    // Change some buttons, polling state to confirm
    NSDate *start = [NSDate date];
    while (!gotNewValues) {
        [webView callAsyncJavaScript:@(pollGamepadStateFunction) arguments:nil inFrame:nil inContentWorld:WKContentWorld.pageWorld completionHandler:resultBlock];
        Util::run(&done);
        done = false;

        if ([[NSDate date] timeIntervalSinceDate:start] > 1.0)
            break;
    }
    EXPECT_TRUE(gotNewValues);
    gotNewValues = false;

    expectedButtons = &expectedButtons2;
    updateStateAndPublish(setButtons2);

    start = [NSDate date];
    while (!gotNewValues) {
        [webView callAsyncJavaScript:@(pollGamepadStateFunction) arguments:nil inFrame:nil inContentWorld:WKContentWorld.pageWorld completionHandler:resultBlock];
        Util::run(&done);
        done = false;

        if ([[NSDate date] timeIntervalSinceDate:start] > 1.0)
            break;
    }
    EXPECT_TRUE(gotNewValues);
    gotNewValues = false;

    // Disconnect the gamepad
    gamepad = nullptr;

    Util::run(&didReceiveMessage);
    didReceiveMessage = false;

    EXPECT_EQ(messageHandler.get().messages.size(), 2u);

    NSString *lastMessage = [NSString stringWithFormat:@"Disconnect: %@", expectedName];
    EXPECT_TRUE([messageHandler.get().messages[1] isEqualToString:lastMessage]);
}

TEST(Gamepad, Dualshock3Basic)
{
#if HAVE(WIDE_GAMECONTROLLER_SUPPORT)
    auto expectedHID = 0u;
    auto expectedGC = 1u;
    NSString *expectedName = @"\"Virtual Dualshock3 Extended Gamepad\"";
#else
    auto expectedHID = 1u;
    auto expectedGC = 0u;
    NSString *expectedName = @"\"54c-268-Virtual Dualshock3\"";
#endif

    [WebCore::getGCControllerClass() setShouldMonitorBackgroundEvents:YES];

    auto keyWindowSwizzler = makeUnique<InstanceMethodSwizzler>([NSApplication class], @selector(keyWindow), reinterpret_cast<IMP>(getKeyWindowForTesting));

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto messageHandler = adoptNS([[GamepadMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"gamepad"];

    auto schemeHandler = adoptNS([[TestURLSchemeHandler alloc] init]);
    [configuration setURLSchemeHandler:schemeHandler.get() forURLScheme:@"gamepad"];

    [schemeHandler setStartURLSchemeTaskHandler:^(WKWebView *, id<WKURLSchemeTask> task) {
        auto response = adoptNS([[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"text/html" expectedContentLength:0 textEncodingName:nil]);
        [task didReceiveResponse:response.get()];
        [task didReceiveData:[NSData dataWithBytes:mainBytes length:strlen(mainBytes)]];
        [task didFinish];
    }];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    keyWindowForTesting = [webView window];
    [webView synchronouslyLoadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"gamepad://host/main.html"]]];

    [[webView window] makeFirstResponder:webView.get()];

    // Resigning/reinstating the key window state triggers the "key window did change" notification that WKWebView currently
    // needs to convince it to monitor gamepad devices
    [[webView window] resignKeyWindow];
    [[webView window] makeKeyWindow];

    // Connect a gamepad and make it visible to the page
    auto gamepad = makeUnique<VirtualGamepad>(VirtualGamepad::sonyDualshock3Mapping());
    while (![webView.get().configuration.processPool _numberOfConnectedGamepadsForTesting])
        Util::runFor(0.01_s);

    EXPECT_EQ([webView.get().configuration.processPool _numberOfConnectedHIDGamepadsForTesting], expectedHID);
    EXPECT_EQ([webView.get().configuration.processPool _numberOfConnectedGameControllerFrameworkGamepadsForTesting], expectedGC);

    gamepad->setButtonValue(1, 0.75);
    gamepad->publishReport();

    // Wait for the page to tell us a gamepad connected
    Util::run(&didReceiveMessage);
    didReceiveMessage = false;

    EXPECT_EQ(messageHandler.get().messages.size(), 1u);
    EXPECT_TRUE([messageHandler.get().messages[0] isEqualToString:expectedName]);

    bool done = false;
    auto resultBlock = [&] (id result, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_TRUE([result[@"gamepadCount"] isEqualToNumber:@(1)]);
        EXPECT_EQ(((NSArray *)result[@"gamepadButtons"][0]).count, 17u);
        EXPECT_EQ(((NSArray *)result[@"gamepadAxes"][0]).count, 4u);

        done = true;
    };

    [webView callAsyncJavaScript:@(pollGamepadStateFunction) arguments:nil inFrame:nil inContentWorld:WKContentWorld.pageWorld completionHandler:resultBlock];

    Util::run(&done);
    didReceiveMessage = true;
}

// Disabled for now with GameController framework's handling because our virtual HID report isn't quite right
#if !HAVE(WIDE_GAMECONTROLLER_SUPPORT)
TEST(Gamepad, Stadia)
{
    [WebCore::getGCControllerClass() setShouldMonitorBackgroundEvents:YES];

    auto keyWindowSwizzler = makeUnique<InstanceMethodSwizzler>([NSApplication class], @selector(keyWindow), reinterpret_cast<IMP>(getKeyWindowForTesting));

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto messageHandler = adoptNS([[GamepadMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"gamepad"];

    auto schemeHandler = adoptNS([[TestURLSchemeHandler alloc] init]);
    [configuration setURLSchemeHandler:schemeHandler.get() forURLScheme:@"gamepad"];

    [schemeHandler setStartURLSchemeTaskHandler:^(WKWebView *, id<WKURLSchemeTask> task) {
        auto response = adoptNS([[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"text/html" expectedContentLength:0 textEncodingName:nil]);
        [task didReceiveResponse:response.get()];
        [task didReceiveData:[NSData dataWithBytes:mainBytes length:strlen(mainBytes)]];
        [task didFinish];
    }];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    keyWindowForTesting = [webView window];
    [webView synchronouslyLoadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"gamepad://host/main.html"]]];

    [[webView window] makeFirstResponder:webView.get()];

    // Resigning/reinstating the key window state triggers the "key window did change" notification that WKWebView currently
    // needs to convince it to monitor gamepad devices
    [[webView window] resignKeyWindow];
    [[webView window] makeKeyWindow];

    // Connect a gamepad and make it visible to the page
    auto gamepad = makeUnique<VirtualGamepad>(VirtualGamepad::googleStadiaMapping());
    while (![webView.get().configuration.processPool _numberOfConnectedGamepadsForTesting])
        Util::runFor(0.01_s);

    EXPECT_EQ([webView.get().configuration.processPool _numberOfConnectedHIDGamepadsForTesting], 1u);
    EXPECT_EQ([webView.get().configuration.processPool _numberOfConnectedGameControllerFrameworkGamepadsForTesting], 0u);

    gamepad->setButtonValue(0, 1.0);
    gamepad->setButtonValue(1, 1.0);
    gamepad->setButtonValue(2, 1.0);
    gamepad->setButtonValue(3, 1.0);
    gamepad->setButtonValue(4, 1.0);
    gamepad->setButtonValue(5, 1.0);
    gamepad->publishReport();

    // Wait for the page to tell us a gamepad connected
    Util::run(&didReceiveMessage);
    didReceiveMessage = false;

    EXPECT_EQ(messageHandler.get().messages.size(), 1u);
    EXPECT_TRUE([messageHandler.get().messages[0] isEqualToString:@"\"18d1-9400-Virtual Stadia\""]);

    bool done = false;
    auto resultBlock = [&] (id result, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_TRUE([result[@"gamepadCount"] isEqualToNumber:@(1)]);
        EXPECT_EQ(((NSArray *)result[@"gamepadButtons"][0]).count, 19u);
        EXPECT_EQ(((NSArray *)result[@"gamepadAxes"][0]).count, 4u);

        done = true;
    };

    [webView callAsyncJavaScript:@(pollGamepadStateFunction) arguments:nil inFrame:nil inContentWorld:WKContentWorld.pageWorld completionHandler:resultBlock];

    Util::run(&done);
    didReceiveMessage = true;
}
#endif // #if !HAVE(WIDE_GAMECONTROLLER_SUPPORT)

TEST(Gamepad, LogitechBasic)
{
#if HAVE(WIDE_GAMECONTROLLER_SUPPORT)
    auto expectedHID = 0u;
    auto expectedGC = 2u;
    auto expectedButtons = 17u;
    NSSet *expectedGamepadNames = [NSSet setWithArray:@[
        @"\"Virtual Logitech F310 Extended Gamepad\"",
        @"\"Virtual Logitech F710 Extended Gamepad\""]];
#else
    auto expectedHID = 2u;
    auto expectedGC = 0u;
    auto expectedButtons = 16u;
    NSSet *expectedGamepadNames = [NSSet setWithArray:@[
        @"\"46d-c216-Virtual Logitech F310\"",
        @"\"46d-c219-Virtual Logitech F710\""]];
#endif
    [WebCore::getGCControllerClass() setShouldMonitorBackgroundEvents:YES];

    auto keyWindowSwizzler = makeUnique<InstanceMethodSwizzler>([NSApplication class], @selector(keyWindow), reinterpret_cast<IMP>(getKeyWindowForTesting));

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto messageHandler = adoptNS([[GamepadMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"gamepad"];

    auto schemeHandler = adoptNS([[TestURLSchemeHandler alloc] init]);
    [configuration setURLSchemeHandler:schemeHandler.get() forURLScheme:@"gamepad"];

    [schemeHandler setStartURLSchemeTaskHandler:^(WKWebView *, id<WKURLSchemeTask> task) {
        auto response = adoptNS([[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"text/html" expectedContentLength:0 textEncodingName:nil]);
        [task didReceiveResponse:response.get()];
        [task didReceiveData:[NSData dataWithBytes:mainBytes length:strlen(mainBytes)]];
        [task didFinish];
    }];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    keyWindowForTesting = [webView window];
    [webView synchronouslyLoadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"gamepad://host/main.html"]]];

    [[webView window] makeFirstResponder:webView.get()];

    // Resigning/reinstating the key window state triggers the "key window did change" notification that WKWebView currently
    // needs to convince it to monitor gamepad devices
    [[webView window] resignKeyWindow];
    [[webView window] makeKeyWindow];
    [[webView window] makeFirstResponder:webView.get()];

    // Connect a gamepad and make it visible to the page
    auto gamepad1 = makeUnique<VirtualGamepad>(VirtualGamepad::logitechF310Mapping());
    auto gamepad2 = makeUnique<VirtualGamepad>(VirtualGamepad::logitechF710Mapping());
    while ([webView.get().configuration.processPool _numberOfConnectedGamepadsForTesting] != 2u)
        Util::runFor(0.01_s);

    EXPECT_EQ([webView.get().configuration.processPool _numberOfConnectedHIDGamepadsForTesting], expectedHID);
    EXPECT_EQ([webView.get().configuration.processPool _numberOfConnectedGameControllerFrameworkGamepadsForTesting], expectedGC);

    gamepad1->setButtonValue(0, 1.0);
    gamepad2->setButtonValue(0, 1.0);
    gamepad1->publishReport();
    gamepad2->publishReport();

    // The page messages back once for each controller
    while (messageHandler.get().messages.size() < 2) {
        didReceiveMessage = false;
        Util::run(&didReceiveMessage);
    }

    EXPECT_EQ(messageHandler.get().messages.size(), 2u);

    for (size_t i = 0; i < 2; ++i)
        EXPECT_TRUE([expectedGamepadNames containsObject:messageHandler.get().messages[i].get()]);

    bool done = false;
    auto resultBlock = [&] (id result, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_TRUE([result[@"gamepadCount"] isEqualToNumber:@(2)]);
        EXPECT_EQ(((NSArray *)result[@"gamepadButtons"][0]).count, expectedButtons);
        EXPECT_EQ(((NSArray *)result[@"gamepadButtons"][1]).count, expectedButtons);
        EXPECT_EQ(((NSArray *)result[@"gamepadAxes"][0]).count, 4u);
        EXPECT_EQ(((NSArray *)result[@"gamepadAxes"][1]).count, 4u);

        done = true;
    };

    [webView callAsyncJavaScript:@(pollGamepadStateFunction) arguments:nil inFrame:nil inContentWorld:WKContentWorld.pageWorld completionHandler:resultBlock];

    Util::run(&done);
    didReceiveMessage = true;
}

TEST(Gamepad, FullInfoAfterConnection)
{
    [WebCore::getGCControllerClass() setShouldMonitorBackgroundEvents:YES];

    auto keyWindowSwizzler = makeUnique<InstanceMethodSwizzler>([NSApplication class], @selector(keyWindow), reinterpret_cast<IMP>(getKeyWindowForTesting));

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto messageHandler = adoptNS([[GamepadMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"gamepad"];

    auto schemeHandler = adoptNS([[TestURLSchemeHandler alloc] init]);
    [configuration setURLSchemeHandler:schemeHandler.get() forURLScheme:@"gamepad"];

    [schemeHandler setStartURLSchemeTaskHandler:^(WKWebView *, id<WKURLSchemeTask> task) {
        auto response = adoptNS([[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"text/html" expectedContentLength:0 textEncodingName:nil]);
        [task didReceiveResponse:response.get()];
        [task didReceiveData:[NSData dataWithBytes:mainBytes length:strlen(mainBytes)]];
        [task didFinish];
    }];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    keyWindowForTesting = [webView window];
    [webView synchronouslyLoadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"gamepad://host/main.html"]]];

    [[webView window] makeFirstResponder:webView.get()];

    // Resigning/reinstating the key window state triggers the "key window did change" notification that WKWebView currently
    // needs to convince it to monitor gamepad devices
    [[webView window] resignKeyWindow];
    [[webView window] makeKeyWindow];

    // Connect a gamepad and make it visible to the page
    auto gamepad1 = makeUnique<VirtualGamepad>(VirtualGamepad::logitechF310Mapping());
    while ([webView.get().configuration.processPool _numberOfConnectedGamepadsForTesting] != 1u)
        Util::runFor(0.01_s);

    // Wait until the page sees the gamepad
    while (messageHandler.get().messages.size() < 1) {
        didReceiveMessage = false;
        gamepad1->setButtonValue(0, 1.0);
        gamepad1->publishReport();
        Util::runFor(0.01_s);
        gamepad1->setButtonValue(0, 0.0);
        gamepad1->publishReport();
        Util::runFor(0.01_s);
    }

    bool done = false;
    auto resultBlock = [&] (id result, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_TRUE([result[@"gamepadCount"] isEqualToNumber:@(1)]);
        EXPECT_TRUE([result[@"gamepadMapping"][0] isEqualToString:@"standard"]);
        done = true;
    };

    // Verify the gamepad has the expected mapping.
    [webView callAsyncJavaScript:@(pollGamepadStateFunction) arguments:nil inFrame:nil inContentWorld:WKContentWorld.pageWorld completionHandler:resultBlock];
    Util::run(&done);
    done = false;

    // Make a second web view to make the same gamepad visible to it.
    auto webView2 = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    keyWindowForTesting = [webView2 window];
    [webView2 synchronouslyLoadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"gamepad://host/main.html"]]];
    [[webView2 window] makeFirstResponder:webView2.get()];

    // The gamepad is already connected, but this new web view can't see it.
    // Press buttons to make it visible.
    while (messageHandler.get().messages.size() < 2) {
        didReceiveMessage = false;
        gamepad1->setButtonValue(0, 1.0);
        gamepad1->publishReport();
        Util::runFor(0.01_s);
        gamepad1->setButtonValue(0, 0.0);
        gamepad1->publishReport();
        Util::runFor(0.01_s);
    }

    // Verify the gamepad has the expected mapping in the second web view.
    [webView2 callAsyncJavaScript:@(pollGamepadStateFunction) arguments:nil inFrame:nil inContentWorld:WKContentWorld.pageWorld completionHandler:resultBlock];
    Util::run(&done);
    done = false;
}


} // namespace TestWebKitAPI

#endif // USE(APPLE_INTERNAL_SDK)
