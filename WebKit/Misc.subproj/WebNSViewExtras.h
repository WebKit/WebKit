/*
    WebNSViewExtras.h
    Copyright (c) 2002, Apple, Inc. All rights reserved.
*/

#import <AppKit/AppKit.h>

#define WebDragImageAlpha    0.75

@class WebArchive;
@class WebBridge;
@class WebDataSource;
@class WebFrame;
@class WebFrameView;
@class WebImageRenderer;
@class WebView;

@interface NSView (WebExtras)

// Returns the nearest enclosing view of the given class, or nil if none.
- (NSView *)_web_superviewOfClass:(Class)class;

// Returns the nearest enclosing view of the given class, or nil if none.
// Stops searching and returns nil when limitClass is encountered
- (NSView *)_web_superviewOfClass:(Class)class stoppingAtClass:(Class)limitClass;

// Returns the first WebFrameView superview. Only works if self is the WebFrameView's document view.
- (WebFrameView *)_web_parentWebFrameView;
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

// Resizes and applies alpha to image and drags it.
- (void)_web_dragImage:(WebImageRenderer *)image
                  rect:(NSRect)rect
                 event:(NSEvent *)event
            pasteboard:(NSPasteboard *)pasteboard 
                source:(id)source
                offset:(NSPoint *)dragImageOffset;

- (BOOL)_web_firstResponderIsSelfOrDescendantView;
- (BOOL)_web_firstResponderCausesFocusDisplay;

@end

@interface NSView (WebDocumentViewExtras)

- (WebView *)_webView;
- (WebFrame *)_frame;
- (WebBridge *)_bridge;
- (WebDataSource *)_dataSource;

@end
