//
//  IFDynamicScrollBarsView.h
//  WebKit
//
//  Created by John Sullivan on Tue Jan 22 2002.
//  Copyright (c) 2001, 2002 Apple Computer, Inc. All rights reserved.
//

#import <Cocoa/Cocoa.h>

#import <WebCoreFrameView.h>

// FIXME: This has grown to be more than just a dynamic scroll bar view.
// We want to rename this class at least, and perhaps break it up into
// more than one class.

@interface IFDynamicScrollBarsView : NSScrollView <WebCoreFrameView>
{
    NSCursor *cursor;
    BOOL breakRecursionCycle;
    BOOL allowsScrolling;
}

- (void)setAllowsScrolling: (BOOL)flag;
- (BOOL)allowsScrolling;
- (void)updateScrollers;
- (void)resetCursorRects;

@end
