/*
    WebNSViewExtras.h
	Copyright (c) 2002, Apple, Inc. All rights reserved.
*/

#import <AppKit/AppKit.h>

#define DragImageAlpha    		0.75
#define MaxDragImageSize 		NSMakeSize(400, 400)

@class WebView;

@interface NSView (WebExtras)

// Returns the nearest enclosing view of the given class, or nil if none.
- (NSView *)_web_superviewOfClass:(Class)class;

// Returns the first WebView superview. Only works if self is the WebView's document view.
- (WebView *)_web_parentWebView;

- (BOOL)_web_dragShouldBeginFromMouseDown: (NSEvent *)mouseDownEvent withExpiration:(NSDate *)expiration;

// Convenience method. Returns NSDragOperationCopy if _web_bestURLFromPasteboard doesn't return nil.
// Returns NSDragOperationNone otherwise.
- (NSDragOperation)_web_dragOperationForDraggingInfo:(id <NSDraggingInfo>)sender;

// Resizes and applies alpha to image, extends pboard and sets drag origins for dragging promised images.
// Only call from within your overidden dragImage:at:offset:event:pasteboard:source:slideBack: method
// after calling dragPromisedFilesOfTypes:fromRect:source:slideBack:event:
// Only really needed by WebImageView and WebHTMLView.
- (void)_web_setPromisedImageDragImage:(NSImage **)dragImage
                                    at:(NSPoint *)imageLoc
                                offset:(NSSize *)mouseOffset
                         andPasteboard:(NSPasteboard *)pboard
                             withImage:(NSImage *)image
                              andEvent:(NSEvent *)theEvent;

@end
