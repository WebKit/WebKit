/*	
    WebHTMLView.m
    Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebHTMLView.h>

#import <WebKit/WebBridge.h>
#import <WebKit/WebClipView.h>
#import <WebKit/WebContextMenuDelegate.h>
#import <WebKit/WebController.h>
#import <WebKit/WebControllerPrivate.h>
#import <WebKit/WebDataSourcePrivate.h>
#import <WebKit/WebDOMDocument.h>
#import <WebKit/WebDynamicScrollBarsView.h>
#import <WebKit/WebException.h>
#import <WebKit/WebFrame.h>
#import <WebKit/WebFramePrivate.h>
#import <WebKit/WebHTMLViewPrivate.h>
#import <WebKit/WebIconDatabase.h>
#import <WebKit/WebIconLoader.h>
#import <WebKit/WebKitLogging.h>
#import <WebKit/WebNSImageExtras.h>
#import <WebKit/WebNSPasteboardExtras.h>
#import <WebKit/WebNSViewExtras.h>
#import <WebKit/WebPreferences.h>
#import <WebKit/WebPluginController.h>
#import <WebKit/WebStringTruncator.h>
#import <WebKit/WebTextRenderer.h>
#import <WebKit/WebTextRendererFactory.h>
#import <WebKit/WebViewPrivate.h>

// Needed for the mouse moved notification.
#import <AppKit/NSResponder_Private.h>

// These are a little larger than typical because dragging links is a fairly
// advanced feature that can confuse non-power-users
#define DragStartXHysteresis  		10.0
#define DragStartYHysteresis  		10.0

#define DRAG_LABEL_BORDER_X		4.0
#define DRAG_LABEL_BORDER_Y		2.0
#define DRAG_LABEL_RADIUS	5

#define MIN_DRAG_LABEL_WIDTH_BEFORE_CLIP	120.0

#import <CoreGraphics/CGStyle.h>
#import <CoreGraphics/CGSTypes.h>
#import <CoreGraphics/CGContextGState.h>

@implementation WebHTMLView

+(void)initialize
{
    [NSApp registerServicesMenuSendTypes:[[self class] _pasteboardTypes] returnTypes:nil];
}

- initWithFrame: (NSRect) frame
{
    [super initWithFrame: frame];
    
    _private = [[WebHTMLViewPrivate alloc] init];

    _private->needsLayout = YES;

    _private->canDragTo = YES;
    _private->canDragFrom = YES;

    return self;
}

- (void)dealloc
{
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
    NSPasteboard *findPasteboard;

    if (![self hasSelection]) {
        NSBeep();
        return;
    }
    
    // Note: Can't use writeSelectionToPasteboard:type: here, though it seems equivalent, because
    // it doesn't declare the types to the pasteboard and thus doesn't bump the change count.
    findPasteboard = [NSPasteboard pasteboardWithName:NSFindPboard];
    [findPasteboard declareTypes:[NSArray arrayWithObject:NSStringPboardType] owner:self];
    [findPasteboard setString:[self selectedString] forType:NSStringPboardType];
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

- (void)addMouseMovedObserver
{
    if ([[self window] isMainWindow] && ![self _insideAnotherHTMLView]) {
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(mouseMovedNotification:)
            name:NSMouseMovedNotification object:nil];
        [self _frameOrBoundsChanged];
    }
}

- (void)removeMouseMovedObserver
{
    [[self _controller] _mouseDidMoveOverElement:nil modifierFlags:0];
    [[NSNotificationCenter defaultCenter] removeObserver:self
        name:NSMouseMovedNotification object:nil];
}

- (void)addSuperviewObservers
{
    // We watch the bounds of our superview, so that we can do a layout when the size
    // of the superview changes. This is different from other scrollable things that don't
    // need this kind of thing because their layout doesn't change.
    
    // We need to pay attention to both height and width because, our "layout" has to change
    // to extend the background the full height of the space.
    
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
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(windowDidBecomeMain:)
            name:NSWindowDidBecomeMainNotification object:window];
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(windowDidResignMain:)
            name:NSWindowDidResignMainNotification object:window];
    }
}

- (void)removeWindowObservers
{
    NSWindow *window = [self window];
    if (window) {
        [[NSNotificationCenter defaultCenter] removeObserver:self
            name:NSWindowDidBecomeMainNotification object:window];
        [[NSNotificationCenter defaultCenter] removeObserver:self
            name:NSWindowDidResignMainNotification object:window];
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
    [self removeMouseMovedObserver];
    [self removeWindowObservers];
    [self removeSuperviewObservers];
    [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(_updateMouseoverWithFakeEvent) object:nil];
}

- (void)viewDidMoveToWindow
{
    if ([self window]) {
        [self addWindowObservers];
        [self addSuperviewObservers];
        [self addMouseMovedObserver];
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

- (void)addSubview:(NSView *)view
{
    [super addSubview:view];

    if ([view conformsToProtocol:@protocol(WebPlugin)]) {
        [[[self _frame] _pluginController] didAddPluginView:view];
    }
}

- (void)reapplyStyles
{
    if (!_private->needsToApplyStyles) {
        return;
    }
    
#ifdef _KWQ_TIMING        
    double start = CFAbsoluteTimeGetCurrent();
#endif

    [[self _bridge] reapplyStyles];
    
#ifdef _KWQ_TIMING        
    double thisTime = CFAbsoluteTimeGetCurrent() - start;
    LOG(Timing, "%s apply style seconds = %f", [self URL], thisTime);
#endif

    _private->needsToApplyStyles = NO;
}


- (void)layout
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
    [[self _bridge] forceLayout];
    _private->needsLayout = NO;
    
    _private->lastLayoutSize = [(NSClipView *)[self superview] documentVisibleRect].size;
    
    [self setNeedsDisplay:YES];

#ifdef _KWQ_TIMING        
    double thisTime = CFAbsoluteTimeGetCurrent() - start;
    LOG(Timing, "%s layout seconds = %f", [self URL], thisTime);
#endif
}


// Drag and drop links and images.  Others?
- (void)setAcceptsDrags: (BOOL)flag
{
    _private->canDragFrom = flag;
}

- (BOOL)acceptsDrags
{
    return _private->canDragFrom;
}

- (void)setAcceptsDrops: (BOOL)flag
{
    _private->canDragTo = flag;
}

- (BOOL)acceptsDrops
{
    return _private->canDragTo;
}

- (NSMenu *)menuForEvent:(NSEvent *)theEvent
{    
    NSPoint point = [self convertPoint:[theEvent locationInWindow] fromView:nil];
    NSDictionary *element = [self _elementAtPoint:point];

    return [[self _controller] _menuForElement:element];
}

// Search from the end of the currently selected location, or from the beginning of the document if nothing
// is selected.
- (BOOL)searchFor: (NSString *)string direction: (BOOL)forward caseSensitive: (BOOL)caseFlag
{
    return [[self _bridge] searchFor: string direction: forward caseSensitive: caseFlag];
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
    
    NSView *focusView = [NSView focusView];
    if ([WebTextRenderer shouldBufferTextDrawing] && focusView)
        [[WebTextRendererFactory sharedFactory] startCoalesceTextDrawing];

    //double start = CFAbsoluteTimeGetCurrent();
    [[self _bridge] drawRect:rect];
    //LOG(Timing, "draw time %e", CFAbsoluteTimeGetCurrent() - start);

    if ([WebTextRenderer shouldBufferTextDrawing] && focusView)
        [[WebTextRendererFactory sharedFactory] endCoalesceTextDrawing];

    [(WebClipView *)[self superview] resetAdditionalClip];
    
    [self _drawBorder: [[self _bridge] frameBorderStyle]];

    [NSGraphicsContext restoreGraphicsState];

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

- (void)windowDidBecomeMain:(NSNotification *)notification
{
    ASSERT([notification object] == [self window]);
    [self addMouseMovedObserver];
}

- (void)windowDidResignMain: (NSNotification *)notification
{
    ASSERT([notification object] == [self window]);
    [self removeMouseMovedObserver];
}

- (void)mouseDown: (NSEvent *)event
{
    // Record the mouse down position so we can determine
    // drag hysteresis.
    [_private->mouseDownEvent release];
    _private->mouseDownEvent = [event retain];
    
    // Let khtml get a chance to deal with the event.
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
    // Don't allow drags to be accepted by this WebView.
    [[self _web_parentWebView] unregisterDraggedTypes];
    
    // Retain this view during the drag because it may be released before the drag ends.
    [self retain];

    [super dragImage:dragImage at:at offset:offset event:event pasteboard:pasteboard source:source slideBack:slideBack];
}

- (void)mouseDragged:(NSEvent *)event
{
    // If the frame has a provisional data source, this view may be released.
    // Don't allow drag because drag callbacks will reference this released view.
    if([[self _frame] provisionalDataSource]){
        return;
    }
    
    // Ensure that we're visible wrt the event location.
    BOOL didScroll = [self autoscroll:event];

    NSPoint mouseDownPoint = [_private->mouseDownEvent locationInWindow];
    if (didScroll){
        mouseDownPoint.x = -FLT_MAX;
        mouseDownPoint.y = -FLT_MAX;
    }
    
    // Now do WebKit dragging.
    float deltaX = ABS([event locationInWindow].x - mouseDownPoint.x);
    float deltaY = ABS([event locationInWindow].y - mouseDownPoint.y);

    NSPoint point = [self convertPoint:mouseDownPoint fromView:nil];
    NSDictionary *element = [self _elementAtPoint: point];
    NSURL *linkURL = [element objectForKey: WebElementLinkURLKey];
    NSURL *imageURL = [element objectForKey: WebElementImageURLKey];

    [_private->draggingImageURL release];
    _private->draggingImageURL = nil;
    
    if ((deltaX >= DragStartXHysteresis || deltaY >= DragStartYHysteresis) && !didScroll){
        if((imageURL && [[WebPreferences standardPreferences] willLoadImagesAutomatically]) || (!imageURL && linkURL)){
            
            if (imageURL){
                _private->draggingImageURL = [imageURL retain];
                
                [self _web_dragPromisedImage:[element objectForKey:WebElementImageKey]
                                      origin:[[element objectForKey:WebElementImageLocationKey] pointValue]
                                         URL:linkURL ? linkURL : imageURL
                                    fileType:[[imageURL path] pathExtension]
                                       title:[element objectForKey:WebElementImageAltStringKey]
                                       event:_private->mouseDownEvent];
            }else if (linkURL) {
                BOOL drawURLString = YES;
                BOOL clipURLString = NO;
                                                
                NSString *label = [element objectForKey: WebElementLinkLabelKey];
                NSString *urlString = [linkURL absoluteString];
                
                if (!label){
                    drawURLString = NO;
                    label = urlString;
                }
                
                // FIXME: This mega-block of code needs to be cleaned-up or put into another method.
                NSFont *labelFont = [NSFont systemFontOfSize: 12.0];
                NSFont *urlFont = [NSFont systemFontOfSize: 8.0];
                NSDictionary *labelAttributes = [NSDictionary dictionaryWithObjectsAndKeys: labelFont, NSFontAttributeName, [NSColor whiteColor], NSForegroundColorAttributeName, nil];
                NSDictionary *urlAttributes = [NSDictionary dictionaryWithObjectsAndKeys: urlFont, NSFontAttributeName, [NSColor whiteColor], NSForegroundColorAttributeName, nil];
                NSSize labelSize = [label sizeWithAttributes: labelAttributes];
                NSSize imageSize, urlStringSize;
                imageSize.width += labelSize.width + DRAG_LABEL_BORDER_X * 2;
                imageSize.height += labelSize.height + DRAG_LABEL_BORDER_Y *2;
                if (drawURLString){
                    urlStringSize = [urlString sizeWithAttributes: urlAttributes];
                    imageSize.height += urlStringSize.height;
                    // Clip the url string to 2.5 times the width of the label.
                    if (urlStringSize.width > MAX(2.5 * labelSize.width, MIN_DRAG_LABEL_WIDTH_BEFORE_CLIP)){
                        imageSize.width = MAX((labelSize.width * 2.5) + DRAG_LABEL_BORDER_X * 2, MIN_DRAG_LABEL_WIDTH_BEFORE_CLIP);
                        clipURLString = YES;
                    }
                    else
                        imageSize.width = MAX(labelSize.width + DRAG_LABEL_BORDER_X * 2, urlStringSize.width + DRAG_LABEL_BORDER_X * 2);
                }
                NSImage *dragImage = [[[NSImage alloc] initWithSize: imageSize] autorelease];
                [dragImage lockFocus];

                [[NSColor colorWithCalibratedRed: 0.5 green: 0.5 blue: 0.5 alpha: 0.8] set];

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
            
                // Draw the label with a slight shadow.
                CGShadowStyle shadow;
                CGSGenericObj style;
                
                shadow.version    = 0;
                shadow.elevation  = kCGShadowElevationDefault;
                shadow.azimuth    = 136.869995;
                shadow.ambient    = 0.317708;
                shadow.height     = 2.187500;
                shadow.radius     = 1.875000;
                shadow.saturation = kCGShadowSaturationDefault;
                style = CGStyleCreateShadow(&shadow);
                [NSGraphicsContext saveGraphicsState];
                CGContextSetStyle([[NSGraphicsContext currentContext] graphicsPort], style);

                if (drawURLString){
                    if (clipURLString) {
                        urlString = [WebStringTruncator rightTruncateString: urlString toWidth:imageSize.width - (DRAG_LABEL_BORDER_X * 2) withFont:urlFont];
                    }
                    [urlString drawAtPoint: NSMakePoint(DRAG_LABEL_BORDER_X, DRAG_LABEL_BORDER_Y) withAttributes: urlAttributes];
                }
                [label drawAtPoint: NSMakePoint (DRAG_LABEL_BORDER_X, DRAG_LABEL_BORDER_Y + urlStringSize.height) withAttributes: labelAttributes];

                [NSGraphicsContext restoreGraphicsState];
                CGStyleRelease(style);

                [dragImage unlockFocus];

                NSPasteboard *pasteboard = [NSPasteboard pasteboardWithName:NSDragPboard];
                [pasteboard _web_writeURL:linkURL andTitle:label withOwner:self];

                NSPoint mousePoint = [self convertPoint:[event locationInWindow] fromView:nil];
                NSSize centerOffset = NSMakeSize(imageSize.width / 2, -DRAG_LABEL_BORDER_Y);
                NSPoint imagePoint = NSMakePoint(mousePoint.x - centerOffset.width, mousePoint.y - centerOffset.height);

                [self dragImage:dragImage
                             at:imagePoint
                         offset:centerOffset
                          event:event
                     pasteboard:pasteboard
                         source:self
                      slideBack:NO];
            }
            
            return;
        }
    }

    // Give khtml a crack at the event only if we haven't started,
    // or potentially started, a drag
    if (!linkURL && !imageURL){
        [[self _bridge] mouseDragged:event];
    }
}

- (unsigned)draggingSourceOperationMaskForLocal:(BOOL)isLocal
{
    return NSDragOperationCopy;
}

- (void)draggedImage:(NSImage *)anImage endedAt:(NSPoint)aPoint operation:(NSDragOperation)operation
{
    // During a drag, we don't get any mouseMoved or flagsChanged events.
    // So after the drag we need to explicitly update the mouseover state.
    [self _updateMouseoverWithEvent:[NSApp currentEvent]];

    // Reregister for drag types because they were unregistered before the drag.
    [[self _web_parentWebView] _reregisterDraggedTypes];
    
    // Balance the previous retain from when the drag started.
    [self release];
}

- (NSArray *)namesOfPromisedFilesDroppedAtDestination:(NSURL *)dropDestination
{
    if (!_private->draggingImageURL) {
        return nil;
    }
    
    NSString *filename = [[_private->draggingImageURL path] lastPathComponent];
    NSString *path = [[dropDestination path] stringByAppendingPathComponent:filename];

    [[self _controller] _downloadURL:_private->draggingImageURL toPath:path];
    
    return [NSArray arrayWithObject:filename];
}

- (void)mouseUp: (NSEvent *)event
{
    [[self _bridge] mouseUp:event];
}

- (void)mouseMovedNotification:(NSNotification *)notification
{
    [self _updateMouseoverWithEvent:[[notification userInfo] objectForKey:@"NSEvent"]];
}

#if 0

// Why don't we call KHTMLView's keyPressEvent/keyReleaseEvent?
// Because we currently don't benefit from any of the code in there.

// It implements its own version of keyboard scrolling, but we have our
// version in WebView. It implements some keyboard access to individual
// nodes, but we are probably going to handle that on the NSView side.
// We already handle normal typing through the responder chain.

// One of the benefits of not calling through to KHTMLView is that we don't
// have to have the isScrollEvent function at all.

- (void)keyDown: (NSEvent *)event
{
    LOG(Events, "keyDown: %@", event);
    int state = 0;
    
    // FIXME: We don't want to call keyPressEvent for scrolling key events,
    // so we have to have some logic for deciding which events go to the KHTMLView.
    // Best option is probably to only pass in certain events that we know it
    // handles in a way we like.
    
    if (passToWidget) {
        [self _addModifiers:[event modifierFlags] toState:&state];
        QKeyEvent kEvent(QEvent::KeyPress, 0, 0, state, NSSTRING_TO_QSTRING([event characters]), [event isARepeat], 1);
        
        KHTMLView *widget = _private->widget;
        if (widget)
            widget->keyPressEvent(&kEvent);
    } else {
        [super keyDown:event];
    }
}

- (void)keyUp: (NSEvent *)event
{
    LOG(Events, "keyUp: %@", event);
    int state = 0;
    
    // FIXME: Make sure this logic matches keyDown above.
    
    if (passToWidget) {
        [self _addModifiers:[event modifierFlags] toState:&state];
        QKeyEvent kEvent(QEvent::KeyPress, 0, 0, state, NSSTRING_TO_QSTRING([event characters]), [event isARepeat], 1);
        
        KHTMLView *widget = _private->widget;
        if (widget)
            widget->keyReleaseEvent(&kEvent);
    } else {
        [super keyUp:event];
    }
}

#endif

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
    switch ([[self window] keyViewSelectionDirection]) {
    case NSDirectSelection:
        break;
    case NSSelectingNext:
        view = [[self _bridge] nextKeyViewInsideWebViews];
        break;
    case NSSelectingPrevious:
        view = [[self _bridge] previousKeyViewInsideWebViews];
        break;
    }
    if (view) {
        [[self window] makeFirstResponder:view];
    } 
    return YES;
}

//------------------------------------------------------------------------------------
// WebDocumentView protocol
//------------------------------------------------------------------------------------
- (void)setDataSource:(WebDataSource *)dataSource 
{
    [[dataSource _bridge]
        createKHTMLViewWithNSView:self
        marginWidth:[[[dataSource webFrame] webView] _marginWidth]
        marginHeight:[[[dataSource webFrame] webView] _marginHeight]];
    [[self _bridge] installInFrame:[[self _web_parentWebView] frameScrollView]];
}

- (void)dataSourceUpdated:(WebDataSource *)dataSource
{
}

@end
