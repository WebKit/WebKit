/*	
    WebHTMLView.mm
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebHTMLView.h>

#import <WebKit/WebDynamicScrollBarsView.h>
#import <WebKit/WebException.h>
#import <WebKit/WebHTMLViewPrivate.h>
#import <WebKit/WebNSViewExtras.h>
#import <WebKit/WebController.h>
#import <WebKit/WebBridge.h>
#import <WebKit/WebFrameBridge.h>
#import <WebKit/WebDataSourcePrivate.h>
#import <WebKit/WebFrame.h>
#import <WebKit/WebViewPrivate.h>
#import <WebKit/WebKitDebug.h>

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

    // We added add/remove this view as a mouse moved observer when its window becomes/resigns main.
    [[NSNotificationCenter defaultCenter] addObserver: self selector: @selector(windowDidBecomeMain:) name: NSWindowDidBecomeMainNotification object: nil];
    [[NSNotificationCenter defaultCenter] addObserver: self selector: @selector(windowDidResignMain:) name: NSWindowDidResignMainNotification object: nil];

    // Add this view initially as a mouse move observer.  Subsequently we will add/remove this view
    // when the window becomes/resigns main.
    [[NSNotificationCenter defaultCenter] addObserver: self selector: @selector(mouseMovedNotification:) name: NSMouseMovedNotification object: nil];

    return self;
}

- (BOOL)validateUserInterfaceItem:(id <NSValidatedUserInterfaceItem>)item 
{
    SEL action = [item action];
    if (action == @selector(copy:)) {
        return [[[self _bridge] selectedText] length] != 0;
    }
    return YES;
}


- (void)copy:(id)sender
{
    WebBridge *bridge = [self _bridge];
    NSPasteboard *pboard = [NSPasteboard generalPasteboard];
    
    [pboard declareTypes:[NSArray arrayWithObjects:NSStringPboardType, nil] owner:nil];
    [pboard setString:[bridge selectedText] forType:NSStringPboardType];
}


- (void)selectAll: sender
{
    WebBridge *bridge = [self _bridge];
    [bridge selectAll];
}


- (void)dealloc 
{
    [self _reset];
    [[NSNotificationCenter defaultCenter] removeObserver: self];
    [_private release];
    [super dealloc];
}


- (BOOL)acceptsFirstResponder
{
    return YES;
}


- (void)removeNotifications
{
    [[NSNotificationCenter defaultCenter] removeObserver: self name: NSMouseMovedNotification object: nil];
    [[NSNotificationCenter defaultCenter] removeObserver: self name: NSWindowDidResignMainNotification object: nil];
    [[NSNotificationCenter defaultCenter] removeObserver: self name: NSWindowDidResignMainNotification object: nil];
}


- (void)viewDidMoveToWindow
{
    if ([self window]) {
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


// This method is typically called by the view's controller when
// the data source is changed.
- (void)provisionalDataSourceChanged:(WebDataSource *)dataSource 
{
    [[dataSource _bridge]
        createKHTMLViewWithNSView:[[[dataSource webFrame] webView] documentView]
	width:(int)[self frame].size.width height:(int)[self frame].size.height
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
    WEBKITDEBUGLEVEL(WEBKIT_LOG_TIMING, "%s apply style seconds = %f\n", [self URL], thisTime);
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

    WEBKITDEBUGLEVEL(WEBKIT_LOG_VIEW, "%s doing layout\n", DEBUG_OBJECT(self));
    [[self _bridge] forceLayout];
    _private->needsLayout = NO;

#ifdef _KWQ_TIMING        
    double thisTime = CFAbsoluteTimeGetCurrent() - start;
    WEBKITDEBUGLEVEL(WEBKIT_LOG_TIMING, "%s layout seconds = %f\n", [self URL], thisTime);
#endif
}


// Stop animating animated GIFs, etc.
- (void)stopAnimations
{
    [NSException raise:WebMethodNotYetImplemented format:@"WebView::stopAnimations is not implemented"];
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

// Returns an array of built-in context menu items for this node.
// Generally called by WebContextMenuHandlers from contextMenuItemsForNode:
#ifdef TENTATIVE_API
- (NSArray *)defaultContextMenuItemsForNode: (WebDOMNode *)
{
    [NSException raise:WebMethodNotYetImplemented format:@"WebView::defaultContextMenuItemsForNode: is not implemented"];
    return nil;
}
#endif

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
- (void)searchFor: (NSString *)string direction: (BOOL)forward caseSensitive: (BOOL)caseFlag
{
    [NSException raise:WebMethodNotYetImplemented format:@"WebView::searchFor:direction:caseSensitive: is not implemented"];
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
    WEBKITDEBUGLEVEL (WEBKIT_LOG_VIEW, "%s flag = %d\n", DEBUG_OBJECT(self), (int)flag);
    [super setNeedsDisplay: flag];
}


- (void)setNeedsLayout: (bool)flag
{
    WEBKITDEBUGLEVEL (WEBKIT_LOG_VIEW, "%s flag = %d\n", DEBUG_OBJECT(self), (int)flag);
    _private->needsLayout = flag;
}


- (void)setNeedsToApplyStyles: (bool)flag
{
    WEBKITDEBUGLEVEL (WEBKIT_LOG_VIEW, "%s flag = %d\n", DEBUG_OBJECT(self), (int)flag);
    _private->needsToApplyStyles = flag;
}


// This should eventually be removed.
- (void)drawRect:(NSRect)rect
{
    WEBKITDEBUGLEVEL (WEBKIT_LOG_VIEW, "%s drawing\n", DEBUG_OBJECT(self));

    [self reapplyStyles];

    [self layout];

#ifdef _KWQ_TIMING
    double start = CFAbsoluteTimeGetCurrent();
#endif
    
    //double start = CFAbsoluteTimeGetCurrent();
    [[self _bridge] drawRect:rect];
    //WebKitDebugAtLevel (WEBKIT_LOG_TIMING, "draw time %e\n", CFAbsoluteTimeGetCurrent() - start);

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
    WEBKITDEBUGLEVEL (WEBKIT_LOG_TIMING, "%s draw seconds = %f\n", widget->part()->baseURL().url().latin1(), thisTime);
#endif
}

- (BOOL)isFlipped 
{
    return YES;
}


- (void)windowDidBecomeMain: (NSNotification *)notification
{
    if ([notification object] == [self window])
        [[NSNotificationCenter defaultCenter] addObserver: self selector: @selector(mouseMovedNotification:) name: NSMouseMovedNotification object: nil];
}


- (void)windowDidResignMain: (NSNotification *)notification
{
    if ([notification object] == [self window])
        [[NSNotificationCenter defaultCenter] removeObserver: self name: NSMouseMovedNotification object: nil];
}

- (void)mouseUp: (NSEvent *)event
{
    [[self _bridge] mouseUp:event];
}

- (void)mouseDown: (NSEvent *)event
{
    [[self _bridge] mouseDown:event];
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

- (void)mouseDragged:(NSEvent *)event
{
    [self autoscroll:event];
    [[self _bridge] mouseDragged:event];
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
    WEBKITDEBUGLEVEL(WEBKIT_LOG_EVENTS, "keyDown: %s\n", DEBUG_OBJECT(event));
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
    WEBKITDEBUGLEVEL(WEBKIT_LOG_EVENTS, "keyUp: %s\n", DEBUG_OBJECT(event));
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

@end
