//
//  WebDynamicScrollBarsView.h
//  WebKit
//
//  Created by John Sullivan on Tue Jan 22 2002.
//  Copyright (c) 2001, 2002 Apple Computer, Inc. All rights reserved.
//

#import <WebCore/WebCoreFrameView.h>
#import <WebCore/WebCoreScrollView.h>

// FIXME 2980779: This has grown to be more than just a dynamic scroll bar view,
// and it is no longer completely appropriate for use outside of WebKit.

@interface WebDynamicScrollBarsView : WebCoreScrollView <WebCoreFrameView>
{
    WebCoreScrollBarMode hScroll;
    WebCoreScrollBarMode vScroll;
    BOOL suppressLayout;
    BOOL suppressScrollers;
    BOOL inUpdateScrollers;
}

- (void)setAllowsHorizontalScrolling:(BOOL)flag;
- (BOOL)allowsHorizontalScrolling;
- (void)setAllowsVerticalScrolling:(BOOL)flag;
- (BOOL)allowsVerticalScrolling;

// Convenience method to affect both scrolling directions at once.
- (void)setAllowsScrolling:(BOOL)flag;

// Returns YES if either horizontal or vertical scrolling is allowed.
- (BOOL)allowsScrolling;

- (void)updateScrollers;
- (void)setSuppressLayout: (BOOL)flag;
@end
