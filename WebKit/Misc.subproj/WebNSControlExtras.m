//
//  WebNSControlExtras.m
//  WebKit
//
//  Created by Darin Adler on Tue Oct 22 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import "WebNSControlExtras.h"


@implementation NSControl (WebExtras)

- (void)sizeToFitAndAdjustWindowHeight
{
    NSRect frame = [self frame];

    NSSize bestSize = [[self cell] cellSizeForBounds:NSMakeRect(0, 0, frame.size.width, 10000.0)];
    
    float heightDelta = bestSize.height - frame.size.height;

    frame.size.height += heightDelta;
    frame.origin.y    -= heightDelta;
    [self setFrame:frame];

    NSRect windowFrame = [[self window] frame];
    windowFrame.size.height += heightDelta;
    [[self window] setFrame:windowFrame display:NO];
}

@end
