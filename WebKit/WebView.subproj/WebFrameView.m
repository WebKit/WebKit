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

    _viewPrivate = [[IFWebViewPrivate alloc] init];

    ((IFWebViewPrivate *)_viewPrivate)->isFlipped = YES;
    ((IFWebViewPrivate *)_viewPrivate)->needsLayout = YES;

    return self;
}


- (void)dealloc 
{
    [_viewPrivate release];
}

 
// Set and get the controller.  Note that the controller is not retained.
// Perhaps setController: should be private?
- (id <IFWebController>)controller
{
    return ((IFWebViewPrivate *)_viewPrivate)->controller;
}



// This method is typically called by the view's controller when
// the data source is changed.
- (void)dataSourceChanged: (IFWebDataSource *)dataSource 
{
    IFWebViewPrivate *data = ((IFWebViewPrivate *)_viewPrivate);
    NSRect r = [self frame];
    
    // Only delete the widget if we're the top level widget.  In other
    // cases the widget is associated with a RenderFrame which will
    // delete it's widget.
    if ([dataSource isMainDocument] && data->widget)
        delete data->widget;

    // Nasty!  Setup the cross references between the KHTMLView and
    // the KHTMLPart.
    KHTMLPart *part = [dataSource _part];

    data->widget = new KHTMLView (part, 0);
    part->setView (data->widget);

    // Check to see if we're a frame.
    if ([self _frameScrollView])
        data->widget->setView ([self _frameScrollView]);
    else
        data->widget->setView (self);
    
    data->widget->resize (r.size.width,r.size.height);

    [self _resetView];
    [self layout];
}



// This method should not be public until we have a more completely
// understood way to subclass IFWebView.
- (void)layout
{
    KHTMLView *widget = ((IFWebViewPrivate *)_viewPrivate)->widget;
    if (widget->part()->xmlDocImpl() && 
        widget->part()->xmlDocImpl()->renderer()){
        if (((IFWebViewPrivate *)_viewPrivate)->needsLayout){
            //double start = CFAbsoluteTimeGetCurrent();
            widget->layout(TRUE);
            //WebKitDebugAtLevel (0x200, "layout time %e\n", CFAbsoluteTimeGetCurrent() - start);
            ((IFWebViewPrivate *)_viewPrivate)->needsLayout = NO;
        }
    }
}


// Stop animating animated GIFs, etc.
- (void)stopAnimations
{
    [NSException raise:IFMethodNotYetImplemented format:@"IFWebView::stopAnimations is not implemented"];
}


// Font API
- (void)setFontSizes: (NSArray *)sizes
{
    [NSException raise:IFMethodNotYetImplemented format:@"IFWebView::setFontSizes: is not implemented"];
}


- (NSArray *)fontSizes
{
    [NSException raise:IFMethodNotYetImplemented format:@"IFWebView::fontSizes is not implemented"];
    return nil;
}



- (void)resetFontSizes
{
    [NSException raise:IFMethodNotYetImplemented format:@"IFWebView::resetFontSizes is not implemented"];
}


- (void)setStandardFont: (NSFont *)font
{
    [NSException raise:IFMethodNotYetImplemented format:@"IFWebView::setStandardFont: is not implemented"];
}


- (NSFont *)standardFont
{
    [NSException raise:IFMethodNotYetImplemented format:@"IFWebView::standardFont is not implemented"];
    return nil;
}


- (void)setFixedFont: (NSFont *)font
{
    [NSException raise:IFMethodNotYetImplemented format:@"IFWebView::setFixedFont: is not implemented"];
}


- (NSFont *)fixedFont
{
    [NSException raise:IFMethodNotYetImplemented format:@"IFWebView::fixedFont is not implemented"];
    return nil;
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


- (void)setNeedsLayout: (bool)flag
{
    ((IFWebViewPrivate *)_viewPrivate)->needsLayout = flag;
}


// This should eventually be removed.
- (void)drawRect:(NSRect)rect {
    KHTMLView *widget = ((IFWebViewPrivate *)_viewPrivate)->widget;

    if (widget != 0l){        
        [self layout];

        QPainter p(widget);    
        
        [self lockFocus];

        //double start = CFAbsoluteTimeGetCurrent();
        ((KHTMLView *)widget)->drawContents( &p, (int)rect.origin.x, 
                    (int)rect.origin.y, 
                    (int)rect.size.width, 
                    (int)rect.size.height );
        //WebKitDebugAtLevel (0x200, "draw time %e\n", CFAbsoluteTimeGetCurrent() - start);

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
    }
}

- (void)setIsFlipped: (bool)flag
{
    ((IFWebViewPrivate *)_viewPrivate)->isFlipped = flag;
}


- (BOOL)isFlipped 
{
    return ((IFWebViewPrivate *)_viewPrivate)->isFlipped;
}



// Override superclass implementation.  We want to relayout when the frame size is changed.
- (void)setFrame:(NSRect)frameRect
{
    [super setFrame:frameRect];
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
    
    QMouseEvent *kEvent = new QMouseEvent(QEvent::MouseButtonPress, QPoint(p.x, p.y), button, state);
    ((IFWebViewPrivate *)_viewPrivate)->widget->viewportMouseReleaseEvent(kEvent);
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
    
    QMouseEvent *kEvent = new QMouseEvent(QEvent::MouseButtonPress, QPoint(p.x, p.y), button, state);
    ((IFWebViewPrivate *)_viewPrivate)->widget->viewportMousePressEvent(kEvent);
}

- (void)mouseDragged: (NSEvent *)event
{
    NSPoint p = [event locationInWindow];
    //WebKitDebugAtLevel (0x200, "mouseDragged %f, %f\n", p.x, p.y);
}

@end
