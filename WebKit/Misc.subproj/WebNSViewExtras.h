/*
    IFNSViewExtras.h
	Copyright (c) 2002, Apple, Inc. All rights reserved.
*/

#import <AppKit/AppKit.h>

@class IFWebView;

@interface NSView (IFExtensions)

- (NSView *) _IF_superviewWithName:(NSString *)viewName;
- (IFWebView *)_IF_parentWebView;

@end
