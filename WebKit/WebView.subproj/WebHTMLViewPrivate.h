/*	WebHTMLViewPrivate.h
	Copyright 2002, Apple, Inc. All rights reserved.
        
        Private header file.  This file may reference classes (both ObjectiveC and C++)
        in WebCore.  Instances of this class are referenced by _private in 
        WebHTMLView.
*/

#import <WebKit/WebHTMLView.h>

@class WebBridge;
@class WebController;

@interface WebHTMLViewPrivate : NSObject
{
@public
    WebController *controller;
    BOOL needsLayout;
    BOOL needsToApplyStyles;
    BOOL canDragTo;
    BOOL canDragFrom;
    NSCursor *cursor;
    BOOL liveAllowsScrolling;
    BOOL inWindow;
}
@end

@interface WebHTMLView (WebPrivate)
- (void)_reset;
- (WebController *)_controller;
- (void)_setController: (WebController *)controller;
- (WebBridge *)_bridge;
- (void)_adjustFrames;

// Modifier (flagsChanged) tracking SPI
+ (void)_setModifierTrackingEnabled:(BOOL)enabled;
+ (BOOL)_modifierTrackingEnabled;
+ (void)_postFlagsChangedEvent:(NSEvent *)flagsChangedEvent;
- (NSDictionary *)_elementInfoAtPoint:(NSPoint)point;
@end
