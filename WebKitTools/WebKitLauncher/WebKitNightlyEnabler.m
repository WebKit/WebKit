/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
 * Copyright (C) 2006 Graham Dennis.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import <Cocoa/Cocoa.h>

static void enableWebKitNightlyBehaviour() __attribute__ ((constructor));

static NSString *WKNERunState = @"WKNERunState";
static NSString *WKNEShouldMonitorShutdowns = @"WKNEShouldMonitorShutdowns";

typedef enum {
    RunStateShutDown,
    RunStateInitializing,
    RunStateRunning
} WKNERunStates;

static bool extensionBundlesWereLoaded = NO;
static NSSet *extensionPaths = nil;

static void myBundleDidLoad(CFNotificationCenterRef center, void *observer, CFStringRef name, const void *object, CFDictionaryRef userInfo)
{
    // Break out early if we have already detected an extension
    if (extensionBundlesWereLoaded)
        return;

    NSBundle *bundle = (NSBundle *)object;
    NSString *bundlePath = [[bundle bundlePath] stringByAbbreviatingWithTildeInPath];
    NSString *bundleFileName = [bundlePath lastPathComponent];

    // Explicitly ignore SIMBL.bundle, as its only purpose is to load extensions
    // on a per-application basis.  It's presence indicates a user has application
    // extensions, but not that any will be loaded into Safari
    if ([bundleFileName isEqualToString:@"SIMBL.bundle"])
        return;

    // If the bundle lives inside a known extension path, flag it as an extension
    NSEnumerator *e = [extensionPaths objectEnumerator];
    NSString *path = nil;
    while (path = [e nextObject]) {
        if ([bundlePath length] < [path length])
            continue;

        if ([[bundlePath substringToIndex:[path length]] isEqualToString:path]) {
            extensionBundlesWereLoaded = YES;
            break;
        }
    }
}

static void myApplicationWillFinishLaunching(CFNotificationCenterRef center, void *observer, CFStringRef name, const void *object, CFDictionaryRef userInfo)
{
    CFNotificationCenterRemoveObserver(CFNotificationCenterGetLocalCenter(), &myApplicationWillFinishLaunching, NULL, NULL);
    CFNotificationCenterRemoveObserver(CFNotificationCenterGetLocalCenter(), &myBundleDidLoad, NULL, NULL);
    [extensionPaths release];
    extensionPaths = nil;

    NSUserDefaults *userDefaults = [NSUserDefaults standardUserDefaults];
    [userDefaults setInteger:RunStateRunning forKey:WKNERunState];
    [userDefaults synchronize];

    if (extensionBundlesWereLoaded)
        NSRunInformationalAlertPanel(@"Safari extensions detected",
                                     @"Safari extensions were detected on your system.  Extensions are incompatible with nightly builds of WebKit, and may cause crashes or incorrect behavior.  Please disable them if you experience such behavior.", @"Continue",
                                     nil, nil);
}

static void myApplicationWillTerminate(CFNotificationCenterRef center, void *observer, CFStringRef name, const void *object, CFDictionaryRef userInfo)
{
    NSUserDefaults *userDefaults = [NSUserDefaults standardUserDefaults];
    [userDefaults setInteger:RunStateShutDown forKey:WKNERunState];
    [userDefaults synchronize];
}

extern char **_CFGetProcessPath() __attribute__((weak));

static void poseAsWebKitApp()
{
    char *webKitAppPath = getenv("WebKitAppPath");
    if (!webKitAppPath || !_CFGetProcessPath)
        return;

    // Set up the main bundle early so it points at Safari.app
    CFBundleGetMainBundle();

    // Fiddle with CoreFoundation to have it pick up the executable path as being within WebKit.app
    char **processPath = _CFGetProcessPath();
    *processPath = NULL;
    setenv("CFProcessPath", webKitAppPath, 1);
    _CFGetProcessPath();

    // Clean up
    unsetenv("CFProcessPath");
    unsetenv("WebKitAppPath");
}

static void enableWebKitNightlyBehaviour()
{
    unsetenv("DYLD_INSERT_LIBRARIES");
    poseAsWebKitApp();

    extensionPaths = [[NSSet alloc] initWithObjects:@"~/Library/InputManagers/", @"/Library/InputManagers/",
                                                    @"~/Library/Application Support/SIMBL/Plugins/", @"/Library/Application Support/SIMBL/Plugins/",
                                                    @"~/Library/Application Enhancers/", @"/Library/Application Enhancers/",
                                                    nil];

    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    NSUserDefaults *userDefaults = [NSUserDefaults standardUserDefaults];
    NSDictionary *defaultPrefs = [NSDictionary dictionaryWithObjectsAndKeys:[NSNumber numberWithBool:RunStateShutDown], WKNERunState,
                                                                            [NSNumber numberWithBool:YES], WKNEShouldMonitorShutdowns, nil];
    [userDefaults registerDefaults:defaultPrefs];
    if ([userDefaults boolForKey:WKNEShouldMonitorShutdowns]) {
        WKNERunStates savedState = (WKNERunStates)[userDefaults integerForKey:WKNERunState];
        if (savedState == RunStateInitializing) {
            // Use CoreFoundation here as AppKit hasn't been initialized at this stage of Safari's lifetime
            CFOptionFlags responseFlags;
            CFUserNotificationDisplayAlert(0, kCFUserNotificationCautionAlertLevel,
                                           NULL, NULL, NULL,
                                           CFSTR("WebKit failed to open correctly"),
                                           CFSTR("WebKit failed to open correctly on your previous attempt. Please disable any Safari extensions that you may have installed.  If the problem continues to occur, please file a bug report at http://webkit.org/quality/reporting.html"), 
                                           CFSTR("Continue"), NULL, NULL, &responseFlags);
        }
        else if (savedState == RunStateRunning) {
            NSLog(@"WebKit failed to shut down cleanly.  Checking for Safari extensions.");
            CFNotificationCenterAddObserver(CFNotificationCenterGetLocalCenter(), &myBundleDidLoad,
                                            myBundleDidLoad, (CFStringRef) NSBundleDidLoadNotification,
                                            NULL, CFNotificationSuspensionBehaviorDeliverImmediately);
        }
    }
    [userDefaults setInteger:RunStateInitializing forKey:WKNERunState];
    [userDefaults synchronize];

    CFNotificationCenterAddObserver(CFNotificationCenterGetLocalCenter(), &myApplicationWillFinishLaunching,
                                    myApplicationWillFinishLaunching, (CFStringRef) NSApplicationWillFinishLaunchingNotification,
                                    NULL, CFNotificationSuspensionBehaviorDeliverImmediately);
    CFNotificationCenterAddObserver(CFNotificationCenterGetLocalCenter(), &myApplicationWillTerminate,
                                    myApplicationWillTerminate, (CFStringRef) NSApplicationWillTerminateNotification,
                                    NULL, CFNotificationSuspensionBehaviorDeliverImmediately);
    [pool release];
}
