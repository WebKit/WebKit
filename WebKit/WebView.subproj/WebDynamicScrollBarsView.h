//
//  DynamicScrollBarsView.h
//  WebBrowser
//
//  Created by John Sullivan on Tue Jan 22 2002.
//  Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
//

#import <Cocoa/Cocoa.h>

@interface IFDynamicScrollBarsView : NSScrollView 
{
    NSCursor *cursor;
    BOOL breakRecursionCycle;
    BOOL allowsScrolling;
}

- (void)setAllowsScrolling: (BOOL)flag;
- (BOOL)allowsScrolling;
- (void)updateScrollers;
- (void)setCursor:(NSCursor *)cur;
- (void)resetCursorRects;

@end
