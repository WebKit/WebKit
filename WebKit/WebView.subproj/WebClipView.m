//
//  WebClipView.m
//  WebKit
//
//  Created by Darin Adler on Tue Sep 24 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import "WebClipView.h"

#import <WebKit/WebAssertions.h>

@implementation WebClipView

- (void)resetAdditionalClip
{
    ASSERT(_haveAdditionalClip);
    _haveAdditionalClip = NO;
}

- (void)setAdditionalClip:(NSRect)additionalClip
{
    ASSERT(!_haveAdditionalClip);
    _haveAdditionalClip = YES;
    _additionalClip = additionalClip;
}

- (BOOL)hasAdditionalClip
{
    return _haveAdditionalClip;
}

- (NSRect)additionalClip
{
    ASSERT(_haveAdditionalClip);
    return _additionalClip;
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
