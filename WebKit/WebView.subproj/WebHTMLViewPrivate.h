/*	WebHTMLViewPrivate.h
	Copyright 2002, Apple, Inc. All rights reserved.
        
        Private header file.
*/

#import <WebKit/WebHTMLView.h>

@class DOMDocumentFragment;
@class DOMRange;
@class WebArchive;
@class WebBridge;
@class WebView;
@class WebFrame;
@class WebPluginController;

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

@interface WebHTMLView (WebPrivate)

- (void)_reset;
- (WebView *)_webView;
- (WebFrame *)_frame;
- (WebBridge *)_bridge;
- (WebDataSource *)_dataSource;

// Modifier (flagsChanged) tracking SPI
+ (void)_postFlagsChangedEvent:(NSEvent *)flagsChangedEvent;
- (void)_updateMouseoverWithFakeEvent;

- (void)_setAsideSubviews;
- (void)_restoreSubviews;

- (BOOL)_insideAnotherHTMLView;
- (void)_clearLastHitViewIfSelf;
- (void)_updateMouseoverWithEvent:(NSEvent *)event;

+ (NSArray *)_insertablePasteboardTypes;
+ (NSArray *)_selectionPasteboardTypes;
- (void)_writeSelectionToPasteboard:(NSPasteboard *)pasteboard;
- (DOMDocumentFragment *)_documentFragmentFromPasteboard:(NSPasteboard *)pasteboard;
- (WebArchive *)_selectedArchive:(NSString **)markupString;
- (NSData *)_selectedRTFData;
- (BOOL)_canDelete;
- (BOOL)_canPaste;
- (BOOL)_haveSelection;
- (void)_replaceSelectionWithPasteboard:(NSPasteboard *)pasteboard selectReplacement:(BOOL)selectReplacement;

- (void)_frameOrBoundsChanged;

- (NSImage *)_dragImageForLinkElement:(NSDictionary *)element;
- (BOOL)_handleMouseDragged:(NSEvent *)event;
- (void)_handleAutoscrollForMouseDragged:(NSEvent *)event;
- (BOOL)_mayStartDragWithMouseDragged:(NSEvent *)event;

- (WebPluginController *)_pluginController;

- (NSRect)_selectionRect;

- (void)_startAutoscrollTimer:(NSEvent *)event;
- (void)_stopAutoscrollTimer;

- (void)copy:(id)sender;
- (void)cut:(id)sender;
- (void)delete:(id)sender;
- (void)paste:(id)sender;
- (void)delete:(id)sender;

@end
