//
//  WebClipView.m
//  WebKit
//
//  Created by Darin Adler on Tue Sep 24 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import "WebClipView.h"

@implementation WebClipView

- (void)resetAdditionalClip
{
    _haveAdditionalClip = NO;
}

- (void)setAdditionalClip:(NSRect)additionalClip
{
    _haveAdditionalClip = YES;
    _additionalClip = additionalClip;
}

- (NSRect)visibleRect
{
    NSRect rect = [super visibleRect];
    if (_haveAdditionalClip) {
        rect = NSIntersectionRect(rect, _additionalClip);
    }
    return rect;
}

@end
