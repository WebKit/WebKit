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

@interface NSScrollView (NSPrivate)
- (void)_adjustForGrowBox;
@end

@implementation IFHTMLView

- initWithFrame: (NSRect) frame
{
    [super initWithFrame: frame];
    
    _private = [[IFHTMLViewPrivate alloc] init];

    _private->isFlipped = YES;
    _private->needsLayout = YES;

    _private->canDragTo = YES;
    _private->canDragFrom = YES;

    // We added add/remove this view as a mouse moved observer when it's window becomes/resigns main.
    [[NSNotificationCenter defaultCenter] addObserver: self selector: @selector(windowDidBecomeMain:) name: NSWindowDidBecomeMainNotification object: nil];
    [[NSNotificationCenter defaultCenter] addObserver: self selector: @selector(windowDidResignMain:) name: NSWindowDidResignMainNotification object: nil];

    // Add this view initially as a mouse move observer.  Subsequently we will add/remove this view
    // when the window becomes/resigns main.
    [[NSNotificationCenter defaultCenter] addObserver: self selector: @selector(mouseMovedNotification:) name: NSMouseMovedNotification object: nil];

    [[NSNotificationCenter defaultCenter] addObserver: self selector: @selector(windowResized:) name: NSWindowDidResizeNotification object: nil];

    // We remove this view as an observer from all window notifications when the window
    // is closed.  This may be redundant, but ensures that the view has no outstanding
    // references.
    [[NSNotificationCenter defaultCenter] addObserver: self selector: @selector(windowWillClose:) name: NSWindowWillCloseNotification object: nil];

    return self;
}


- (void)dealloc 
{
    [self _stopPlugins];
    [[NSNotificationCenter defaultCenter] removeObserver: self];
    [_private release];
    [super dealloc];
}


- (BOOL)acceptsFirstResponder
{
    return YES;
}


- (void)removeFromSuperview
{
    [self _stopPlugins];
    [super removeFromSuperview];
}


- (void)removeFromSuperviewWithoutNeedingDisplay
{
    [self _stopPlugins];
    [super removeFromSuperviewWithoutNeedingDisplay];
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

    // Only delete the widget if we're the top level widget.  In other
    // cases the widget is associated with a RenderFrame which will
    // delete it's widget.
    if ([dataSource isMainDocument] && data->widget)
        delete data->widget;

    data->widget = data->provisionalWidget;
    data->provisionalWidget = 0;
}

- (void)dataSourceUpdated: (IFWebDataSource *)dataSource
{

}

