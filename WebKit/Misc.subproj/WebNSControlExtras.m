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
    float heightBefore = [self frame].size.height;
    
    [self sizeToFit];
    
    NSRect frame = [self frame];
    float heightDelta = frame.size.height - heightBefore;
    
    frame.origin.y -= heightDelta;
    [self setFrameOrigin:frame.origin];
    
    NSRect windowFrame = [[self window] frame];
    windowFrame.size.height += heightDelta;
    [[self window] setFrame:windowFrame display:NO];
}

@end
