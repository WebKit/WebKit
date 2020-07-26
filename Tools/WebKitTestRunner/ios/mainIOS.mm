/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#import "HIDEventGenerator.h"
#import "TestController.h"
#import "UIKitSPI.h"
#import <pal/spi/ios/GraphicsServicesSPI.h>

static int _argc;
static const char **_argv;

@interface WebKitTestRunnerApp : UIApplication {
    UIBackgroundTaskIdentifier backgroundTaskIdentifier;
}
@end

@implementation WebKitTestRunnerApp

- (void)_runTestController
{
    WTR::TestController controller(_argc, _argv);
}

- (void)applicationDidFinishLaunching:(NSNotification *)notification
{
    [self performSelectorOnMainThread:@selector(_runTestController) withObject:nil waitUntilDone:NO];
}

- (void)applicationDidEnterBackground:(UIApplication *)application
{
    RELEASE_ASSERT_NOT_REACHED();
}

- (GSKeyboardRef)GSKeyboardForHWLayout:(NSString *)layout forceRebuild:(BOOL)forceRebuild createIfNeeded:(BOOL)createIfNeeded
{
    auto keyboard = [super GSKeyboardForHWLayout:layout forceRebuild:forceRebuild createIfNeeded:createIfNeeded];
    WTFLogAlways("-[%@ GSKeyboardForHWLayout:%@ forceRebuild:%@ createIfNeeded:%@] => %p", self.class, layout, forceRebuild ? @"YES" : @"NO", createIfNeeded ? @"YES" : @"NO", keyboard);
    return keyboard;
}

- (void)handleKeyHIDEvent:(IOHIDEventRef)event
{
    {
        auto keyboard = [self _hardwareKeyboard:NO];
        WTFLogAlways("(Before): keyboard %p has modifier state: 0x%02x; layout:%@; type: %d", keyboard, GSKeyboardGetLiveModifierState(keyboard), (__bridge NSString *)GSKeyboardGetLayout(keyboard), GSKeyboardGetHWKeyboardType(keyboard));
    }
    [super handleKeyHIDEvent:event];
    {
        auto keyboard = [self _hardwareKeyboard:NO];
        WTFLogAlways("(After): keyboard %p has modifier state: 0x%02x; layout:%@; type: %d", keyboard, GSKeyboardGetLiveModifierState(keyboard), (__bridge NSString *)GSKeyboardGetLayout(keyboard), GSKeyboardGetHWKeyboardType(keyboard));
    }
}

- (void)_handleHIDEvent:(IOHIDEventRef)event
{
    [super _handleHIDEvent:event];

    [[HIDEventGenerator sharedHIDEventGenerator] markerEventReceived:event];
}

@end

int main(int argc, const char* argv[])
{
    _argc = argc;
    _argv = argv;

    @autoreleasepool {
        [[NSUserDefaults standardUserDefaults] setBool:YES forKey:@"WebKitLinkedOnOrAfterEverything"];
    }

    UIApplicationMain(argc, (char**)argv, @"WebKitTestRunnerApp", @"WebKitTestRunnerApp");
    return 0;
}
