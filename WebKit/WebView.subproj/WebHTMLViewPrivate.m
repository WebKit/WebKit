/*	
    WebHTMLViewPrivate.m
    Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebHTMLViewPrivate.h>

#import <AppKit/NSResponder_Private.h>

#import <WebKit/WebAssertions.h>
#import <WebKit/WebBridge.h>
#import <WebKit/WebFramePrivate.h>
#import <WebKit/WebFrameViewPrivate.h>
#import <WebKit/WebImageRenderer.h>
#import <WebKit/WebKitNSStringExtras.h>
#import <WebKit/WebNSImageExtras.h>
#import <WebKit/WebNSPasteboardExtras.h>
#import <WebKit/WebNSURLExtras.h>
#import <WebKit/WebNSViewExtras.h>
#import <WebKit/WebPluginController.h>
#import <WebKit/WebPreferences.h>
#import <WebKit/WebStringTruncator.h>
#import <WebKit/WebViewPrivate.h>

// These are a little larger than typical because dragging links is a fairly
// advanced feature that can confuse non-power-users
#define DragHysteresis  		10.0
#define TextDragHysteresis  		3.0
#define TextDragDelay			0.15

#define AUTOSCROLL_INTERVAL             0.1

#define DRAG_LABEL_BORDER_X		4.0
#define DRAG_LABEL_BORDER_Y		2.0
#define DRAG_LABEL_RADIUS		5.0
#define DRAG_LABEL_BORDER_Y_OFFSET              2.0

#define MIN_DRAG_LABEL_WIDTH_BEFORE_CLIP	120.0
#define MAX_DRAG_LABEL_WIDTH                    320.0

#define DRAG_LINK_LABEL_FONT_SIZE   11.0
#define DRAG_LINK_URL_FONT_SIZE   10.0

// Any non-zero value will do, but using somethign recognizable might help us debug some day.
#define TRACKING_RECT_TAG 0xBADFACE

static BOOL forceRealHitTest = NO;

@interface NSView (AppKitSecretsIKnowAbout)
- (void)_recursiveDisplayRectIfNeededIgnoringOpacity:(NSRect)rect isVisibleRect:(BOOL)isVisibleRect rectIsVisibleRectForView:(NSView *)visibleView topView:(BOOL)topView;
- (void)_recursiveDisplayAllDirtyWithLockFocus:(BOOL)needsLockFocus visRect:(NSRect)visRect;
- (NSRect)_dirtyRect;
@end

@interface NSView (WebHTMLViewPrivate)
- (void)_web_propagateDirtyRectToAncestor;
@end

@interface NSMutableDictionary (WebHTMLViewPrivate)
- (void)_web_setObjectIfNotNil:(id)object forKey:(id)key;
@end

@implementation WebHTMLViewPrivate

- (void)dealloc
{
    ASSERT(autoscrollTimer == nil);
    
    [pluginController destroyAllPlugins];
    
    [mouseDownEvent release];
    [draggingImageURL release];
    [pluginController release];
    [toolTip release];
    
    [super dealloc];
}

@end


@implementation WebHTMLView (WebPrivate)

- (void)_adjustFrames
{
    // Ick!  khtml set the frame size during layout and
    // the frame origins during drawing!  So we have to 
    // layout and do a draw with rendering disabled to
    // correclty adjust the frames.
    [[self _bridge] adjustFrames:[self frame]];
}

- (void)_reset
{
    [WebImageRenderer stopAnimationsInView:self];
}

- (WebView *)_webView
{
    return (WebView *)[self _web_superviewOfClass:[WebView class]];
}

- (WebFrame *)_frame
{
    WebFrameView *webFrameView = [self _web_parentWebFrameView];
    return [webFrameView webFrame];
}

// Required so view can access the part's selection.
- (WebBridge *)_bridge
{
    return [[self _frame] _bridge];
}

+ (void)_postFlagsChangedEvent:(NSEvent *)flagsChangedEvent
{
    NSEvent *fakeEvent = [NSEvent mouseEventWithType:NSMouseMoved
        location:[[flagsChangedEvent window] convertScreenToBase:[NSEvent mouseLocation]]
        modifierFlags:[flagsChangedEvent modifierFlags]
        timestamp:[flagsChangedEvent timestamp]
        windowNumber:[flagsChangedEvent windowNumber]
        context:[flagsChangedEvent context]
        eventNumber:0 clickCount:0 pressure:0];

    // Pretend it's a mouse move.
    [[NSNotificationCenter defaultCenter]
        postNotificationName:NSMouseMovedNotification object:self
        userInfo:[NSDictionary dictionaryWithObject:fakeEvent forKey:@"NSEvent"]];
}

- (void)_updateMouseoverWithFakeEvent
{
    NSEvent *fakeEvent = [NSEvent mouseEventWithType:NSMouseMoved
        location:[[self window] convertScreenToBase:[NSEvent mouseLocation]]
        modifierFlags:[[NSApp currentEvent] modifierFlags]
        timestamp:[NSDate timeIntervalSinceReferenceDate]
        windowNumber:[[self window] windowNumber]
        context:[[NSApp currentEvent] context]
        eventNumber:0 clickCount:0 pressure:0];
    
    [self _updateMouseoverWithEvent:fakeEvent];
}

- (void)_frameOrBoundsChanged
{
    if (!NSEqualSizes(_private->lastLayoutSize, [(NSClipView *)[self superview] documentVisibleRect].size)) {
        [self setNeedsLayout:YES];
        [self setNeedsDisplay:YES];
    }

    SEL selector = @selector(_updateMouseoverWithFakeEvent);
    [NSObject cancelPreviousPerformRequestsWithTarget:self selector:selector object:nil];
    [self performSelector:selector withObject:nil afterDelay:0];
}

- (NSDictionary *)_elementAtPoint:(NSPoint)point
{
    NSDictionary *elementInfoWC = [[self _bridge] elementAtPoint:point];
    NSMutableDictionary *elementInfo = [elementInfoWC mutableCopy];

    // Convert URL strings to NSURLs
    [elementInfo _web_setObjectIfNotNil:[NSURL _web_URLWithDataAsString:[elementInfoWC objectForKey:WebElementLinkURLKey]] forKey:WebElementLinkURLKey];
    [elementInfo _web_setObjectIfNotNil:[NSURL _web_URLWithDataAsString:[elementInfoWC objectForKey:WebElementImageURLKey]] forKey:WebElementImageURLKey];
    
    WebFrameView *webFrameView = [self _web_parentWebFrameView];
    ASSERT(webFrameView);
    WebFrame *webFrame = [webFrameView webFrame];
    
    if (webFrame) {
        NSString *frameName = [elementInfoWC objectForKey:WebElementLinkTargetFrameKey];
        if ([frameName length] == 0) {
            [elementInfo setObject:webFrame forKey:WebElementLinkTargetFrameKey];
        } else {
            WebFrame *wf = [webFrame findFrameNamed:frameName];
            if (wf != nil)
                [elementInfo setObject:wf forKey:WebElementLinkTargetFrameKey];
            else
                [elementInfo removeObjectForKey:WebElementLinkTargetFrameKey];
        }
    
        [elementInfo setObject:webFrame forKey:WebElementFrameKey];
    }
    
    return [elementInfo autorelease];
}

- (void)_setAsideSubviews
{
    ASSERT(!_private->subviewsSetAside);
    ASSERT(_private->savedSubviews == nil);
    _private->savedSubviews = _subviews;
    _subviews = nil;
    _private->subviewsSetAside = YES;
 }
 
 - (void)_restoreSubviews
 {
    ASSERT(_private->subviewsSetAside);
    ASSERT(_subviews == nil);
    _subviews = _private->savedSubviews;
    _private->savedSubviews = nil;
    _private->subviewsSetAside = NO;
}

// Don't let AppKit even draw subviews. We take care of that.
- (void)_recursiveDisplayRectIfNeededIgnoringOpacity:(NSRect)rect isVisibleRect:(BOOL)isVisibleRect rectIsVisibleRectForView:(NSView *)visibleView topView:(BOOL)topView
{
    // The _setDrawsDescendants: mechanism now takes care of this for us.
    // But only in AppKit-722 and newer.
    if (NSAppKitVersionNumber < 722) {
        [_subviews makeObjectsPerformSelector:@selector(_web_propagateDirtyRectToAncestor)];
    }
    [self _setAsideSubviews];
    [super _recursiveDisplayRectIfNeededIgnoringOpacity:rect isVisibleRect:isVisibleRect
        rectIsVisibleRectForView:visibleView topView:topView];
    [self _restoreSubviews];
}

// Don't let AppKit even draw subviews. We take care of that.
- (void)_recursiveDisplayAllDirtyWithLockFocus:(BOOL)needsLockFocus visRect:(NSRect)visRect
{
    BOOL needToSetAsideSubviews = !_private->subviewsSetAside;
    
    if (needToSetAsideSubviews) {
        // The _setDrawsDescendants: mechanism now takes care of this for us.
        // But only in AppKit-722 and newer.
        if (NSAppKitVersionNumber < 722) {
            [_subviews makeObjectsPerformSelector:@selector(_web_propagateDirtyRectToAncestor)];
        }
        [self _setAsideSubviews];
    }
    
    [super _recursiveDisplayAllDirtyWithLockFocus:needsLockFocus visRect:visRect];
    
    if (needToSetAsideSubviews) {
        [self _restoreSubviews];
    }
}

- (BOOL)_insideAnotherHTMLView
{
    NSView *view = self;
    while ((view = [view superview])) {
        if ([view isKindOfClass:[WebHTMLView class]]) {
            return YES;
        }
    }
    return NO;
}

- (void)scrollPoint:(NSPoint)point
{
    // Since we can't subclass NSTextView to do what we want, we have to second guess it here.
    // If we get called during the handling of a key down event, we assume the call came from
    // NSTextView, and ignore it and use our own code to decide how to page up and page down
    // We are smarter about how far to scroll, and we have "superview scrolling" logic.
    NSEvent *event = [[self window] currentEvent];
    if ([event type] == NSKeyDown) {
        const unichar pageUp = NSPageUpFunctionKey;
        if ([[event characters] rangeOfString:[NSString stringWithCharacters:&pageUp length:1]].length == 1) {
            [self tryToPerform:@selector(scrollPageUp:) with:nil];
            return;
        }
        const unichar pageDown = NSPageDownFunctionKey;
        if ([[event characters] rangeOfString:[NSString stringWithCharacters:&pageDown length:1]].length == 1) {
            [self tryToPerform:@selector(scrollPageDown:) with:nil];
            return;
        }
    }
    
    [super scrollPoint:point];
}

- (NSView *)hitTest:(NSPoint)point
{
    // WebHTMLView objects handle all left mouse clicks for objects inside them.
    // That does not include left mouse clicks with the control key held down.
    BOOL captureHitsOnSubviews;
    if (forceRealHitTest) {
        captureHitsOnSubviews = NO;
    } else {
        NSEvent *event = [[self window] currentEvent];
        captureHitsOnSubviews = [event type] == NSLeftMouseDown && ([event modifierFlags] & NSControlKeyMask) == 0;
    }
    if (!captureHitsOnSubviews) {
        return [super hitTest:point];
    }
    if ([[self superview] mouse:point inRect:[self frame]]) {
        return self;
    }
    return nil;
}

static WebHTMLView *lastHitView = nil;

- (void)_clearLastHitViewIfSelf
{
    if (lastHitView == self) {
	lastHitView = nil;
    }
}

- (NSTrackingRectTag)addTrackingRect:(NSRect)rect owner:(id)owner userData:(void *)data assumeInside:(BOOL)assumeInside
{
    ASSERT(_private->trackingRectOwner == nil);
    _private->trackingRectOwner = owner;
    _private->trackingRectUserData = data;
    return TRACKING_RECT_TAG;
}

- (NSTrackingRectTag)_addTrackingRect:(NSRect)rect owner:(id)owner userData:(void *)data assumeInside:(BOOL)assumeInside useTrackingNum:(int)tag
{
    ASSERT(tag == TRACKING_RECT_TAG);
    return [self addTrackingRect:rect owner:owner userData:data assumeInside:assumeInside];
}

- (void)removeTrackingRect:(NSTrackingRectTag)tag
{
    ASSERT(tag == TRACKING_RECT_TAG);
    if (_private != nil) {
        ASSERT(_private->trackingRectOwner != nil);
        _private->trackingRectOwner = nil;
    }
}

- (void)_sendToolTipMouseExited
{
    // Nothing matters except window, trackingNumber, and userData.
    NSEvent *fakeEvent = [NSEvent enterExitEventWithType:NSMouseExited
        location:NSMakePoint(0, 0)
        modifierFlags:0
        timestamp:0
        windowNumber:[[self window] windowNumber]
        context:NULL
        eventNumber:0
        trackingNumber:TRACKING_RECT_TAG
        userData:_private->trackingRectUserData];
    [_private->trackingRectOwner mouseExited:fakeEvent];
}

- (void)_sendToolTipMouseEntered
{
    // Nothing matters except window, trackingNumber, and userData.
    NSEvent *fakeEvent = [NSEvent enterExitEventWithType:NSMouseEntered
        location:NSMakePoint(0, 0)
        modifierFlags:0
        timestamp:0
        windowNumber:[[self window] windowNumber]
        context:NULL
        eventNumber:0
        trackingNumber:TRACKING_RECT_TAG
        userData:_private->trackingRectUserData];
    [_private->trackingRectOwner mouseEntered:fakeEvent];
}

- (void)_setToolTip:(NSString *)string
{
    NSString *toolTip = [string length] == 0 ? nil : string;
    NSString *oldToolTip = _private->toolTip;
    if ((toolTip == nil || oldToolTip == nil) ? toolTip == oldToolTip : [toolTip isEqualToString:oldToolTip]) {
        return;
    }
    if (oldToolTip) {
        [self _sendToolTipMouseExited];
        [oldToolTip release];
    }
    _private->toolTip = [toolTip copy];
    if (toolTip) {
        if (_private->toolTipTag) {
            [self removeToolTip:_private->toolTipTag];
        }
        NSRect wideOpenRect = NSMakeRect(-100000, -100000, 200000, 200000);
        _private->toolTipTag = [self addToolTipRect:wideOpenRect owner:self userData:NULL];
        [self _sendToolTipMouseEntered];
    }
}

- (NSString *)view:(NSView *)view stringForToolTip:(NSToolTipTag)tag point:(NSPoint)point userData:(void *)data
{
    return [[_private->toolTip copy] autorelease];
}

- (void)_updateMouseoverWithEvent:(NSEvent *)event
{
    WebHTMLView *view = nil;
    if ([event window] == [self window]) {
        forceRealHitTest = YES;
        NSView *hitView = [[[self window] contentView] hitTest:[event locationInWindow]];
        forceRealHitTest = NO;
        while (hitView) {
            if ([hitView isKindOfClass:[WebHTMLView class]]) {
                view = (WebHTMLView *)hitView;
                break;
            }
            hitView = [hitView superview];
        }
    }

    if (lastHitView != view && lastHitView != nil) {
	// If we are moving out of a view (or frame), let's pretend the mouse moved
	// all the way out of that view. But we have to account for scrolling, because
	// khtml doesn't understand our clipping.
	NSRect visibleRect = [[[[lastHitView _frame] frameView] _scrollView] documentVisibleRect];
	float yScroll = visibleRect.origin.y;
	float xScroll = visibleRect.origin.x;

	event = [NSEvent mouseEventWithType:NSMouseMoved
			 location:NSMakePoint(-1 - xScroll, -1 - yScroll )
			 modifierFlags:[[NSApp currentEvent] modifierFlags]
			 timestamp:[NSDate timeIntervalSinceReferenceDate]
			 windowNumber:[[self window] windowNumber]
			 context:[[NSApp currentEvent] context]
			 eventNumber:0 clickCount:0 pressure:0];
	[[lastHitView _bridge] mouseMoved:event];
    }

    lastHitView = view;
    
    NSDictionary *element;
    if (view == nil) {
        element = nil;
    } else {
        [[view _bridge] mouseMoved:event];

        NSPoint point = [view convertPoint:[event locationInWindow] fromView:nil];
        element = [view _elementAtPoint:point];
    }

    // Have the web view send a message to the delegate so it can do status bar display.
    [[self _webView] _mouseDidMoveOverElement:element modifierFlags:[event modifierFlags]];

    // Set a tool tip; it won't show up right away but will if the user pauses.
    [self _setToolTip:[element objectForKey:WebCoreElementTitleKey]];
}

- (BOOL)_interceptKeyEvent:(NSEvent *)event toView:(NSView *)view
{
    return [[self _bridge] interceptKeyEvent:event toView:view];
}


+ (NSArray *)_pasteboardTypes
{
    return [NSArray arrayWithObjects:
#if SUPPORT_HTML_PBOARD
        NSHTMLPboardType,
#endif
        NSRTFPboardType, NSRTFDPboardType, NSStringPboardType, nil];
}

- (void)_writeSelectionToPasteboard:(NSPasteboard *)pasteboard
{
    [pasteboard declareTypes:[[self class] _pasteboardTypes] owner:nil];

#if SUPPORT_HTML_PBOARD
    // Put HTML on the pasteboard.
    [pasteboard setData:??? forType:NSHTMLPboardType];
#endif

    // Put attributed string on the pasteboard (RTF format).
    NSAttributedString *attributedString = [self selectedAttributedString];
    NSRange range = NSMakeRange(0, [attributedString length]);
    NSData *attributedData = [attributedString RTFFromRange:range documentAttributes:nil];
    [pasteboard setData:attributedData forType:NSRTFPboardType];

    attributedData = [attributedString RTFDFromRange:range documentAttributes:nil];
    [pasteboard setData:attributedData forType:NSRTFDPboardType];
    
    // Put plain string on the pasteboard.
    [pasteboard setString:[self selectedString] forType:NSStringPboardType];
}


-(NSImage *)_dragImageForLinkElement:(NSDictionary *)element
{
    NSURL *linkURL = [element objectForKey: WebElementLinkURLKey];

    BOOL drawURLString = YES;
    BOOL clipURLString = NO, clipLabelString = NO;
    
    NSString *label = [element objectForKey: WebElementLinkLabelKey];
    NSString *urlString = [linkURL _web_userVisibleString];
    
    if (!label) {
	drawURLString = NO;
	label = urlString;
    }
    
    NSFont *labelFont = [[NSFontManager sharedFontManager] convertFont:[NSFont systemFontOfSize:DRAG_LINK_LABEL_FONT_SIZE]
                                                   toHaveTrait:NSBoldFontMask];
    NSFont *urlFont = [NSFont systemFontOfSize: DRAG_LINK_URL_FONT_SIZE];
    NSSize labelSize;
    labelSize.width = [label _web_widthWithFont: labelFont];
    labelSize.height = [labelFont ascender] - [labelFont descender];
    if (labelSize.width > MAX_DRAG_LABEL_WIDTH){
        labelSize.width = MAX_DRAG_LABEL_WIDTH;
        clipLabelString = YES;
    }
    
    NSSize imageSize, urlStringSize;
    imageSize.width += labelSize.width + DRAG_LABEL_BORDER_X * 2;
    imageSize.height += labelSize.height + DRAG_LABEL_BORDER_Y * 2;
    if (drawURLString) {
	urlStringSize.width = [urlString _web_widthWithFont: urlFont];
        urlStringSize.height = [urlFont ascender] - [urlFont descender];
	imageSize.height += urlStringSize.height;
	if (urlStringSize.width > MAX_DRAG_LABEL_WIDTH) {
	    imageSize.width = MAX(MAX_DRAG_LABEL_WIDTH + DRAG_LABEL_BORDER_X * 2, MIN_DRAG_LABEL_WIDTH_BEFORE_CLIP);
	    clipURLString = YES;
	} else {
	    imageSize.width = MAX(labelSize.width + DRAG_LABEL_BORDER_X * 2, urlStringSize.width + DRAG_LABEL_BORDER_X * 2);
	}
    }
    NSImage *dragImage = [[[NSImage alloc] initWithSize: imageSize] autorelease];
    [dragImage lockFocus];
    
    [[NSColor colorWithCalibratedRed: 0.7 green: 0.7 blue: 0.7 alpha: 0.8] set];
    
    // Drag a rectangle with rounded corners/
    NSBezierPath *path = [NSBezierPath bezierPath];
    [path appendBezierPathWithOvalInRect: NSMakeRect(0,0, DRAG_LABEL_RADIUS * 2, DRAG_LABEL_RADIUS * 2)];
    [path appendBezierPathWithOvalInRect: NSMakeRect(0,imageSize.height - DRAG_LABEL_RADIUS * 2, DRAG_LABEL_RADIUS * 2, DRAG_LABEL_RADIUS * 2)];
    [path appendBezierPathWithOvalInRect: NSMakeRect(imageSize.width - DRAG_LABEL_RADIUS * 2, imageSize.height - DRAG_LABEL_RADIUS * 2, DRAG_LABEL_RADIUS * 2, DRAG_LABEL_RADIUS * 2)];
    [path appendBezierPathWithOvalInRect: NSMakeRect(imageSize.width - DRAG_LABEL_RADIUS * 2,0, DRAG_LABEL_RADIUS * 2, DRAG_LABEL_RADIUS * 2)];
    
    [path appendBezierPathWithRect: NSMakeRect(DRAG_LABEL_RADIUS, 0, imageSize.width - DRAG_LABEL_RADIUS * 2, imageSize.height)];
    [path appendBezierPathWithRect: NSMakeRect(0, DRAG_LABEL_RADIUS, DRAG_LABEL_RADIUS + 10, imageSize.height - 2 * DRAG_LABEL_RADIUS)];
    [path appendBezierPathWithRect: NSMakeRect(imageSize.width - DRAG_LABEL_RADIUS - 20,DRAG_LABEL_RADIUS, DRAG_LABEL_RADIUS + 20, imageSize.height - 2 * DRAG_LABEL_RADIUS)];
    [path fill];
        
    NSColor *topColor = [NSColor colorWithCalibratedWhite:0.0 alpha:0.75];
    NSColor *bottomColor = [NSColor colorWithCalibratedWhite:1.0 alpha:0.5];
    if (drawURLString) {
	if (clipURLString)
	    urlString = [WebStringTruncator centerTruncateString: urlString toWidth:imageSize.width - (DRAG_LABEL_BORDER_X * 2) withFont:urlFont];

        [urlString _web_drawDoubledAtPoint:NSMakePoint(DRAG_LABEL_BORDER_X, DRAG_LABEL_BORDER_Y - [urlFont descender]) 
             withTopColor:topColor bottomColor:bottomColor font:urlFont];
    }

    if (clipLabelString)
	    label = [WebStringTruncator rightTruncateString: label toWidth:imageSize.width - (DRAG_LABEL_BORDER_X * 2) withFont:labelFont];
    [label _web_drawDoubledAtPoint:NSMakePoint (DRAG_LABEL_BORDER_X, imageSize.height - DRAG_LABEL_BORDER_Y_OFFSET - [labelFont pointSize])
             withTopColor:topColor bottomColor:bottomColor font:labelFont];
    
    [dragImage unlockFocus];
    
    return dragImage;
}

- (void)_handleMouseDragged:(NSEvent *)event
{
    // If the frame has a provisional data source, this view may be released.
    // Don't allow drag because drag callbacks will reference this released view.
    if ([[self _frame] provisionalDataSource]) {
	return;
    }

    NSPoint mouseDownPoint = [self convertPoint:[_private->mouseDownEvent locationInWindow] fromView:nil];
    NSDictionary *element = [self _elementAtPoint:mouseDownPoint];

    NSURL *linkURL = [element objectForKey:WebElementLinkURLKey];
    NSURL *imageURL = [element objectForKey:WebElementImageURLKey];
    BOOL isSelectedText = [[element objectForKey:WebElementIsSelectedKey] boolValue];

    [_private->draggingImageURL release];
    _private->draggingImageURL = nil;

    // We must have started over something draggable:
    ASSERT((imageURL && [[WebPreferences standardPreferences] loadsImagesAutomatically]) ||
           (!imageURL && linkURL) || isSelectedText); 

    NSPoint mouseDraggedPoint = [self convertPoint:[event locationInWindow] fromView:nil];
    float deltaX = ABS(mouseDraggedPoint.x - mouseDownPoint.x);
    float deltaY = ABS(mouseDraggedPoint.y - mouseDownPoint.y);
    
    // Drag hysteresis hasn't been met yet but we don't want to do other drag actions like selection.
    if (((imageURL || linkURL) && deltaX < DragHysteresis && deltaY < DragHysteresis) ||
        (isSelectedText && deltaX < TextDragHysteresis && deltaY < TextDragHysteresis)) {
	return;
    }
    
    if (imageURL) {
	_private->draggingImageURL = [imageURL retain];
        WebImageRenderer *image = [element objectForKey:WebElementImageKey];
        ASSERT([image isKindOfClass:[WebImageRenderer class]]);
        [self _web_dragPromisedImage:image
                                rect:[[element objectForKey:WebElementImageRectKey] rectValue]
                                 URL:linkURL ? linkURL : imageURL
                               title:[element objectForKey:WebElementImageAltStringKey]
                               event:_private->mouseDownEvent];
        
    } else if (linkURL) {
        NSPasteboard *pasteboard = [NSPasteboard pasteboardWithName:NSDragPboard];
        NSString *label = [element objectForKey:WebElementLinkLabelKey];
        [pasteboard _web_writeURL:linkURL andTitle:label withOwner:self];
        NSImage *dragImage = [self _dragImageForLinkElement:element];
        NSSize offset = NSMakeSize([dragImage size].width / 2, -DRAG_LABEL_BORDER_Y);
        [self dragImage:dragImage
                     at:NSMakePoint(mouseDraggedPoint.x - offset.width, mouseDraggedPoint.y - offset.height)
                 offset:offset
                  event:event
             pasteboard:pasteboard
                 source:self
              slideBack:NO];
        
    } else if (isSelectedText) {
        NSPasteboard *pasteboard = [NSPasteboard pasteboardWithName:NSDragPboard];
        [self _writeSelectionToPasteboard:pasteboard];
        NSImage *selectionImage = [[self _bridge] selectionImage];
        [selectionImage _web_dissolveToFraction:WebDragImageAlpha];
        NSRect visibleSelectionRect = [[self _bridge] visibleSelectionRect];
        [self dragImage:selectionImage
                     at:NSMakePoint(NSMinX(visibleSelectionRect), NSMaxY(visibleSelectionRect))
                 offset:NSMakeSize(mouseDownPoint.x, mouseDownPoint.y)
                  event:_private->mouseDownEvent
             pasteboard:pasteboard
                 source:self
              slideBack:YES];
    } else {
        ERROR("Attempt to drag unknown element");
    }
}

- (void)_handleAutoscrollForMouseDragged:(NSEvent *)event
{
    // FIXME: this really needs to be based on a timer
    [self autoscroll:event];
}

- (BOOL)_mayStartDragWithMouseDragged:(NSEvent *)mouseDraggedEvent
{
    NSPoint mouseDownPoint = [self convertPoint:[_private->mouseDownEvent locationInWindow] fromView:nil];
    NSDictionary *mouseDownElement = [self _elementAtPoint:mouseDownPoint];

    NSURL *imageURL = [mouseDownElement objectForKey: WebElementImageURLKey];

    if ((imageURL && [[WebPreferences standardPreferences] loadsImagesAutomatically]) ||
        (!imageURL && [mouseDownElement objectForKey: WebElementLinkURLKey]) ||
        ([[mouseDownElement objectForKey:WebElementIsSelectedKey] boolValue] &&
         ([mouseDraggedEvent timestamp] - [_private->mouseDownEvent timestamp]) > TextDragDelay)) {
        return YES;
    }

    return NO;
}

- (WebPluginController *)_pluginController
{
    return _private->pluginController;
}

- (NSRect)_selectionRect
{
    return [[self _bridge] selectionRect];
}

- (void)_startAutoscrollTimer
{
    if (_private->autoscrollTimer == nil) {
        _private->autoscrollTimer = [[NSTimer scheduledTimerWithTimeInterval:AUTOSCROLL_INTERVAL
            target:self selector:@selector(_autoscroll) userInfo:nil repeats:YES] retain];
    }
}

- (void)_stopAutoscrollTimer
{
    NSTimer *timer = _private->autoscrollTimer;
    _private->autoscrollTimer = nil;
    [timer invalidate];
    [timer release];
}

- (void)_autoscroll
{
    NSEvent *mouseUpEvent = [NSApp nextEventMatchingMask:NSLeftMouseUpMask untilDate:nil inMode:NSEventTrackingRunLoopMode dequeue:NO];
    if (mouseUpEvent) {
        return;
    }

    NSEvent *fakeEvent = [NSEvent mouseEventWithType:NSLeftMouseDragged
        location:[[self window] convertScreenToBase:[NSEvent mouseLocation]]
        modifierFlags:[[NSApp currentEvent] modifierFlags]
        timestamp:[NSDate timeIntervalSinceReferenceDate]
        windowNumber:[[self window] windowNumber]
        context:[[NSApp currentEvent] context]
        eventNumber:0 clickCount:0 pressure:0];
    [self mouseDragged:fakeEvent];
}

@end

@implementation NSView (WebHTMLViewPrivate)

- (void)_web_propagateDirtyRectToAncestor
{
    [_subviews makeObjectsPerformSelector:@selector(_web_propagateDirtyRectToAncestor)];
    if ([self needsDisplay]) {
        [[self superview] setNeedsDisplayInRect:[self convertRect:[self _dirtyRect] toView:[self superview]]];
    }
}

@end

@implementation NSMutableDictionary (WebHTMLViewPrivate)

- (void)_web_setObjectIfNotNil:(id)object forKey:(id)key
{
    if (object == nil) {
        [self removeObjectForKey:key];
    } else {
        [self setObject:object forKey:key];
    }
}

@end

// The following is a workaround for
// <rdar://problem/3429631> window stops getting mouse moved events after first tooltip appears
// The trick is to define a category on NSToolTipPanel that implements setAcceptsMouseMovedEvents:.
// Since the category will be searched before the real class, we'll prevent the flag from being
// set on the tool tip panel.

@interface NSToolTipPanel : NSPanel
@end

@interface NSToolTipPanel (WebHTMLViewPrivate)
@end

@implementation NSToolTipPanel (WebHTMLViewPrivate)

- (void)setAcceptsMouseMovedEvents:(BOOL)flag
{
    // Do nothing, preventing the tool tip panel from trying to accept mouse-moved events.
}

@end
