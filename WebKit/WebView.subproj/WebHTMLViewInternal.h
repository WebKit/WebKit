// Things internal to the WebKit framework; not SPI.

#import <WebKit/WebHTMLViewPrivate.h>

@class WebTextCompleteController;

@interface WebHTMLViewPrivate : NSObject
{
@public
    BOOL needsLayout;
    BOOL needsToApplyStyles;
    BOOL inWindow;
    BOOL showsURLsInToolTips;
    BOOL ignoringMouseDraggedEvents;
    BOOL printing;
    BOOL initiatedDrag;
    // Is WebCore handling drag destination duties (DHTML dragging)?
    BOOL webCoreHandlingDrag;
    NSDragOperation webCoreDragOp;
    // Offset from lower left corner of dragged image to mouse location (when we're the drag source)
    NSPoint dragOffset;
    
    id savedSubviews;
    BOOL subviewsSetAside;

    NSEvent *mouseDownEvent; // Kept after handling the event.
    BOOL handlingMouseDownEvent;
    NSEvent *keyDownEvent; // Kept only during handling of the event.

    NSURL *draggingImageURL;
    unsigned int dragSourceActionMask;
    
    NSSize lastLayoutSize;
    NSSize lastLayoutFrameSize;
    BOOL laidOutAtLeastOnce;
    
    NSPoint lastScrollPosition;

    WebPluginController *pluginController;
    
    NSString *toolTip;
    id trackingRectOwner;
    void *trackingRectUserData;
    
    NSTimer *autoscrollTimer;
    NSEvent *autoscrollTriggerEvent;
    
    NSArray* pageRects;

    BOOL resigningFirstResponder;
    BOOL ignoreMarkedTextSelectionChange;
    BOOL startNewKillRingSequence;
    BOOL nextResponderDisabledOnce;
    
    WebTextCompleteController *compController;
    
    BOOL transparentBackground;

    NSResponder *firstResponderAtMouseDownTime;
}
@end

@interface WebHTMLView (WebInternal)
- (void)_selectionChanged;
- (void)_updateFontPanel;
- (unsigned int)_delegateDragSourceActionMask;
- (BOOL)_canSmartCopyOrDelete;
- (BOOL)_wasFirstResponderAtMouseDownTime:(NSResponder *)responder;
@end
