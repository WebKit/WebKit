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
#import "TestURLSchemeHandler.h"
#import "TestWKWebView.h"
#import "VirtualGamepad.h"
#import <WebKit/WKProcessPoolPrivate.h>

@interface GamepadMessageHandler : NSObject <WKScriptMessageHandler>
@end

static bool didReceiveMessage = false;

@implementation GamepadMessageHandler

- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message
{
    EXPECT_TRUE([message.body isEqualToString:@"\"ffff-1420-Virtual Nimbus\""]);
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

addEventListener("gamepadconnected", handleGamepadConnect);

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

    // Create a gamepad
    auto gamepad = VirtualGamepad::makeVirtualNimbus();
    while (![webView.get().configuration.processPool _numberOfConnectedGamepadsForTesting])
        Util::sleep(0.1);

    // Press a button on it, waiting for the web page to see it
    gamepad->setButtonValue(0, 1.0);
    gamepad->publishReport();

    Util::run(&didReceiveMessage);
    didReceiveMessage = false;
}

} // namespace TestWebKitAPI

#endif // HAVE(HID_FRAMEWORK) && USE(APPLE_INTERNAL_SDK)
