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
#import "UIKitSPIForTesting.h"
#import <WebKit/WKProcessPoolPrivate.h>
#import <wtf/RetainPtr.h>

static int _argc;
static const char **_argv;

static NSString * const sceneConfigurationName = @"Default Configuration";

@interface WebKitTestRunnerSceneDelegate : NSObject <UIWindowSceneDelegate>
@end

@implementation WebKitTestRunnerSceneDelegate

- (void)_runTestController
{
    WTR::TestController controller(_argc, _argv);
}

- (void)scene:(UIScene *)scene willConnectToSession:(UISceneSession *)session options:(UISceneConnectionOptions *)connectionOptions
{
    [self performSelectorOnMainThread:@selector(_runTestController) withObject:nil waitUntilDone:NO];
}

- (void)sceneDidEnterBackground:(UIScene *)scene
{
    RELEASE_ASSERT_NOT_REACHED();
}

@end

@interface WebKitTestRunnerApp : UIApplication {
    UIBackgroundTaskIdentifier backgroundTaskIdentifier;
}
@end

@implementation WebKitTestRunnerApp

- (UISceneConfiguration *)application:(UIApplication *)application configurationForConnectingSceneSession:(UISceneSession *)connectingSceneSession options:(UISceneConnectionOptions *)options
{
    RetainPtr configuration = adoptNS([[UISceneConfiguration alloc] initWithName:sceneConfigurationName sessionRole:connectingSceneSession.role]);
    [configuration setDelegateClass:WebKitTestRunnerSceneDelegate.class];
    return configuration.autorelease();
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

    [WKProcessPool _setLinkedOnOrAfterEverythingForTesting];
    [WKProcessPool _crashOnMessageCheckFailureForTesting];

    UIApplicationMain(argc, (char**)argv, @"WebKitTestRunnerApp", @"WebKitTestRunnerApp");
    return 0;
}
