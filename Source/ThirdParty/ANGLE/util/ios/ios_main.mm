//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// ios_main.mm: Alternative entry point for iOS executables that initializes UIKit before calling
// the default entry point.

#import <UIKit/UIKit.h>

#include <stdio.h>
#include <unistd.h>

static int original_argc;
static char *_Nullable *_Nullable original_argv = nullptr;

int main(int argc, char *_Nullable *_Nullable argv);

@interface AngleUtilAppDelegate : UIResponder <UIApplicationDelegate>

// IOSWindow.mm reaches the test's layer through the application delegate's window, so the scene
// delegate publishes its window here once the scene connects.
@property(nullable, nonatomic, strong) UIWindow *window;

@end

@interface AngleUtilSceneDelegate : UIResponder <UIWindowSceneDelegate>

@property(nullable, nonatomic, strong) UIWindow *window;

@end

@implementation AngleUtilSceneDelegate

@synthesize window;

- (void)runMain
{
    chdir([NSTemporaryDirectory() fileSystemRepresentation]);
    exit(main(original_argc, original_argv));
}

- (void)scene:(UIScene *_Nonnull)scene
    willConnectToSession:(UISceneSession *_Nonnull)session
                 options:(UISceneConnectionOptions *_Nonnull)connectionOptions
{
    self.window = [[UIWindow alloc] initWithWindowScene:(UIWindowScene *)scene];
    self.window.rootViewController = [[UIViewController alloc] initWithNibName:nil bundle:nil];
    [self.window makeKeyAndVisible];
    [UIApplication sharedApplication].delegate.window = self.window;
    // We need to return from this function before the scene finishes connecting, so call main in a
    // timer callback afterward.
    [NSTimer scheduledTimerWithTimeInterval:0
                                     target:self
                                   selector:@selector(runMain)
                                   userInfo:nil
                                    repeats:NO];
}

@end

@implementation AngleUtilAppDelegate

@synthesize window;

- (BOOL)application:(UIApplication *_Nonnull)application
    didFinishLaunchingWithOptions:(NSDictionary *_Nullable)launchOptions
{
    return YES;
}

@end

extern "C" int ios_main(int argc, char *_Nonnull *_Nonnull argv)
{
    original_argc = argc;
    original_argv = argv;
    return UIApplicationMain(argc, argv, nullptr, NSStringFromClass([AngleUtilAppDelegate class]));
}
