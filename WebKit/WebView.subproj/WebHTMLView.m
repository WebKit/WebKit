/*	
    IFHTMLView.mm
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/IFDynamicScrollBarsView.h>
#import <WebKit/IFException.h>
#import <WebKit/IFHTMLViewPrivate.h>
#import <WebKit/IFNSViewExtras.h>
#import <WebKit/IFWebController.h>
#import <WebKit/IFWebCoreBridge.h>
#import <WebKit/IFWebDataSourcePrivate.h>
#import <WebKit/IFWebFrame.h>
#import <WebKit/IFWebViewPrivate.h>
#import <WebKit/WebKitDebug.h>

// Needed for the mouse move notification.
#import <AppKit/NSResponder_Private.h>

// KDE related includes
#import <khtmlview.h>
#import <qpainter.h>
#import <qevent.h>
#import <html/html_documentimpl.h>

#import <KWQKHTMLPartImpl.h>

@implementation IFHTMLView

- initWithFrame: (NSRect) frame
{
    [super initWithFrame: frame];
    
    _private = [[IFHTMLViewPrivate alloc] init];

    _private->needsLayout = YES;

    _private->canDragTo = YES;
    _private->canDragFrom = YES;

    // We added add/remove this view as a mouse moved observer when its window becomes/resigns main.
    [[NSNotificationCenter defaultCenter] addObserver: self selector: @selector(windowDidBecomeMain:) name: NSWindowDidBecomeMainNotification object: nil];
    [[NSNotificationCenter defaultCenter] addObserver: self selector: @selector(windowDidResignMain:) name: NSWindowDidResignMainNotification object: nil];

    // Add this view initially as a mouse move observer.  Subsequently we will add/remove this view
    // when the window becomes/resigns main.
    [[NSNotificationCenter defaultCenter] addObserver: self selector: @selector(mouseMovedNotification:) name: NSMouseMovedNotification object: nil];

    [[NSNotificationCenter defaultCenter] addObserver: self selector: @selector(windowResized:) name: NSWindowDidResizeNotification object: nil];

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
    IFWebCoreBridge *bridge = [self _bridge];
    NSPasteboard *pboard = [NSPasteboard generalPasteboard];
    
    [pboard declareTypes:[NSArray arrayWithObjects:NSStringPboardType, nil] owner:nil];
    [pboard setString:[bridge selectedText] forType:NSStringPboardType];
}


- (void)selectAll: sender
{
    IFWebCoreBridge *bridge = [self _bridge];
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
    [[NSNotificationCenter defaultCenter] removeObserver: self name: NSWindowDidResizeNotification object: nil];
}


- (void)viewWillMoveToWindow:(NSWindow *)window
{
    if ([self window] && !window) {
        [self removeNotifications];
        [self _reset];
    }
    [super viewWillMoveToWindow:window];
}


// This method is typically called by the view's controller when
// the data source is changed.
- (void)provisionalDataSourceChanged:(IFWebDataSource *)dataSource 
{
    IFWebCoreBridge *bridge = [dataSource _bridge];

    // Create a temporary provisional view.  It will be replaced with
    // the actual view once the datasource has been committed.
    IFHTMLView *provisionalView = [[IFHTMLView alloc] initWithFrame:NSMakeRect(0,0,0,0)];
    
    NSRect r = [self frame];
    
    _private->provisionalWidget = [bridge createKHTMLViewWithNSView:provisionalView
        width:(int)r.size.width height:(int)r.size.height
        marginWidth:[[[dataSource webFrame] webView] _marginWidth]
        marginHeight:[[[dataSource webFrame] webView] _marginHeight]];
    
    [provisionalView release];
}

- (void)provisionalDataSourceCommitted: (IFWebDataSource *)dataSource 
{
    IFHTMLViewPrivate *data = _private;
    IFWebView *webView = [self _IF_parentWebView];
    id frameScrollView = [webView frameScrollView];
    
    data->provisionalWidget->setView (frameScrollView);

    if (data->widgetOwned)
        delete data->widget;

    data->widget = data->provisionalWidget;
    data->widgetOwned = YES;
    data->provisionalWidget = 0;
}

- (void)dataSourceUpdated: (IFWebDataSource *)dataSource
{
}

- (void)reapplyStyles
{
    KHTMLView *widget = _private->widget;

    if (widget && widget->part()->xmlDocImpl() && 
        widget->part()->xmlDocImpl()->renderer()) {
        if (_private->needsToApplyStyles){
#ifdef _KWQ_TIMING        
    double start = CFAbsoluteTimeGetCurrent();
#endif
            widget->part()->xmlDocImpl()->updateStyleSelector();
            _private->needsToApplyStyles = NO;
#ifdef _KWQ_TIMING        
    double thisTime = CFAbsoluteTimeGetCurrent() - start;
    WEBKITDEBUGLEVEL (WEBKIT_LOG_TIMING, "%s apply style seconds = %f\n", widget->part()->baseURL().url().latin1(), thisTime);
#endif
        }
    }

}


// This method should not be public until we have more completely
// understood how IFWebView will be subclassed.
- (void)layout
{
    KHTMLView *widget = _private->widget;

    // Ensure that we will receive mouse move events.  Is this the best place to put this?
    [[self window] setAcceptsMouseMovedEvents: YES];
    [[self window] _setShouldPostEventNotifications: YES];

    if (widget && widget->part()->xmlDocImpl() && 
        widget->part()->xmlDocImpl()->renderer() &&
        _private->needsLayout){
 #ifdef _KWQ_TIMING        
    double start = CFAbsoluteTimeGetCurrent();
 #endif

        WEBKITDEBUGLEVEL (WEBKIT_LOG_VIEW, "%s doing layout\n", DEBUG_OBJECT(self));
        widget->layout();
        _private->needsLayout = NO;
 #ifdef _KWQ_TIMING        
    double thisTime = CFAbsoluteTimeGetCurrent() - start;
    WEBKITDEBUGLEVEL (WEBKIT_LOG_TIMING, "%s layout seconds = %f\n", widget->part()->baseURL().url().latin1(), thisTime);
 #endif
    }
    else {
        WEBKITDEBUGLEVEL (WEBKIT_LOG_VIEW, "%s NOT doing layout\n", DEBUG_OBJECT(self));
    }

}


// Stop animating animated GIFs, etc.
- (void)stopAnimations
{
    [NSException raise:IFMethodNotYetImplemented format:@"IFWebView::stopAnimations is not implemented"];
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
// Generally called by IFContextMenuHandlers from contextMenuItemsForNode:
#ifdef TENTATIVE_API
- (NSArray *)defaultContextMenuItemsForNode: (IFDOMNode *)
{
    [NSException raise:IFMethodNotYetImplemented format:@"IFWebView::defaultContextMenuItemsForNode: is not implemented"];
    return nil;
}
#endif

- (void)setContextMenusEnabled: (BOOL)flag
{
    [NSException raise:IFMethodNotYetImplemented format:@"IFWebView::setContextMenusEnabled: is not implemented"];
}


- (BOOL)contextMenusEnabled;
{
    return NO;
}


// Remove the selection.
- (void)deselectText
{
    [NSException raise:IFMethodNotYetImplemented format:@"IFWebView::deselectText: is not implemented"];
}



// Search from the end of the currently selected location, or from the beginning of the document if nothing
// is selected.
- (void)searchFor: (NSString *)string direction: (BOOL)forward caseSensitive: (BOOL)caseFlag
{
    [NSException raise:IFMethodNotYetImplemented format:@"IFWebView::searchFor:direction:caseSensitive: is not implemented"];
}


// Get an attributed string that represents the current selection.
- (NSAttributedString *)selectedText
{
    [NSException raise:IFMethodNotYetImplemented format:@"IFWebView::selectedText is not implemented"];
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
- (void)drawRect:(NSRect)rect {
    KHTMLView *widget = _private->widget;
    //IFWebViewPrivate *data = _private;

    //if (data->provisionalWidget != 0){
    //    WEBKITDEBUGLEVEL (WEBKIT_LOG_VIEW, "not drawing, frame in provisional state.\n");
    //    return;
    //}
    
    // Draw plain white bg in empty case, to avoid redraw weirdness when
    // no page is yet loaded (2890818). We may need to modify this to always
    // draw the background color, in which case we'll have to make sure the
    // no-widget case is still handled correctly.
    if (widget == 0) {
        [[NSColor whiteColor] set];
        NSRectFill(rect);
        return;
    }

    WEBKITDEBUGLEVEL (WEBKIT_LOG_VIEW, "%s drawing\n", DEBUG_OBJECT(self));

    [self reapplyStyles];

    [self layout];

#ifdef _KWQ_TIMING
    double start = CFAbsoluteTimeGetCurrent();
#endif
    QPainter p(widget);

    [self lockFocus];

    //double start = CFAbsoluteTimeGetCurrent();
    widget->drawContents( &p, (int)rect.origin.x,
                              (int)rect.origin.y,
                              (int)rect.size.width,
                               (int)rect.size.height );
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

    [self unlockFocus];

#ifdef _KWQ_TIMING
    double thisTime = CFAbsoluteTimeGetCurrent() - start;
    WEBKITDEBUGLEVEL (WEBKIT_LOG_TIMING, "%s draw seconds = %f\n", widget->part()->baseURL().url().latin1(), thisTime);
#endif
}

- (BOOL)isFlipped 
{
    return YES;
}


- (void)windowResized: (NSNotification *)notification
{
    // FIXME: This is a hack. We should relayout when the width of our
    // superview's bounds changes, not when the window is resized.
    if ([notification object] == [self window]) {
        [self setNeedsLayout: YES];
    }
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


- (void)_addModifiers:(unsigned)modifiers toState:(int *)state
{
    if (modifiers & NSControlKeyMask)
        *state |= Qt::ControlButton;
    if (modifiers & NSShiftKeyMask)
        *state |= Qt::ShiftButton;
    if (modifiers & NSAlternateKeyMask)
        *state |= Qt::AltButton;
    // Mapping command to meta is slightly questionable
    if (modifiers & NSCommandKeyMask)
        *state |= Qt::MetaButton;
}

- (void)mouseUp: (NSEvent *)event
{
    int button, state;
     
    if ([event type] == NSLeftMouseUp){
        button = Qt::LeftButton;
        state = Qt::LeftButton;
    }
    else if ([event type] == NSRightMouseUp){
        button = Qt::RightButton;
        state = Qt::RightButton;
    }
    else if ([event type] == NSOtherMouseUp){
        button = Qt::MidButton;
        state = Qt::MidButton;
    }
    else {
        [NSException raise:IFRuntimeError format:@"IFWebView::mouseUp: unknown button type"];
        button = 0; state = 0; // Shutup the compiler.
    }
    NSPoint p = [event locationInWindow];
    
    [self _addModifiers:[event modifierFlags] toState:&state];

    QMouseEvent kEvent(QEvent::MouseButtonPress, QPoint((int)p.x, (int)p.y), button, state);
    KHTMLView *widget = _private->widget;
    if (widget) {
        widget->viewportMouseReleaseEvent(&kEvent);
    }
}

- (void)mouseDown: (NSEvent *)event
{
    int button, state;
     
    if ([event type] == NSLeftMouseDown){
        button = Qt::LeftButton;
        state = Qt::LeftButton;
    }
    else if ([event type] == NSRightMouseDown){
        button = Qt::RightButton;
        state = Qt::RightButton;
    }
    else if ([event type] == NSOtherMouseDown){
        button = Qt::MidButton;
        state = Qt::MidButton;
    }
    else {
        [NSException raise:IFRuntimeError format:@"IFWebView::mouseDown: unknown button type"];
        button = 0; state = 0; // Shutup the compiler.
    }
    NSPoint p = [event locationInWindow];
    
    [self _addModifiers:[event modifierFlags] toState:&state];

    QMouseEvent kEvent(QEvent::MouseButtonPress, QPoint((int)p.x, (int)p.y), button, state);
    KHTMLView *widget = _private->widget;
    if (widget) {
        widget->viewportMousePressEvent(&kEvent);
    }
}

- (void)mouseMovedNotification: (NSNotification *)notification
{
    NSEvent *event = [(NSDictionary *)[notification userInfo] objectForKey: @"NSEvent"];
    NSPoint p = [event locationInWindow];
    NSWindow *thisWindow = [self window];
    
    // Only act on the mouse move event if it's inside this view (and
    // not inside a subview)
    if ([thisWindow isMainWindow] &&
        [[[notification userInfo] objectForKey: @"NSEvent"] window] == thisWindow &&
        [[thisWindow contentView] hitTest:p] == self) {
        QMouseEvent kEvent(QEvent::MouseMove, QPoint((int)p.x, (int)p.y), 0, 0);
        KHTMLView *widget = _private->widget;
        if (widget) {
            widget->viewportMouseMoveEvent(&kEvent);
        }
    }
}

- (void)mouseDragged: (NSEvent *)event
{
    NSPoint p = [event locationInWindow];
    
    [self autoscroll: event];
    
    QMouseEvent kEvent(QEvent::MouseMove, QPoint((int)p.x, (int)p.y), Qt::LeftButton, Qt::LeftButton);
    KHTMLView *widget = _private->widget;
    if (widget) {
        widget->viewportMouseMoveEvent(&kEvent);
    }
}

- (void)keyDown: (NSEvent *)event
{
    WEBKITDEBUGLEVEL(WEBKIT_LOG_EVENTS, "keyDown: %s\n", DEBUG_OBJECT(event));
    int state = 0;
    
    [self _addModifiers:[event modifierFlags] toState:&state];
    QKeyEvent kEvent(QEvent::KeyPress, 0, 0, state, NSSTRING_TO_QSTRING([event characters]), [event isARepeat], 1);
    
    KHTMLView *widget = _private->widget;
    if (widget)
        widget->keyPressEvent(&kEvent);
}


- (void)keyUp: (NSEvent *)event
{
    WEBKITDEBUGLEVEL(WEBKIT_LOG_EVENTS, "keyUp: %s\n", DEBUG_OBJECT(event));
    int state = 0;
    
    [self _addModifiers:[event modifierFlags] toState:&state];
    QKeyEvent kEvent(QEvent::KeyPress, 0, 0, state, NSSTRING_TO_QSTRING([event characters]), [event isARepeat], 1);
    
    KHTMLView *widget = _private->widget;
    if (widget)
        widget->keyReleaseEvent(&kEvent);
}

@end