- (void)reapplyStyles
{
    KHTMLView *widget = _private->widget;

    if (widget->part()->xmlDocImpl() && 
        widget->part()->xmlDocImpl()->renderer()){
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

    if (widget->part()->xmlDocImpl() && 
        widget->part()->xmlDocImpl()->renderer()){
        if (_private->needsLayout){
#ifdef _KWQ_TIMING        
    double start = CFAbsoluteTimeGetCurrent();
#endif

            WEBKITDEBUGLEVEL (WEBKIT_LOG_VIEW, "doing layout\n");
            //double start = CFAbsoluteTimeGetCurrent();
            widget->layout();
            //WebKitDebugAtLevel (WEBKIT_LOG_TIMING, "layout time %e\n", CFAbsoluteTimeGetCurrent() - start);
            _private->needsLayout = NO;
#ifdef _KWQ_TIMING        
    double thisTime = CFAbsoluteTimeGetCurrent() - start;
    WEBKITDEBUGLEVEL (WEBKIT_LOG_TIMING, "%s layout seconds = %f\n", widget->part()->baseURL().url().latin1(), thisTime);
#endif
        }
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


#ifdef DELAY_LAYOUT
- delayLayout: sender
{
    [NSObject cancelPreviousPerformRequestsWithTarget: self selector: @selector(delayLayout:) object: self];
    WEBKITDEBUG("KWQHTMLView:  delayLayout called\n");
    [self setNeedsLayout: YES];
    [self setNeedsDisplay: YES];
}

-(void)notificationReceived:(NSNotification *)notification
{
    if ([[notification name] rangeOfString: @"uri-fin-"].location == 0){
        WEBKITDEBUG1("KWQHTMLView: Received notification, %s\n", DEBUG_OBJECT([notification name]));
        [self performSelector:@selector(delayLayout:) withObject:self afterDelay:(NSTimeInterval)0.5];
    }
}
#else
-(void)notificationReceived:(NSNotification *)notification
{
    if ([[notification name] rangeOfString: @"uri-fin-"].location == 0){
        [self setNeedsLayout: YES];
        [self setNeedsDisplay: YES];
    }
}
#endif

- (void)setNeedsDisplay:(BOOL)flag
{
    WEBKITDEBUGLEVEL (WEBKIT_LOG_VIEW, "flag = %d\n", (int)flag);
    [super setNeedsDisplay: flag];
}


- (void)setNeedsLayout: (bool)flag
{
    WEBKITDEBUGLEVEL (WEBKIT_LOG_VIEW, "flag = %d\n", (int)flag);
    _private->needsLayout = flag;
}


- (void)setNeedsToApplyStyles: (bool)flag
{
    WEBKITDEBUGLEVEL (WEBKIT_LOG_VIEW, "flag = %d\n", (int)flag);
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
    if (widget == 0l) {
        [[NSColor whiteColor] set];
        NSRectFill(rect);
        return;
    }

    WEBKITDEBUGLEVEL (WEBKIT_LOG_VIEW, "drawing\n");

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

- (void)setIsFlipped: (bool)flag
{
    _private->isFlipped = flag;
}


- (BOOL)isFlipped 
{
    return _private->isFlipped;
}


- (void)viewWillStartLiveResize
{
    [super viewWillStartLiveResize];
}

- (void)viewDidEndLiveResize
{
    id scrollView = [[self superview] superview];

    [super viewDidEndLiveResize];
    
    if ([scrollView isKindOfClass: [NSScrollView class]]){
        [scrollView updateScrollers];
        [scrollView tile];
        [scrollView setNeedsDisplay: YES];
    }

    [self setNeedsLayout: YES];
    [self setNeedsDisplay: YES];
}


- (void)windowWillClose: (NSNotification *)notification
{
    [[NSNotificationCenter defaultCenter] removeObserver: self name: NSMouseMovedNotification object: nil];
    [[NSNotificationCenter defaultCenter] removeObserver: self name: NSWindowDidResignMainNotification object: nil];
    [[NSNotificationCenter defaultCenter] removeObserver: self name: NSWindowDidResignMainNotification object: nil];
    [[NSNotificationCenter defaultCenter] removeObserver: self name: NSWindowDidResizeNotification object: nil];
    [[NSNotificationCenter defaultCenter] removeObserver: self name: NSWindowWillCloseNotification object: nil];
}


- (void)windowResized: (NSNotification *)notification
{
    if ([notification object] == [self window]){
        [self setNeedsLayout: YES];
        [self setNeedsDisplay: YES];
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
    if (widget != 0l) {
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
    if (widget != 0l) {
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
        QMouseEvent kEvent(QEvent::MouseButtonPress, QPoint((int)p.x, (int)p.y), 0, 0);
        KHTMLView *widget = _private->widget;
        if (widget != 0l) {
            widget->viewportMouseMoveEvent(&kEvent);
        }
    }
}

- (void)mouseDragged: (NSEvent *)event
{
    NSPoint p = [event locationInWindow];
    //WebKitDebugAtLevel (WEBKIT_LOG_EVENTS, "mouseDragged %f, %f\n", p.x, p.y);
}

- (void)keyDown: (NSEvent *)event
{
    WEBKITDEBUGLEVEL(WEBKIT_LOG_EVENTS, "keyDown: %s\n", DEBUG_OBJECT(event));
    int state = 0;
    
    [self _addModifiers:[event modifierFlags] toState:&state];
    QKeyEvent kEvent(QEvent::KeyPress, 0, 0, state, NSSTRING_TO_QSTRING([event characters]), [event isARepeat], 1);
    
    KHTMLView *widget = _private->widget;
    if (widget != 0l)
        widget->keyPressEvent(&kEvent);
}


- (void)keyUp: (NSEvent *)event
{
    WEBKITDEBUGLEVEL(WEBKIT_LOG_EVENTS, "keyUp: %s\n", DEBUG_OBJECT(event));
    int state = 0;
    
    [self _addModifiers:[event modifierFlags] toState:&state];
    QKeyEvent kEvent(QEvent::KeyPress, 0, 0, state, NSSTRING_TO_QSTRING([event characters]), [event isARepeat], 1);
    
    KHTMLView *widget = _private->widget;
    if (widget != 0l)
        widget->keyReleaseEvent(&kEvent);
}

- (void)setCursor:(NSCursor *)cursor
{
    [_private->cursor release];
    _private->cursor = [cursor retain];

    // We have to make both of these calls, because:
    // - Just setting a cursor rect will have no effect, if the mouse cursor is already
    //   inside the area of the rect.
    // - Just calling invalidateCursorRectsForView will not call resetCursorRects if
    //   there is no cursor rect set currently and the view has no subviews.
    // Therefore we have to call resetCursorRects to ensure that a cursor rect is set
    // at all, if we are going to want one, and then invalidateCursorRectsForView: to
    // call resetCursorRects from the proper context that will
    // actually result in updating the cursor.
    [self resetCursorRects];
    [[self window] invalidateCursorRectsForView:self];
}

- (void)resetCursorRects
{
    if (_private->cursor != nil && _private->cursor != [NSCursor arrowCursor]) {
        [self addCursorRect:[self visibleRect] cursor:_private->cursor];
    }
}

@end
