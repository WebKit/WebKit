/*	
    WebHTMLView.m
    Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebHTMLView.h>

#import <WebKit/WebBridge.h>
#import <WebKit/WebClipView.h>
#import <WebKit/WebDataSourcePrivate.h>
#import <WebKit/WebDOMDocument.h>
#import <WebKit/WebException.h>
#import <WebKit/WebFrame.h>
#import <WebKit/WebFramePrivate.h>
#import <WebKit/WebFrameViewPrivate.h>
#import <WebKit/WebHTMLViewPrivate.h>
#import <WebKit/WebNetscapePluginEmbeddedView.h>
#import <WebKit/WebKitLogging.h>
#import <WebKit/WebNSPasteboardExtras.h>
#import <WebKit/WebNSViewExtras.h>
#import <WebKit/WebPluginController.h>
#import <WebKit/WebTextRenderer.h>
#import <WebKit/WebTextRendererFactory.h>
#import <WebKit/WebUnicode.h>
#import <WebKit/WebViewPrivate.h>

#import <AppKit/NSResponder_Private.h>
#import <CoreGraphics/CGContextGState.h>

@interface WebHTMLView (WebHTMLViewPrivate)
- (void)_setPrinting:(BOOL)printing pageWidth:(float)pageWidth;
@end

@interface NSArray (WebHTMLView)
- (void)_web_makePluginViewsPerformSelector:(SEL)selector withObject:(id)object;
@end

@interface NSView (AppKitSecretsIKnowAbout)
- (void)_setDrawsOwnDescendants:(BOOL)drawsOwnDescendants;
@end

@implementation WebHTMLView

+ (void)initialize
{
    WebKitInitializeUnicode();
    [NSApp registerServicesMenuSendTypes:[[self class] _pasteboardTypes] returnTypes:nil];
}

- (id)initWithFrame:(NSRect)frame
{
    [super initWithFrame:frame];
    
    // Make all drawing go through us instead of subviews.
    // The bulk of the code to handle this is in WebHTMLViewPrivate.m.
    if (NSAppKitVersionNumber >= 711) {
        [self _setDrawsOwnDescendants:YES];
    }
    
    _private = [[WebHTMLViewPrivate alloc] init];

    _private->pluginController = [[WebPluginController alloc] initWithHTMLView:self];
    _private->needsLayout = YES;

    return self;
}

- (void)dealloc
{
    [self _clearLastHitViewIfSelf];
    [self _reset];
    [[NSNotificationCenter defaultCenter] removeObserver:self];
    [_private release];
    _private = nil;
    [super dealloc];
}

- (BOOL)hasSelection
{
    return [[self selectedString] length] != 0;
}

- (IBAction)takeFindStringFromSelection:(id)sender
{
    if (![self hasSelection]) {
        NSBeep();
        return;
    }

    [NSPasteboard _web_setFindPasteboardString:[self selectedString] withOwner:self];
}

- (void)copy:(id)sender
{
    [self _writeSelectionToPasteboard:[NSPasteboard generalPasteboard]];
}

- (BOOL)writeSelectionToPasteboard:(NSPasteboard *)pasteboard types:(NSArray *)types
{
    [self _writeSelectionToPasteboard:pasteboard];
    return YES;
}

- (void)selectAll:(id)sender
{
    [self selectAll];
}

- (void)jumpToSelection: sender
{
    [[self _bridge] jumpToSelection];
}


- (BOOL)validateUserInterfaceItem:(id <NSValidatedUserInterfaceItem>)item 
{
    SEL action = [item action];
    
    if (action == @selector(copy:))
        return [self hasSelection];
    else if (action == @selector(takeFindStringFromSelection:))
        return [self hasSelection];
    else if (action == @selector(jumpToSelection:))
        return [self hasSelection];
    
    return YES;
}

- (id)validRequestorForSendType:(NSString *)sendType returnType:(NSString *)returnType
{
    if (sendType && ([[[self class] _pasteboardTypes] containsObject:sendType]) && [self hasSelection]){
        return self;
    }

    return [super validRequestorForSendType:sendType returnType:returnType];
}

- (BOOL)acceptsFirstResponder
{
    // Don't accept first responder when we first click on this view.
    // We have to pass the event down through WebCore first to be sure we don't hit a subview.
    // Do accept first responder at any other time, for example from keyboard events,
    // or from calls back from WebCore once we begin mouse-down event handling.
    NSEvent *event = [NSApp currentEvent];
    if ([event type] == NSLeftMouseDown && event != _private->mouseDownEvent) {
        return NO;
    }
    return YES;
}

- (void)updateTextBackgroundColor
{
    NSWindow *window = [self window];
    BOOL shouldUseInactiveTextBackgroundColor = !([window isKeyWindow] && [window firstResponder] == self);
    WebBridge *bridge = [self _bridge];
    if ([bridge usesInactiveTextBackgroundColor] != shouldUseInactiveTextBackgroundColor) {
        [bridge setUsesInactiveTextBackgroundColor:shouldUseInactiveTextBackgroundColor];
        [self setNeedsDisplayInRect:[bridge selectionRect]];
    }
}

- (void)addMouseMovedObserver
{
    if ([[self window] isKeyWindow] && ![self _insideAnotherHTMLView]) {
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(mouseMovedNotification:)
            name:NSMouseMovedNotification object:nil];
        [self _frameOrBoundsChanged];
    }
}

- (void)removeMouseMovedObserver
{
    [[self _webView] _mouseDidMoveOverElement:nil modifierFlags:0];
    [[NSNotificationCenter defaultCenter] removeObserver:self
        name:NSMouseMovedNotification object:nil];
}

- (void)addSuperviewObservers
{
    // We watch the bounds of our superview, so that we can do a layout when the size
    // of the superview changes. This is different from other scrollable things that don't
    // need this kind of thing because their layout doesn't change.
    
    // We need to pay attention to both height and width because our "layout" has to change
    // to extend the background the full height of the space and because some elements have
    // sizes that are based on the total size of the view.
    
    NSView *superview = [self superview];
    if (superview && [self window]) {
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_frameOrBoundsChanged) 
            name:NSViewFrameDidChangeNotification object:superview];
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_frameOrBoundsChanged) 
            name:NSViewBoundsDidChangeNotification object:superview];
    }
}

- (void)removeSuperviewObservers
{
    NSView *superview = [self superview];
    if (superview && [self window]) {
        [[NSNotificationCenter defaultCenter] removeObserver:self
            name:NSViewFrameDidChangeNotification object:superview];
        [[NSNotificationCenter defaultCenter] removeObserver:self
            name:NSViewBoundsDidChangeNotification object:superview];
    }
}

- (void)addWindowObservers
{
    NSWindow *window = [self window];
    if (window) {
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(windowDidBecomeKey:)
            name:NSWindowDidBecomeKeyNotification object:window];
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(windowDidResignKey:)
            name:NSWindowDidResignKeyNotification object:window];
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(windowWillClose:)
            name:NSWindowWillCloseNotification object:window];
    }
}

- (void)removeWindowObservers
{
    NSWindow *window = [self window];
    if (window) {
        [[NSNotificationCenter defaultCenter] removeObserver:self
            name:NSWindowDidBecomeKeyNotification object:window];
        [[NSNotificationCenter defaultCenter] removeObserver:self
            name:NSWindowDidResignKeyNotification object:window];
        [[NSNotificationCenter defaultCenter] removeObserver:self
            name:NSWindowWillCloseNotification object:window];
    }
}

- (void)viewWillMoveToSuperview:(NSView *)newSuperview
{
    [self removeSuperviewObservers];
}

- (void)viewDidMoveToSuperview
{
    [self addSuperviewObservers];
}

- (void)viewWillMoveToWindow:(NSWindow *)window
{
    // Don't do anything if we aren't initialized.  This happens
    // when decoding a WebView.  When WebViews are decoded their subviews
    // are created by initWithCoder: and so won't be normally
    // initialized.  The stub views are discarded by WebView.
    if (_private){
        // FIXME: Some of these calls may not work because this view may be already removed from it's superview.
        [self removeMouseMovedObserver];
        [self removeWindowObservers];
        [self removeSuperviewObservers];
        [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(_updateMouseoverWithFakeEvent) object:nil];
    
        [[self _pluginController] stopAllPlugins];
    }
}

- (void)viewDidMoveToWindow
{
    // Don't do anything if we aren't initialized.  This happens
    // when decoding a WebView.  When WebViews are decoded their subviews
    // are created by initWithCoder: and so won't be normally
    // initialized.  The stub views are discarded by WebView.
    if (_private){
        if ([self window]) {
            [self addWindowObservers];
            [self addSuperviewObservers];
            [self addMouseMovedObserver];
            [self updateTextBackgroundColor];
    
            [[self _pluginController] startAllPlugins];
    
            _private->inWindow = YES;
        } else {
            // Reset when we are moved out of a window after being moved into one.
            // Without this check, we reset ourselves before we even start.
            // This is only needed because viewDidMoveToWindow is called even when
            // the window is not changing (bug in AppKit).
            if (_private->inWindow) {
                [self _reset];
                _private->inWindow = NO;
            }
        }
    }
}

- (void)viewWillMoveToHostWindow:(NSWindow *)hostWindow
{
    [[self subviews] _web_makePluginViewsPerformSelector:@selector(viewWillMoveToHostWindow:) withObject:hostWindow];
}

- (void)viewDidMoveToHostWindow
{
    [[self subviews] _web_makePluginViewsPerformSelector:@selector(viewDidMoveToHostWindow) withObject:nil];
}


- (void)addSubview:(NSView *)view
{
    if ([view conformsToProtocol:@protocol(WebPlugin)]) {
        [[self _pluginController] addPlugin:view];
    }

    [super addSubview:view];
}

- (void)reapplyStyles
{
    if (!_private->needsToApplyStyles) {
        return;
    }
    
#ifdef _KWQ_TIMING        
    double start = CFAbsoluteTimeGetCurrent();
#endif

    [[self _bridge] reapplyStylesForDeviceType:
        _private->printing ? WebCoreDevicePrinter : WebCoreDeviceScreen];
    
#ifdef _KWQ_TIMING        
    double thisTime = CFAbsoluteTimeGetCurrent() - start;
    LOG(Timing, "%s apply style seconds = %f", [self URL], thisTime);
#endif

    _private->needsToApplyStyles = NO;
}

// Do a layout, but set up a new fixed width for the purposes of doing printing layout.
// pageWidth==0 implies a non-printing layout
- (void)layoutToPageWidth:(float)pageWidth
{
    [self reapplyStyles];
    
    // Ensure that we will receive mouse move events.  Is this the best place to put this?
    [[self window] setAcceptsMouseMovedEvents: YES];
    [[self window] _setShouldPostEventNotifications: YES];

    if (!_private->needsLayout) {
        return;
    }

#ifdef _KWQ_TIMING        
    double start = CFAbsoluteTimeGetCurrent();
#endif

    LOG(View, "%@ doing layout", self);
    if (pageWidth > 0.0) {
        [[self _bridge] forceLayoutForPageWidth:pageWidth];
    } else {
        [[self _bridge] forceLayout];
    }
    _private->needsLayout = NO;
    
    if (!_private->printing) {
	// get size of the containing dynamic scrollview, so
	// appearance and disappearance of scrollbars will not show up
	// as a size change
	NSSize newLayoutFrameSize = [[[self superview] superview] frame].size;
	if (_private->laidOutAtLeastOnce && !NSEqualSizes(_private->lastLayoutFrameSize, newLayoutFrameSize)) {
	    [[self _bridge] sendResizeEvent];
	}
	_private->laidOutAtLeastOnce = YES;
	_private->lastLayoutSize = [(NSClipView *)[self superview] documentVisibleRect].size;
        _private->lastLayoutFrameSize = newLayoutFrameSize;
    }
    
    [self setNeedsDisplay:YES];

#ifdef _KWQ_TIMING        
    double thisTime = CFAbsoluteTimeGetCurrent() - start;
    LOG(Timing, "%s layout seconds = %f", [self URL], thisTime);
#endif
}

- (void)layout
{
    [self layoutToPageWidth:0.0];
}

- (NSMenu *)menuForEvent:(NSEvent *)event
{
    if ([[self _bridge] sendContextMenuEvent:event]) {
        return nil;
    }
    NSPoint point = [self convertPoint:[event locationInWindow] fromView:nil];
    NSDictionary *element = [self _elementAtPoint:point];
    return [[self _webView] _menuForElement:element];
}

// Search from the end of the currently selected location, or from the beginning of the
// document if nothing is selected.
- (BOOL)searchFor:(NSString *)string direction:(BOOL)forward caseSensitive:(BOOL)caseFlag wrap:(BOOL)wrapFlag;
{
    return [[self _bridge] searchFor:string direction:forward caseSensitive:caseFlag wrap:wrapFlag];
}

- (NSString *)string
{
    return [[self attributedString] string];
}

- (NSAttributedString *)attributedString
{
    WebBridge *b = [self _bridge];
    return [b attributedStringFrom:[b DOMDocument]
                       startOffset:0
                                to:nil
                         endOffset:0];
}

- (NSString *)selectedString
{
    return [[self _bridge] selectedString];
}

// Get an attributed string that represents the current selection.
- (NSAttributedString *)selectedAttributedString
{
    return [[self _bridge] selectedAttributedString];
}

- (void)selectAll
{
    [[self _bridge] selectAll];
}

// Remove the selection.
- (void)deselectAll
{
    [[self _bridge] deselectAll];
}

- (BOOL)isOpaque
{
    return YES;
}

- (void)setNeedsDisplay:(BOOL)flag
{
    LOG(View, "%@ flag = %d", self, (int)flag);
    [super setNeedsDisplay: flag];
}

- (void)setNeedsLayout: (BOOL)flag
{
    LOG(View, "%@ flag = %d", self, (int)flag);
    _private->needsLayout = flag;
}


- (void)setNeedsToApplyStyles: (BOOL)flag
{
    LOG(View, "%@ flag = %d", self, (int)flag);
    _private->needsToApplyStyles = flag;
}

- (void)_drawBorder: (int)type
{
    switch (type){
        case SunkenFrameBorder:
        {
            NSRect vRect = [self frame];
            
            // Left, light gray, black
            [[NSColor lightGrayColor] set];
            NSRectFill(NSMakeRect(0,0,1,vRect.size.height));
            [[NSColor blackColor] set];
            NSRectFill(NSMakeRect(0,1,1,vRect.size.height-2));
    
            // Top, light gray, black
            [[NSColor lightGrayColor] set];
            NSRectFill(NSMakeRect(0,0,vRect.size.width,1));
            [[NSColor blackColor] set];
            NSRectFill(NSMakeRect(1,1,vRect.size.width-2,1));
    
            // Right, light gray, white
            [[NSColor whiteColor] set];
            NSRectFill(NSMakeRect(vRect.size.width,0,1,vRect.size.height));
            [[NSColor lightGrayColor] set];
            NSRectFill(NSMakeRect(vRect.size.width-1,1,1,vRect.size.height-2));
    
            // Bottom, light gray, white
            [[NSColor whiteColor] set];
            NSRectFill(NSMakeRect(0,vRect.size.height-1,vRect.size.width,1));
            [[NSColor lightGrayColor] set];
            NSRectFill(NSMakeRect(1,vRect.size.height-2,vRect.size.width-2,1));
            break;
        }
        
        case PlainFrameBorder: 
        {
            // Not used yet, but will need for 'focusing' frames.
            NSRect vRect = [self frame];
            
            // Left, black
            [[NSColor blackColor] set];
            NSRectFill(NSMakeRect(0,0,2,vRect.size.height));
    
            // Top, black
            [[NSColor blackColor] set];
            NSRectFill(NSMakeRect(0,0,vRect.size.width,2));
    
            // Right, black
            [[NSColor blackColor] set];
            NSRectFill(NSMakeRect(vRect.size.width,0,2,vRect.size.height));
    
            // Bottom, black
            [[NSColor blackColor] set];
            NSRectFill(NSMakeRect(0,vRect.size.height-2,vRect.size.width,2));
            break;
        }
        
        case NoFrameBorder:
        default:
        {
        }
    }
}

- (void)drawRect:(NSRect)rect
{
    LOG(View, "%@ drawing", self);
    
    BOOL subviewsWereSetAside = _private->subviewsSetAside;
    if (subviewsWereSetAside) {
        [self _restoreSubviews];
    }
    
    // This helps when we print as part of a larger print process.
    // If the WebHTMLView itself is what we're printing, then we will never have to do this.
    BOOL wasInPrintingMode = _private->printing;
    BOOL isPrinting = ![NSGraphicsContext currentContextDrawingToScreen];
    if (wasInPrintingMode != isPrinting) {
        [self _setPrinting:isPrinting pageWidth:0];
    }
    
    if ([[self _bridge] needsLayout]) {
        _private->needsLayout = YES;
    }
    BOOL didReapplyStylesOrLayout = _private->needsToApplyStyles || _private->needsLayout;

    [self layout];

    if (didReapplyStylesOrLayout) {
        // If we reapplied styles or did layout, we would like to draw as much as possible right now.
        // If we can draw the entire view, then we don't need to come back and display, even though
        // layout will have called setNeedsDisplay:YES to make that happen.
        NSRect visibleRect = [self visibleRect];
        CGRect clipBoundingBoxCG = CGContextGetClipBoundingBox((CGContextRef)[[NSGraphicsContext currentContext] graphicsPort]);
        NSRect clipBoundingBox = NSMakeRect(clipBoundingBoxCG.origin.x, clipBoundingBoxCG.origin.y,
            clipBoundingBoxCG.size.width, clipBoundingBoxCG.size.height);
        // If the clip is such that we can draw the entire view instead of just the requested bit,
        // then we will do just that. Note that this works only for rectangular clip, because we
        // are only checking if the clip's bounding box contains the rect; we would prefer to check
        // if the clip contained it, but that's not possible.
        if (NSContainsRect(clipBoundingBox, visibleRect)) {
            rect = visibleRect;
            [self setNeedsDisplay:NO];
        }
    }
    
#ifdef _KWQ_TIMING
    double start = CFAbsoluteTimeGetCurrent();
#endif

    [NSGraphicsContext saveGraphicsState];
    NSRectClip(rect);
    
    ASSERT([[self superview] isKindOfClass:[WebClipView class]]);
    [(WebClipView *)[self superview] setAdditionalClip:rect];

    NS_DURING {
        WebTextRendererFactory *textRendererFactoryIfCoalescing = nil;
        if ([WebTextRenderer shouldBufferTextDrawing] && [NSView focusView]) {
            textRendererFactoryIfCoalescing = [WebTextRendererFactory sharedFactory];
            [textRendererFactoryIfCoalescing startCoalesceTextDrawing];
        }

        //double start = CFAbsoluteTimeGetCurrent();
        [[self _bridge] drawRect:rect];
        //LOG(Timing, "draw time %e", CFAbsoluteTimeGetCurrent() - start);

        if (textRendererFactoryIfCoalescing != nil) {
            [textRendererFactoryIfCoalescing endCoalesceTextDrawing];
        }

        [(WebClipView *)[self superview] resetAdditionalClip];

        [self _drawBorder: [[self _bridge] frameBorderStyle]];

        [NSGraphicsContext restoreGraphicsState];
    } NS_HANDLER {
        [(WebClipView *)[self superview] resetAdditionalClip];
        [NSGraphicsContext restoreGraphicsState];
        ERROR("Exception caught while drawing: %@", localException);
        [localException raise];
    } NS_ENDHANDLER

#ifdef DEBUG_LAYOUT
    NSRect vframe = [self frame];
    [[NSColor blackColor] set];
    NSBezierPath *path;
    path = [NSBezierPath bezierPath];
    [path setLineWidth:(float)0.1];
    [path moveToPoint:NSMakePoint(0, 0)];
    [path lineToPoint:NSMakePoint(vframe.size.width, vframe.size.height)];
    [path closePath];
    [path stroke];
    path = [NSBezierPath bezierPath];
    [path setLineWidth:(float)0.1];
    [path moveToPoint:NSMakePoint(0, vframe.size.height)];
    [path lineToPoint:NSMakePoint(vframe.size.width, 0)];
    [path closePath];
    [path stroke];
#endif

#ifdef _KWQ_TIMING
    double thisTime = CFAbsoluteTimeGetCurrent() - start;
    LOG(Timing, "%s draw seconds = %f", widget->part()->baseURL().URL().latin1(), thisTime);
#endif

    if (wasInPrintingMode != isPrinting) {
        [self _setPrinting:wasInPrintingMode pageWidth:0];
    }

    if (subviewsWereSetAside) {
        [self _setAsideSubviews];
    }
}

// Turn off the additional clip while computing our visibleRect.
- (NSRect)visibleRect
{
    if (!([[self superview] isKindOfClass:[WebClipView class]]))
        return [super visibleRect];
        
    WebClipView *clipView = (WebClipView *)[self superview];

    BOOL hasAdditionalClip = [clipView hasAdditionalClip];
    if (!hasAdditionalClip) {
        return [super visibleRect];
    }
    
    NSRect additionalClip = [clipView additionalClip];
    [clipView resetAdditionalClip];
    NSRect visibleRect = [super visibleRect];
    [clipView setAdditionalClip:additionalClip];
    return visibleRect;
}

- (BOOL)isFlipped 
{
    return YES;
}

- (void)windowDidBecomeKey:(NSNotification *)notification
{
    ASSERT([notification object] == [self window]);
    [self addMouseMovedObserver];
    [self updateTextBackgroundColor];
}

- (void)windowDidResignKey: (NSNotification *)notification
{
    ASSERT([notification object] == [self window]);
    [self removeMouseMovedObserver];
    [self updateTextBackgroundColor];
}

- (void)windowWillClose:(NSNotification *)notification
{
    [[self _pluginController] destroyAllPlugins];
}

- (BOOL)_isSelectionEvent:(NSEvent *)event
{
    NSPoint point = [self convertPoint:[event locationInWindow] fromView:nil];
    return [[[self _elementAtPoint:point] objectForKey:WebElementIsSelectedKey] boolValue];
}

- (BOOL)acceptsFirstMouse:(NSEvent *)event
{
    return [self _isSelectionEvent:event];
}

- (BOOL)shouldDelayWindowOrderingForEvent:(NSEvent *)event
{
    return [self _isSelectionEvent:event];
}

- (void)mouseDown:(NSEvent *)event
{
    // If the web page handles the context menu event and menuForEvent: returns nil, we'll get control click events here.
    // We don't want to pass them along to KHTML a second time.
    if ([event modifierFlags] & NSControlKeyMask) {
        return;
    }
    
    _private->ignoringMouseDraggedEvents = NO;
    
    // Record the mouse down position so we can determine drag hysteresis.
    [_private->mouseDownEvent release];
    _private->mouseDownEvent = [event retain];

    // Don't do any mouseover while the mouse is down.
    [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(_updateMouseoverWithFakeEvent) object:nil];

    // Let KHTML get a chance to deal with the event.
    [[self _bridge] mouseDown:event];
}

- (void)dragImage:(NSImage *)dragImage
               at:(NSPoint)at
           offset:(NSSize)offset
            event:(NSEvent *)event
       pasteboard:(NSPasteboard *)pasteboard
           source:(id)source
        slideBack:(BOOL)slideBack
{    
    // Don't allow drags to be accepted by this WebFrameView.
    [[self _webView] unregisterDraggedTypes];
    
    // Retain this view during the drag because it may be released before the drag ends.
    [self retain];

    [super dragImage:dragImage at:at offset:offset event:event pasteboard:pasteboard source:source slideBack:slideBack];
}

- (void)mouseDragged:(NSEvent *)event
{
    if (!_private->ignoringMouseDraggedEvents) {
        [[self _bridge] mouseDragged:event];
    }
}

- (unsigned)draggingSourceOperationMaskForLocal:(BOOL)isLocal
{
    return (NSDragOperationGeneric|NSDragOperationCopy);
}

- (void)draggedImage:(NSImage *)anImage endedAt:(NSPoint)aPoint operation:(NSDragOperation)operation
{
    // Prevent queued mouseDragged events from coming after the drag and fake mouseUp event.
    _private->ignoringMouseDraggedEvents = YES;
    
    // Once the dragging machinery kicks in, we no longer get mouse drags or the up event.
    // khtml expects to get balanced down/up's, so we must fake up a mouseup.
    NSEvent *fakeEvent = [NSEvent mouseEventWithType:NSLeftMouseUp
                                            location:[[self window] convertScreenToBase:aPoint]
                                       modifierFlags:[[NSApp currentEvent] modifierFlags]
                                           timestamp:[NSDate timeIntervalSinceReferenceDate]
                                        windowNumber:[[self window] windowNumber]
                                             context:[[NSApp currentEvent] context]
                                         eventNumber:0 clickCount:0 pressure:0];
    [self mouseUp:fakeEvent];	    // This will also update the mouseover state.

    // Reregister for drag types because they were unregistered before the drag.
    [[self _webView] _registerDraggedTypes];
    
    // Balance the previous retain from when the drag started.
    [self release];
}

- (NSArray *)namesOfPromisedFilesDroppedAtDestination:(NSURL *)dropDestination
{
    if (!_private->draggingImageURL) {
        return nil;
    }

    [[self _webView] _downloadURL:_private->draggingImageURL toDirectory:[dropDestination path]];

    // FIXME: The file is supposed to be created at this point so the Finder places the file
    // where the drag ended. Since we can't create the file until the download starts,
    // this fails. Even if we did create the file at this point, the Finder doesn't
    // place the file in the right place anyway (2825055).
    // FIXME: We may return a different filename than the file that we will create.
    // Since the file isn't created at this point anwyway, it doesn't matter what we return.
    return [NSArray arrayWithObject:[[_private->draggingImageURL path] lastPathComponent]];
}

- (void)mouseUp: (NSEvent *)event
{
    [[self _bridge] mouseUp:event];
    [self _updateMouseoverWithFakeEvent];
}

- (void)mouseMovedNotification:(NSNotification *)notification
{
    [self _updateMouseoverWithEvent:[[notification userInfo] objectForKey:@"NSEvent"]];
}

- (BOOL)supportsTextEncoding
{
    return YES;
}

- (NSView *)nextKeyView
{
    return (_private && _private->inNextValidKeyView)
        ? [[self _bridge] nextKeyView]
        : [super nextKeyView];
}

- (NSView *)previousKeyView
{
    return (_private && _private->inNextValidKeyView)
        ? [[self _bridge] previousKeyView]
        : [super previousKeyView];
}

- (NSView *)nextValidKeyView
{
    _private->inNextValidKeyView = YES;
    NSView *view = [super nextValidKeyView];
    _private->inNextValidKeyView = NO;
    return view;
}

- (NSView *)previousValidKeyView
{
    _private->inNextValidKeyView = YES;
    NSView *view = [super previousValidKeyView];
    _private->inNextValidKeyView = NO;
    return view;
}

- (BOOL)becomeFirstResponder
{
    NSView *view = nil;
    if (![[self _webView] _isPerformingProgrammaticFocus]) {
	switch ([[self window] keyViewSelectionDirection]) {
	case NSDirectSelection:
	    break;
	case NSSelectingNext:
	    view = [[self _bridge] nextKeyViewInsideWebFrameViews];
	    break;
	case NSSelectingPrevious:
	    view = [[self _bridge] previousKeyViewInsideWebFrameViews];
	    break;
	}
    }
    if (view) {
        [[self window] makeFirstResponder:view];
    }
    [self updateTextBackgroundColor];
    return YES;
}

// This approach could be relaxed when dealing with 3228554
- (BOOL)resignFirstResponder
{
    BOOL resign = [super resignFirstResponder];
    if (resign) {
        [self deselectAll];
        [self updateTextBackgroundColor];
    }
    return resign;
}

//------------------------------------------------------------------------------------
// WebDocumentView protocol
//------------------------------------------------------------------------------------
- (void)setDataSource:(WebDataSource *)dataSource 
{
}

- (void)dataSourceUpdated:(WebDataSource *)dataSource
{
}

// Does setNeedsDisplay:NO as a side effect. Useful for begin/endDocument.
// pageWidth != 0 implies we will relayout to a new width
- (void)_setPrinting:(BOOL)printing pageWidth:(float)pageWidth
{
    WebFrame *frame = [self _frame];
    NSArray *subframes = [frame childFrames];
    unsigned n = [subframes count];
    unsigned i;
    for (i = 0; i != n; ++i) {
        WebFrame *subframe = [subframes objectAtIndex:i];
        WebFrameView *frameView = [subframe frameView];
        if ([[subframe dataSource] _isDocumentHTML]) {
            [(WebHTMLView *)[frameView documentView] _setPrinting:printing pageWidth:0];
        }
    }

    if (printing != _private->printing) {
        _private->printing = printing;
        [self setNeedsToApplyStyles:YES];
        [self setNeedsLayout:YES];
        [self layoutToPageWidth:pageWidth];
        [self setNeedsDisplay:NO];
    }
}

- (void)adjustPageHeightNew:(float *)newBottom top:(float)oldTop bottom:(float)oldBottom limit:(float)bottomLimit
{
    [[self _bridge] adjustPageHeightNew:newBottom top:oldTop bottom:oldBottom limit:bottomLimit];
}

- (void)beginDocument
{
    // Must do this explicit display here, because otherwise the view might redisplay while the print
    // sheet was up, using printer fonts (and looking different).
    [self displayIfNeeded];
    [[self window] setAutodisplay:NO];

    // If we are a frameset just print with the layout we have onscreen, otherwise relayout
    // according to the paper size
    float pageWidth = 0.0;
    if (![[self _bridge] isFrameSet]) {
        NSPrintInfo *printInfo = [[NSPrintOperation currentOperation] printInfo];
        pageWidth = [printInfo paperSize].width - [printInfo leftMargin] - [printInfo rightMargin];
    }
    [self _setPrinting:YES pageWidth:pageWidth];	// will relayout

    [super beginDocument];
    // There is a theoretical chance that someone could do some drawing between here and endDocument,
    // if something caused setNeedsDisplay after this point. If so, it's not a big tragedy, because
    // you'd simply see the printer fonts on screen. As of this writing, this does not happen with Safari.
}

- (void)endDocument
{
    [super endDocument];
    // Note sadly at this point [NSGraphicsContext currentContextDrawingToScreen] is still NO 
    [self _setPrinting:NO pageWidth:0.0];
    [[self window] setAutodisplay:YES];
}

@end

@implementation NSArray (WebHTMLView)

- (void)_web_makePluginViewsPerformSelector:(SEL)selector withObject:(id)object
{
    NSEnumerator *enumerator = [self objectEnumerator];
    WebNetscapePluginEmbeddedView *view;
    while ((view = [enumerator nextObject]) != nil) {
        if ([view isKindOfClass:[WebNetscapePluginEmbeddedView class]]) {
            [view performSelector:selector withObject:object];
        }
    }
}

@end
