/*
    WebNSViewExtras.h
	Copyright (c) 2002, Apple, Inc. All rights reserved.
*/

#import <AppKit/AppKit.h>

#define WebDragImageAlpha    0.75

@class WebView;

@interface NSView (WebExtras)

// Returns the nearest enclosing view of the given class, or nil if none.
- (NSView *)_web_superviewOfClass:(Class)class;

// Returns the nearest enclosing view of the given class, or nil if none.
// Stops searching and returns nil when limitClass is encountered
- (NSView *)_web_superviewOfClass:(Class)class stoppingAtClass:(Class)limitClass;

// Returns the first WebView superview. Only works if self is the WebView's document view.
- (WebView *)_web_parentWebView;

// returns whether a drag should begin starting with mouseDownEvent; if the time
// passes expiration or the mouse moves less than the hysteresis before the mouseUp event,
// returns NO, else returns YES.
- (BOOL)_web_dragShouldBeginFromMouseDown:(NSEvent *)mouseDownEvent
                           withExpiration:(NSDate *)expiration
                              xHysteresis:(float)xHysteresis
                              yHysteresis:(float)yHysteresis;

// Calls _web_dragShouldBeginFromMouseDown:withExpiration:xHysteresis:yHysteresis: with
// the default values for xHysteresis and yHysteresis
- (BOOL)_web_dragShouldBeginFromMouseDown:(NSEvent *)mouseDownEvent
                           withExpiration:(NSDate *)expiration;

// Convenience method. Returns NSDragOperationCopy if _web_bestURLFromPasteboard doesn't return nil.
// Returns NSDragOperationNone otherwise.
- (NSDragOperation)_web_dragOperationForDraggingInfo:(id <NSDraggingInfo>)sender;

// Resizes and applies alpha to image, extends pboard and sets drag origins for dragging promised image files.
- (void)_web_dragPromisedImage:(NSImage *)image
                          rect:(NSRect)rect
                           URL:(NSURL *)URL
                      fileType:(NSString *)fileType
                         title:(NSString *)title
                         event:(NSEvent *)event;

@end
