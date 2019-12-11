/*
 *  Copyright 2014 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "APPRTCAppDelegate.h"
#import "APPRTCViewController.h"
#import <WebRTC/RTCSSLAdapter.h>

@interface APPRTCAppDelegate () <NSWindowDelegate>
@end

@implementation APPRTCAppDelegate {
  APPRTCViewController* _viewController;
  NSWindow* _window;
}

#pragma mark - NSApplicationDelegate

- (void)applicationDidFinishLaunching:(NSNotification*)notification {
  RTCInitializeSSL();
  NSScreen* screen = [NSScreen mainScreen];
  NSRect visibleRect = [screen visibleFrame];
  NSRect windowRect = NSMakeRect(NSMidX(visibleRect),
                                 NSMidY(visibleRect),
                                 1320,
                                 1140);
  NSUInteger styleMask = NSTitledWindowMask | NSClosableWindowMask;
  _window = [[NSWindow alloc] initWithContentRect:windowRect
                                        styleMask:styleMask
                                          backing:NSBackingStoreBuffered
                                            defer:NO];
  _window.delegate = self;
  [_window makeKeyAndOrderFront:self];
  [_window makeMainWindow];
  _viewController = [[APPRTCViewController alloc] initWithNibName:nil
                                                           bundle:nil];
  [_window setContentView:[_viewController view]];
}

#pragma mark - NSWindow

- (void)windowWillClose:(NSNotification*)notification {
  [_viewController windowWillClose:notification];
  RTCCleanupSSL();
  [NSApp terminate:self];
}

@end

