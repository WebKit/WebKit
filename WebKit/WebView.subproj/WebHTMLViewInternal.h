// Things internal to the WebKit framework; not SPI.

#import <WebKit/WebHTMLViewPrivate.h>

@interface WebHTMLViewPrivate : NSObject
{
@public
    BOOL needsLayout;
    BOOL needsToApplyStyles;
    BOOL inWindow;
    BOOL inNextValidKeyView;
    BOOL ignoringMouseDraggedEvents;
    BOOL printing;
    BOOL initiatedDrag;
    // Is WebCore handling drag destination duties (DHTML dragging)?
    BOOL webCoreHandlingDrag;
    
    id savedSubviews;
    BOOL subviewsSetAside;

    NSEvent *mouseDownEvent;

    NSURL *draggingImageURL;
    
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
}
@end

@interface WebHTMLView (WebInternal)
- (void)_updateFontPanel;
@end
