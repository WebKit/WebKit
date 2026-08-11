/*-------------------------------------------------------------------------
 * drawElements Quality Program Tester Core
 * ----------------------------------------
 *
 * Copyright 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief iOS Application Delegate.
 *//*--------------------------------------------------------------------*/

#import "tcuIOSAppDelegate.h"
#import "tcuEAGLView.h"
#import "tcuIOSViewController.h"

@implementation tcuIOSAppDelegate

@synthesize window = _window;
@synthesize viewController = _viewController;

- (BOOL)application:(UIApplication *)application
    didFinishLaunchingWithOptions:(NSDictionary *)launchOptions
{
    DE_UNREF(application && launchOptions);

    // Construct window.
    self.window =
        [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
    if (!self.window)
    {
        [self release];
        return NO;
    }

    self.window.backgroundColor = [UIColor whiteColor];

    // Create view controller.
    self.viewController = [tcuIOSViewController alloc];

    [self.window setRootViewController:self.viewController];

    [self.window makeKeyAndVisible];
    [self.window layoutSubviews];

    // Disable idle timer (keep screen on).
    [[UIApplication sharedApplication] setIdleTimerDisabled:YES];

    return YES;
}

- (void)applicationWillResignActive:(UIApplication *)application
{
    DE_UNREF(application);
    [self.viewController stopTestIteration];
}

- (void)applicationDidEnterBackground:(UIApplication *)application
{
    DE_UNREF(application);
}

- (void)applicationWillEnterForeground:(UIApplication *)application
{
    DE_UNREF(application);
}

- (void)applicationDidBecomeActive:(UIApplication *)application
{
    DE_UNREF(application);
    [self.viewController startTestIteration];
}

- (void)applicationWillTerminate:(UIApplication *)application
{
    DE_UNREF(application);
    [self.viewController stopTestIteration];
}

- (void)dealloc
{
    [_window release];
    [_viewController release];
    [super dealloc];
}

@end
