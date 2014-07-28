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

#import "TestController.h"


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
    /* Apps will get suspended or killed some time after entering the background state but we want to be able to run multiple copies of DumpRenderTree. Periodically check to see if our remaining background time dips below a threshold and create a new background task.
    */
    void (^expirationHandler)() = ^ {
        [application endBackgroundTask:backgroundTaskIdentifier];
        backgroundTaskIdentifier = UIBackgroundTaskInvalid;
    };

    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{

        NSTimeInterval timeRemaining;
        while (true) {
            timeRemaining = [application backgroundTimeRemaining];
            if (timeRemaining <= 10.0 || backgroundTaskIdentifier == UIBackgroundTaskInvalid) {
                [application endBackgroundTask:backgroundTaskIdentifier];
                backgroundTaskIdentifier = [application beginBackgroundTaskWithExpirationHandler:expirationHandler];
            }
            sleep(5);
        }
    });
}

@end

int main(int argc, const char* argv[])
{
    _argc = argc;
    _argv = argv;

    UIApplicationMain(argc, (char**)argv, @"WebKitTestRunnerApp", @"WebKitTestRunnerApp");
    return 0;
}
