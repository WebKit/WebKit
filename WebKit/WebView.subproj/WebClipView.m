//
//  WebClipView.m
//  WebKit
//
//  Created by Darin Adler on Tue Sep 24 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import "WebClipView.h"

#import <WebKit/WebAssertions.h>

// WebClipView's entire reason for existing is to set the clip used by focus ring redrawing.
// There's no easy way to prevent the focus ring from drawing outside the passed-in clip rectangle
// because it expects to have to draw outside the bounds of the view it's being drawn for.
// But it looks for the enclosing clip view, which gives us a hook we can use to control it.
// The "additional clip" is a clip for focus ring redrawing.

// FIXME: Change terminology from "additional clip" to "focus ring clip".

@interface NSView (WebClipViewAdditions)
- (void)_web_renewGStateDeep;
@end

@implementation WebClipView

static BOOL NSViewHasFocusRingVisibleRect;

+ (void)initialize
{
    NSViewHasFocusRingVisibleRect = [NSView instancesRespondToSelector:@selector(_focusRingVisibleRect)];
}

- (void)resetAdditionalClip
{
    ASSERT(_haveAdditionalClip);
    _haveAdditionalClip = NO;
    if (!NSViewHasFocusRingVisibleRect) {
        [self _web_renewGStateDeep];
    }
}

- (void)setAdditionalClip:(NSRect)additionalClip
{
    ASSERT(!_haveAdditionalClip);
    _haveAdditionalClip = YES;
    _additionalClip = additionalClip;
    if (!NSViewHasFocusRingVisibleRect) {
        [self _web_renewGStateDeep];
    }
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
    if (_haveAdditionalClip && !NSViewHasFocusRingVisibleRect) {
        rect = NSIntersectionRect(rect, _additionalClip);
    }
    return rect;
}

- (NSRect)_focusRingVisibleRect
{
    NSRect rect = [self visibleRect];
    if (_haveAdditionalClip) {
        rect = NSIntersectionRect(rect, _additionalClip);
    }
    return rect;
}

@end

@implementation NSView (WebClipViewAdditions)

- (void)_web_renewGStateDeep
{
    [[self subviews] makeObjectsPerformSelector:@selector(_web_renewGStateDeep)];
    [self renewGState];
}
 
@end
