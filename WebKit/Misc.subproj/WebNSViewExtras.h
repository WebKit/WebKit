/*
    WebNSViewExtras.h
	Copyright (c) 2002, Apple, Inc. All rights reserved.
*/

#import <AppKit/AppKit.h>

@class WebView;

@interface NSView (WebExtras)

- (NSView *) _web_superviewWithName:(NSString *)viewName;
- (WebView *)_web_parentWebView;

@end
