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

#if HAVE(HID_FRAMEWORK) && USE(APPLE_INTERNAL_SDK)

#import "InstanceMethodSwizzler.h"
#import "PlatformUtilities.h"
#import "Test.h"
#import "TestURLSchemeHandler.h"
#import "TestWKWebView.h"
#import "VirtualGamepad.h"
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
        Util::sleep(0.1);

    // Press a button on it, waiting for the web page to see it
    gamepad->setButtonValue(0, 1.0);
    gamepad->publishReport();

    Util::run(&didReceiveMessage);
    didReceiveMessage = false;

    EXPECT_EQ(messageHandler.get().messages.size(), 1u);
    EXPECT_TRUE([messageHandler.get().messages[0] isEqualToString:@"\"ffff-1420-Virtual Nimbus\""]);
    EXPECT_EQ([webView.get().configuration.processPool _numberOfConnectedGamepadsForTesting], 1u);
}

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
        Util::sleep(0.1);

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
        @"\"79-11-Virtual Shenzhen Longshengwei Technology Gamepad\"",
        @"\"12bd-d015-Virtual Sun Light Application Generic NES Controller\""]];

    for (size_t i = 0; i < TestGamepadCount; ++i)
        EXPECT_TRUE([expectedGamepadNames containsObject:messageHandler.get().messages[i].get()]);

    EXPECT_EQ([webView.get().configuration.processPool _numberOfConnectedHIDGamepadsForTesting], 2u);
    EXPECT_EQ([webView.get().configuration.processPool _numberOfConnectedGameControllerFrameworkGamepadsForTesting], 3u);
}

static const char* pollGamepadStateFunction = R"GAMEPADRESOURCE(
var result = new Object();
var gamepads = navigator.getGamepads();
result.gamepadCount = gamepads.length;
result.gamepadButtons = new Array;
result.gamepadAxes = new Array;

for (var i = 0; i < gamepads.length; ++i) {
    result.gamepadButtons[i] = new Array;
    for (var j = 0; j < gamepads[i].buttons.length; ++j)
        result.gamepadButtons[i][j] = gamepads[i].buttons[j].value;

    result.gamepadAxes[i] = new Array;
    for (var j = 0; j < gamepads[i].axes.length; ++j)
        result.gamepadAxes[i][j] = gamepads[i].axes[j];
}

return result;

)GAMEPADRESOURCE";

TEST(Gamepad, GamepadState)
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

    // Connect a gamepad and make it visible to the page
    auto gamepad = makeUnique<VirtualGamepad>(VirtualGamepad::shenzhenLongshengweiTechnologyGamepadMapping());
    while (![webView.get().configuration.processPool _numberOfConnectedGamepadsForTesting])
        Util::sleep(0.01);

    Vector<double> expectedButtons = { 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
    Vector<double> expectedAxes = { -0.9921568627450981, -0.003921568627450966, -0.003921568627450966, -0.003921568627450966, -0.003921568627450966 };

    auto updateStateAndPublish = [&] {
        for (size_t i = 0; i < expectedButtons.size(); ++i)
            gamepad->setButtonValue(i, expectedButtons[i]);
        for (size_t i = 0; i < 5; ++i)
            gamepad->setAxisValue(i, expectedAxes[i]);
        gamepad->publishReport();
    };

    updateStateAndPublish();

    // Wait for the page to tell us a gamepad connected
    Util::run(&didReceiveMessage);
    didReceiveMessage = false;

    EXPECT_EQ(messageHandler.get().messages.size(), 1u);
    EXPECT_TRUE([messageHandler.get().messages[0] isEqualToString:@"\"79-11-Virtual Shenzhen Longshengwei Technology Gamepad\""]);

    bool done = false;
    bool gotNewValues = false;
    auto resultBlock = [&] (id result, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_TRUE([result[@"gamepadCount"] isEqualToNumber:@(1)]);

        bool areEqual = true;

        for (size_t i = 0; i < 10; ++i) {
            if (!WTF::areEssentiallyEqual([(NSNumber *)result[@"gamepadButtons"][0][i] doubleValue], expectedButtons[i]))
                areEqual = false;
        }

        for (size_t i = 0; i < 5; ++i) {
            if (!WTF::areEssentiallyEqual([(NSNumber *)result[@"gamepadAxes"][0][i] doubleValue], expectedAxes[i]))
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

    expectedButtons = { 0.0, 1.0, 1.0, 1.0, 1.0, 1.0, 0.0, 0.0, 1.0, 1.0 };
    updateStateAndPublish();

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

    expectedAxes = { -1.0, -1.0, -1.0, -1.0, 1.0 };
    updateStateAndPublish();

    // Even though we set -1.0 for each "X" axis, the first 3 axes on this controller are always constant
    expectedAxes = { -0.9921568627450981, -0.003921568627450966, -0.003921568627450966, -1.0, 1.0 };

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
    EXPECT_TRUE([messageHandler.get().messages[1] isEqualToString:@"Disconnect: \"79-11-Virtual Shenzhen Longshengwei Technology Gamepad\""]);
}

TEST(Gamepad, Dualshock3Basic)
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

    // Connect a gamepad and make it visible to the page
    auto gamepad = makeUnique<VirtualGamepad>(VirtualGamepad::sonyDualshock3Mapping());
    while (![webView.get().configuration.processPool _numberOfConnectedGamepadsForTesting])
        Util::sleep(0.01);

    EXPECT_EQ([webView.get().configuration.processPool _numberOfConnectedHIDGamepadsForTesting], 1u);
    EXPECT_EQ([webView.get().configuration.processPool _numberOfConnectedGameControllerFrameworkGamepadsForTesting], 0u);

    gamepad->setButtonValue(0, 0.75);
    gamepad->publishReport();

    // Wait for the page to tell us a gamepad connected
    Util::run(&didReceiveMessage);
    didReceiveMessage = false;

    EXPECT_EQ(messageHandler.get().messages.size(), 1u);
    EXPECT_TRUE([messageHandler.get().messages[0] isEqualToString:@"\"54c-268-Virtual Dualshock3\""]);

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

TEST(Gamepad, Stadia)
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

    // Connect a gamepad and make it visible to the page
    auto gamepad = makeUnique<VirtualGamepad>(VirtualGamepad::googleStadiaMapping());
    while (![webView.get().configuration.processPool _numberOfConnectedGamepadsForTesting])
        Util::sleep(0.01);

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

} // namespace TestWebKitAPI

#endif // HAVE(HID_FRAMEWORK) && USE(APPLE_INTERNAL_SDK)
