/*
    WebNSViewExtras.h
	Copyright (c) 2002, Apple, Inc. All rights reserved.
*/

#import <AppKit/AppKit.h>

@class WebView;

@interface NSView (WebExtras)

// Returns the nearest enclosing view of the given class, or nil if none.
- (NSView *)_web_superviewOfClass:(Class)class;

// Returns the first WebView superview. Only works if self is the WebView's document view.
- (WebView *)_web_parentWebView;

- (BOOL)_web_dragShouldBeginFromMouseDown: (NSEvent *)mouseDownEvent withExpiration:(NSDate *)expiration;

// FIXME: The following can be class methods

// Returns an array with NSURLPboardType, NSStringPboardType and NSFilenamesPboardType pasteboard types
- (NSArray *)_web_acceptableDragTypes;

// Finds the best URL from the NSURLPboardType, NSStringPboardType and NSFilenamesPboardType pasteboard types
// and gives priority to http and https URLs
- (NSURL *)_web_bestURLForDraggingInfo:(id <NSDraggingInfo>)sender;

// Convenience method. Returns NSDragOperationCopy if _web_bestURLForDraggingInfo doesn't return nil.
// Returns NSDragOperationNone otherwise.
- (NSDragOperation)_web_dragOperationForDraggingInfo:(id <NSDraggingInfo>)sender;
@end
