/*	IFWebView.mm
	Copyright 2001, Apple, Inc. All rights reserved.
*/
#import <WebKit/IFWebView.h>
#import <WebKit/IFWebViewPrivate.h>
#import <WebKit/IFWebDataSource.h>
#import <WebKit/IFWebDataSourcePrivate.h>
#import <WebKit/IFBaseWebController.h>
#import <WebKit/IFDynamicScrollBarsView.h>
#import <WebKit/IFException.h>
#import <WebKit/WebKitDebug.h>

// KDE related includes
#include <khtmlview.h>
#include <qwidget.h>
#include <qpainter.h>
#include <qevent.h>
#include <html/html_documentimpl.h>


@implementation IFWebView

- initWithFrame: (NSRect) frame
{
    [super initWithFrame: frame];

    _private = [[IFWebViewPrivate alloc] init];

    _private->isFlipped = YES;
    _private->needsLayout = YES;

    [[NSNotificationCenter defaultCenter] addObserver: self selector: @selector(windowResized:) name: NSWindowDidResizeNotification object: nil];
        
    return self;
}


- (void)dealloc 
{
    [self _stopPlugins];
    [[NSNotificationCenter defaultCenter] removeObserver: self];
    [_private release];
    [super dealloc];
}

 
// Note that the controller is not retained.
- (id <IFWebController>)controller
{
    return _private->controller;
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
- (void)provisionalDataSourceChanged: (IFWebDataSource *)dataSource 
{
    NSRect r = [self frame];
    IFWebView *provisionalView;
    
    // Nasty!  Setup the cross references between the KHTMLView and
    // the KHTMLPart.
    KHTMLPart *part = [dataSource _part];

    _private->provisionalWidget = new KHTMLView (part, 0);
    part->setView (_private->provisionalWidget);

    // Create a temporary provisional view.  It will be replaced with
    // the actual view once the datasource has been committed.
    provisionalView = [[IFWebView alloc] initWithFrame: NSMakeRect (0,0,0,0)];
    _private->provisionalWidget->setView (provisionalView);
    [provisionalView release];

    _private->provisionalWidget->resize ((int)r.size.width, (int)r.size.height);
}

- (void)dataSourceChanged: (IFWebDataSource *)dataSource 
{
    IFWebViewPrivate *data = _private;

    // Setup the real view.
    if ([self _frameScrollView])
        data->provisionalWidget->setView ([self _frameScrollView]);
    else
        data->provisionalWidget->setView (self);

    // Only delete the widget if we're the top level widget.  In other
    // cases the widget is associated with a RenderFrame which will
    // delete it's widget.
    if ([dataSource isMainDocument] && data->widget)
        delete data->widget;

    data->widget = data->provisionalWidget;
    data->provisionalWidget = 0;
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
    [NSException raise:IFMethodNotYetImplemented format:@"IFWebView::setCanDragFrom: is not implemented"];
}

- (BOOL)canDragFrom
{
    return NO;
}

- (void)setCanDragTo: (BOOL)flag
{
    [NSException raise:IFMethodNotYetImplemented format:@"IFWebView::setCanDragTo: is not implemented"];
}

- (BOOL)canDragTo
{
    return NO;
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
    KWQDEBUG("KWQHTMLView:  delayLayout called\n");
    [self setNeedsLayout: YES];
    [self setNeedsDisplay: YES];
}

-(void)notificationReceived:(NSNotification *)notification
{
    if ([[notification name] rangeOfString: @"uri-fin-"].location == 0){
        KWQDEBUG1("KWQHTMLView: Received notification, %s\n", DEBUG_OBJECT([notification name]));
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



- (void)windowResized: (NSNotification *)notification
{
    if ([notification object] == [self window])
        [self setNeedsLayout: YES];
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
    
    QMouseEvent *kEvent = new QMouseEvent(QEvent::MouseButtonPress, QPoint((int)p.x, (int)p.y), button, state);
    KHTMLView *widget = _private->widget;
    if (widget != 0l) {
        widget->viewportMouseReleaseEvent(kEvent);
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
        [NSException raise:IFRuntimeError format:@"IFWebView::mouseUp: unknown button type"];
        button = 0; state = 0; // Shutup the compiler.
    }
    NSPoint p = [event locationInWindow];
    
    QMouseEvent *kEvent = new QMouseEvent(QEvent::MouseButtonPress, QPoint((int)p.x, (int)p.y), button, state);
    KHTMLView *widget = _private->widget;
    if (widget != 0l) {
        widget->viewportMousePressEvent(kEvent);
    }
}

- (void)mouseDragged: (NSEvent *)event
{
    NSPoint p = [event locationInWindow];
    //WebKitDebugAtLevel (WEBKIT_LOG_EVENTS, "mouseDragged %f, %f\n", p.x, p.y);
}

@end
