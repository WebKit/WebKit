/*	
    WebHTMLView.mm
    Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebHTMLView.h>

#import <WebKit/WebBridge.h>
#import <WebKit/WebController.h>
#import <WebKit/WebControllerPrivate.h>
#import <WebKit/WebDataSourcePrivate.h>
#import <WebKit/WebDynamicScrollBarsView.h>
#import <WebKit/WebException.h>
#import <WebKit/WebFrame.h>
#import <WebKit/WebHTMLViewPrivate.h>
#import <WebKit/WebIconDatabase.h>
#import <WebKit/WebIconLoader.h>
#import <WebKit/WebKitDebug.h>
#import <WebKit/WebNSViewExtras.h>
#import <WebKit/WebTextRenderer.h>
#import <WebKit/WebTextRendererFactory.h>
#import <WebKit/WebViewPrivate.h>
#import <WebKit/WebURLsWithTitles.h>

#import <WebFoundation/WebAssertions.h>

// Needed for the mouse moved notification.
#import <AppKit/NSResponder_Private.h>

@implementation WebHTMLView

- initWithFrame: (NSRect) frame
{
    [super initWithFrame: frame];
    
    _private = [[WebHTMLViewPrivate alloc] init];

    _private->needsLayout = YES;

    _private->canDragTo = YES;
    _private->canDragFrom = YES;

    // We will add/remove this view as a mouse moved observer when its window becomes/resigns main.
    [[NSNotificationCenter defaultCenter] addObserver: self selector: @selector(windowDidBecomeMain:) name: NSWindowDidBecomeMainNotification object: nil];
    [[NSNotificationCenter defaultCenter] addObserver: self selector: @selector(windowDidResignMain:) name: NSWindowDidResignMainNotification object: nil];

    return self;
}

- (BOOL)hasSelection
{
    return [[[self _bridge] selectedText] length] != 0;
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
    [findPasteboard setString:[[self _bridge] selectedText] forType:NSStringPboardType];
}

- (void)copy:(id)sender
{
    NSPasteboard *pboard = [NSPasteboard generalPasteboard];
    [pboard declareTypes:[NSArray arrayWithObjects:NSStringPboardType, nil] owner:nil];
    [pboard setString:[[self _bridge] selectedText] forType:NSStringPboardType];
}


- (void)selectAll: sender
{
    [[self _bridge] selectAll];
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


- (void)dealloc 
{
    [self _reset];
    [[NSNotificationCenter defaultCenter] removeObserver: self];
    [_private release];
    _private = nil;
    [super dealloc];
}


- (BOOL)acceptsFirstResponder
{
    return YES;
}

- (void)addMouseMovedObserver
{
    [[NSNotificationCenter defaultCenter] addObserver: self selector: @selector(mouseMovedNotification:) name: NSMouseMovedNotification object: nil];
}

- (void)removeMouseMovedObserver
{
    [[NSNotificationCenter defaultCenter] removeObserver: self name: NSMouseMovedNotification object: nil];
}

- (void)removeNotifications
{
    [self removeMouseMovedObserver];
    [[NSNotificationCenter defaultCenter] removeObserver: self name: NSWindowDidResignMainNotification object: nil];
    [[NSNotificationCenter defaultCenter] removeObserver: self name: NSWindowDidResignMainNotification object: nil];
}

- (void)viewWillMoveToWindow:(NSWindow *)window
{
    if ([[self window] isMainWindow]) {
	[self removeMouseMovedObserver];
    }
}

- (void)viewDidMoveToWindow
{
    if ([self window]) {
        if ([[self window] isMainWindow]) {
            [self addMouseMovedObserver];
        }
        _private->inWindow = YES;
    } else {
        // Reset when we are moved out of a window after being moved into one.
        // Without this check, we reset ourselves before we even start.
        // This is only needed because viewDidMoveToWindow is called even when
        // the window is not changing (bug in AppKit).
        if (_private->inWindow) {
            [self removeNotifications];
            [self _reset];
            _private->inWindow = NO;
        }
    }
    [super viewDidMoveToWindow];
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
    WEBKITDEBUGLEVEL(WEBKIT_LOG_TIMING, "%s apply style seconds = %f", [self URL], thisTime);
#endif

    _private->needsToApplyStyles = NO;
}


// This method should not be public until we have more completely
// understood how WebView will be subclassed.
- (void)layout
{
    // Ensure that we will receive mouse move events.  Is this the best place to put this?
    [[self window] setAcceptsMouseMovedEvents: YES];
    [[self window] _setShouldPostEventNotifications: YES];

    if (!_private->needsLayout) {
        return;
    }

#ifdef _KWQ_TIMING        
    double start = CFAbsoluteTimeGetCurrent();
#endif

    WEBKITDEBUGLEVEL(WEBKIT_LOG_VIEW, "%s doing layout", DEBUG_OBJECT(self));
    [[self _bridge] forceLayout];
    _private->needsLayout = NO;

#ifdef _KWQ_TIMING        
    double thisTime = CFAbsoluteTimeGetCurrent() - start;
    WEBKITDEBUGLEVEL(WEBKIT_LOG_TIMING, "%s layout seconds = %f", [self URL], thisTime);
#endif
}


// Drag and drop links and images.  Others?
- (void)setCanDragFrom: (BOOL)flag
{
    _private->canDragFrom = flag;
}

- (BOOL)canDragFrom
{
    return _private->canDragFrom;
}

- (void)setCanDragTo: (BOOL)flag
{
    _private->canDragTo = flag;
}

- (BOOL)canDragTo
{
    return _private->canDragTo;
}

- (NSMenu *)menuForEvent:(NSEvent *)theEvent
{
    id <WebContextMenuHandler> contextMenuHandler, defaultContextMenuHandler;
    NSArray *menuItems, *defaultMenuItems;
    NSDictionary *elementInfo;
    NSMenu *menu = nil;
    NSPoint point;
    unsigned i;
    
    point = [self convertPoint:[theEvent locationInWindow] fromView:nil];
    elementInfo = [self _elementAtPoint:point];

    defaultContextMenuHandler = [[self _controller] _defaultContextMenuHandler];
    defaultMenuItems = [defaultContextMenuHandler contextMenuItemsForElement: elementInfo  defaultMenuItems: nil];
    contextMenuHandler = [[self _controller] contextMenuHandler];

    if(contextMenuHandler){
        menuItems = [contextMenuHandler contextMenuItemsForElement: elementInfo  defaultMenuItems: defaultMenuItems];
    } else {
        menuItems = defaultMenuItems;
    }
    
    if([menuItems count] > 0){
        menu = [[[NSMenu alloc] init] autorelease];
    
        for(i=0; i<[menuItems count]; i++){
            [menu addItem:[menuItems objectAtIndex:i]];
        }
    }
    
    return menu;
}

- (void)setContextMenusEnabled: (BOOL)flag
{
    [NSException raise:WebMethodNotYetImplemented format:@"WebView::setContextMenusEnabled: is not implemented"];
}


- (BOOL)contextMenusEnabled;
{
    return NO;
}


// Remove the selection.
- (void)deselectText
{
    [NSException raise:WebMethodNotYetImplemented format:@"WebView::deselectText: is not implemented"];
}



// Search from the end of the currently selected location, or from the beginning of the document if nothing
// is selected.
- (BOOL)searchFor: (NSString *)string direction: (BOOL)forward caseSensitive: (BOOL)caseFlag
{
    return [[self _bridge] searchFor: string direction: forward caseSensitive: caseFlag];
}


// Get an attributed string that represents the current selection.
- (NSAttributedString *)selectedText
{
    [NSException raise:WebMethodNotYetImplemented format:@"WebView::selectedText is not implemented"];
    return nil;
}


- (BOOL)isOpaque
{
    return YES;
}


- (void)setNeedsDisplay:(BOOL)flag
{
    WEBKITDEBUGLEVEL (WEBKIT_LOG_VIEW, "%s flag = %d", DEBUG_OBJECT(self), (int)flag);
    [super setNeedsDisplay: flag];
}


- (void)setNeedsLayout: (BOOL)flag
{
    WEBKITDEBUGLEVEL (WEBKIT_LOG_VIEW, "%s flag = %d", DEBUG_OBJECT(self), (int)flag);
    _private->needsLayout = flag;
}


- (void)setNeedsToApplyStyles: (BOOL)flag
{
    WEBKITDEBUGLEVEL (WEBKIT_LOG_VIEW, "%s flag = %d", DEBUG_OBJECT(self), (int)flag);
    _private->needsToApplyStyles = flag;
}


// This should eventually be removed.
- (void)drawRect:(NSRect)rect
{
    WEBKITDEBUGLEVEL (WEBKIT_LOG_VIEW, "%s drawing", DEBUG_OBJECT(self));

    if ([self inLiveResize]){
        if (!NSEqualRects(rect, [self visibleRect])){
            rect = [self visibleRect];
            [self setNeedsLayout: YES];
        }
    }

    [self reapplyStyles];

    [self layout];

#ifdef _KWQ_TIMING
    double start = CFAbsoluteTimeGetCurrent();
#endif
    
    NSView *focusView = [NSView focusView];
    if ([WebTextRenderer shouldBufferTextDrawing] && focusView)
        [[WebTextRendererFactory sharedFactory] startCoalesceTextDrawing];

    //double start = CFAbsoluteTimeGetCurrent();
    [[self _bridge] drawRect:rect];
    //WebKitDebugAtLevel (WEBKIT_LOG_TIMING, "draw time %e", CFAbsoluteTimeGetCurrent() - start);

    if ([WebTextRenderer shouldBufferTextDrawing] && focusView)
        [[WebTextRendererFactory sharedFactory] endCoalesceTextDrawing];

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
    WEBKITDEBUGLEVEL (WEBKIT_LOG_TIMING, "%s draw seconds = %f", widget->part()->baseURL().URL().latin1(), thisTime);
#endif
}

- (BOOL)isFlipped 
{
    return YES;
}

- (void)windowDidBecomeMain: (NSNotification *)notification
{
    if ([notification object] == [self window]) {
        [self addMouseMovedObserver];
    }
}

- (void)windowDidResignMain: (NSNotification *)notification
{
    if ([notification object] == [self window]) {
        [self removeMouseMovedObserver];
    }
}

- (void)mouseDown: (NSEvent *)event
{
    // Record the mouse down position so we can determine
    // drag hysteresis.
    _private->mouseDownPoint = [event locationInWindow];
    
    // Let khtml get a chance to deal with the event.
    [[self _bridge] mouseDown:event];
}

#define DragStartXHysteresis  		2.0
#define DragStartYHysteresis  		1.0

- (void)mouseDragged:(NSEvent *)event
{
    // Ensure that we're visible wrt the event location.
    [self autoscroll:event];
    
    // Now do WebKit dragging.
    float deltaX = ABS([event locationInWindow].x - _private->mouseDownPoint.x);
    float deltaY = ABS([event locationInWindow].y - _private->mouseDownPoint.y);

    if (deltaX >= DragStartXHysteresis || deltaY >= DragStartYHysteresis){
        NSPoint point = [self convertPoint:_private->mouseDownPoint fromView:nil];
        NSDictionary *element = [self _elementAtPoint: point];
        NSURL *linkURL = [element objectForKey: WebContextMenuElementLinkURLKey];
        NSURL *imageURL = [element objectForKey: WebContextMenuElementImageURLKey];

        if(imageURL || linkURL){
            [_private->draggedURL release];
            
            if (imageURL){
                _private->draggedURL = imageURL;

                NSArray *fileType = [NSArray arrayWithObject:[[_private->draggedURL path] pathExtension]];
                NSRect rect = NSMakeRect(point.x + -16, point.y - 16, 32, 32);
                [self dragPromisedFilesOfTypes: fileType fromRect: rect source: self slideBack: YES event: event];
            }
            else if (linkURL) {
                _private->draggedURL = linkURL;
                                
                NSString *label = [element objectForKey: WebContextMenuElementLinkLabelKey];
                
                if (!label)
                    label = [linkURL absoluteString];
                    
                NSFont *dragImageFont = [NSFont systemFontOfSize: 12.0];
                NSDictionary *dragImageAttributes = [NSDictionary dictionaryWithObject:dragImageFont forKey: NSFontAttributeName];
                NSSize imageSize = [label sizeWithAttributes: dragImageAttributes];
                imageSize.width += 4.0;
                imageSize.height += 4.0;
                NSImage *dragImage = [[[NSImage alloc] initWithSize: imageSize] autorelease];
                [dragImage lockFocus];
                [[NSColor colorWithCalibratedRed: 0.25 green: 0.25 blue: 0.75 alpha: 0.5] set];
                [NSBezierPath fillRect:NSMakeRect(0, 0, imageSize.width, imageSize.height)];
                [label drawAtPoint: NSMakePoint (2.0,2.0) withAttributes: dragImageAttributes];
                [dragImage unlockFocus];

                NSPasteboard *pasteboard = [NSPasteboard pasteboardWithName:NSDragPboard];
                [pasteboard declareTypes:[NSArray arrayWithObjects:NSURLPboardType, WebURLsWithTitlesPboardType, nil] owner:nil];
                [WebURLsWithTitles writeURLs:[NSArray arrayWithObjects: linkURL, nil] andTitles:[NSArray arrayWithObjects: label, nil] toPasteboard:pasteboard];

                NSSize offset;
                offset.width = 0;
                offset.height = 0;
                [self dragImage:dragImage
                            at:[self convertPoint:[event locationInWindow] fromView:nil]
                        offset:offset
                        event:event
                    pasteboard:pasteboard
                        source:self
                    slideBack:NO];
            }
            else
                _private->draggedURL = nil;
            
            [_private->draggedURL retain];
            
            return;
        }
    }

    // Give khtml a crack at the event only if we haven't started
    // a drag.
    [[self _bridge] mouseDragged:event];
}


- (void)mouseUp: (NSEvent *)event
{
    NSEvent *theEvent;
    
    if([self _continueAfterClickPolicyForEvent:event]){
        theEvent = event;
    }else{
        // Send a bogus mouse up event so we don't confuse WebCore
        theEvent = [NSEvent mouseEventWithType: NSLeftMouseUp
                                      location: NSMakePoint(0,0)
                                 modifierFlags: [event modifierFlags]
                                     timestamp: [event timestamp]
                                  windowNumber: [event windowNumber]
                                       context: [event context]
                                   eventNumber: [event eventNumber]
                                    clickCount: [event clickCount]
                                      pressure: [event pressure]];
    }
    
    [[self _bridge] mouseUp:theEvent];
}

- (void)mouseMovedNotification:(NSNotification *)notification
{
    // Only act on the mouse move event if it's inside this view (and not inside a subview).
    NSEvent *event = [[notification userInfo] objectForKey:@"NSEvent"];
    if ([event window] == [self window] && [[self window] isMainWindow]
            && [[[self window] contentView] hitTest:[event locationInWindow]] == self) {
        [[self _bridge] mouseMoved:event];
    }
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
    WEBKITDEBUGLEVEL(WEBKIT_LOG_EVENTS, "keyDown: %s", DEBUG_OBJECT(event));
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
    WEBKITDEBUGLEVEL(WEBKIT_LOG_EVENTS, "keyUp: %s", DEBUG_OBJECT(event));
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

- (unsigned)draggingSourceOperationMaskForLocal:(BOOL)isLocal
{
    return NSDragOperationCopy;
}

- (NSArray *)namesOfPromisedFilesDroppedAtDestination:(NSURL *)dropDestination
{
    NSString *filename = [[_private->draggedURL path] lastPathComponent];
    NSString *path = [[dropDestination path] stringByAppendingPathComponent:filename];

    [[self _controller] _downloadURL:_private->draggedURL toPath:path];
    return [NSArray arrayWithObject:filename];
}

- (CFStringEncoding)textEncoding
{
    return [[self _bridge] textEncoding];
}

- (void)setTextEncoding:(CFStringEncoding)encoding
{
    WebFrame *frame = [self _frame];
    [frame reload:NO];
    [[frame provisionalDataSource] _setOverrideEncoding:encoding];
}

- (void)setDefaultTextEncoding
{
    WebFrame *frame = [self _frame];
    [frame reload:NO];
    [[frame provisionalDataSource] _setOverrideEncoding:kCFStringEncodingInvalidId];
}

- (BOOL)usingDefaultTextEncoding
{
    return [[[self _frame] dataSource] _overrideEncoding] == kCFStringEncodingInvalidId;
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
- (void)provisionalDataSourceChanged:(WebDataSource *)dataSource 
{
    [[dataSource _bridge]
        createKHTMLViewWithNSView:self
        marginWidth:[[[dataSource webFrame] webView] _marginWidth]
        marginHeight:[[[dataSource webFrame] webView] _marginHeight]];
}


- (void)provisionalDataSourceCommitted:(WebDataSource *)dataSource 
{
    [[self _bridge] installInFrame:[[self _web_parentWebView] frameScrollView]];
}

- (void)dataSourceUpdated:(WebDataSource *)dataSource
{
}




@end
