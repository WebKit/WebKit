/*	WebHTMLViewPrivate.h
	Copyright 2002, Apple, Inc. All rights reserved.
        
        Private header file.  This file may reference classes (both ObjectiveC and C++)
        in WebCore.  Instances of this class are referenced by _private in 
        WebHTMLView.
*/

#import <WebKit/WebHTMLView.h>

@class WebBridge;
@class WebController;
@class WebFrame;

@interface WebHTMLViewPrivate : NSObject
{
@public
    NSPoint mouseDownPoint;
    BOOL needsLayout;
    BOOL needsToApplyStyles;
    BOOL canDragTo;
    BOOL canDragFrom;
    BOOL liveAllowsScrolling;
    BOOL inWindow;
    BOOL inNextValidKeyView;
    
    id savedSubviews;
    BOOL subviewsSetAside;

    NSDictionary *draggingImageElement;
    
    BOOL lastMouseOverElementWasNotNil;
}
@end

@interface WebHTMLView (WebPrivate)

- (void)_reset;
- (WebController *)_controller;
- (WebFrame *)_frame;
- (WebBridge *)_bridge;
- (void)_adjustFrames;

// Modifier (flagsChanged) tracking SPI
+ (void)_setModifierTrackingEnabled:(BOOL)enabled;
+ (BOOL)_modifierTrackingEnabled;
+ (void)_postFlagsChangedEvent:(NSEvent *)flagsChangedEvent;
- (NSDictionary *)_elementAtPoint:(NSPoint)point;
- (BOOL)_continueAfterClickPolicyForEvent: (NSEvent *)event;
- (void)_mouseOverElement:(NSDictionary *)elementInformation modifierFlags:(unsigned)modifierFlags;

- (void)_setAsideSubviews;
- (void)_restoreSubviews;

- (BOOL)_insideAnotherHTMLView;

@end
