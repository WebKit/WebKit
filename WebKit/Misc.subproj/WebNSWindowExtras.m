//
//  WebNSWindowExtras.m
//  WebKit
//
//  Created by Darin Adler on Tue Oct 22 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import "WebNSWindowExtras.h"

@implementation NSWindow (WebExtras)

- (void)centerOverMainWindow
{
    NSRect frameToCenterOver;
    if ([NSApp mainWindow]) {
        frameToCenterOver = [[NSApp mainWindow] frame];
    } else {
        frameToCenterOver = [[NSScreen mainScreen] visibleFrame];
    }
    
    NSSize size = [self frame].size;
    NSPoint origin;
    origin.y = NSMaxY(frameToCenterOver)
        - (frameToCenterOver.size.height - size.height) / 3
        - size.height;
    origin.x = frameToCenterOver.origin.x
        + (frameToCenterOver.size.width - size.width) / 2;
    [self setFrameOrigin:origin];
}

@end
