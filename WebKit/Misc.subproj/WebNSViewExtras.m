/*
        WebNSViewExtras.m
        Copyright (c) 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebNSImageExtras.h>
#import <WebKit/WebNSPasteboardExtras.h>
#import <WebKit/WebNSViewExtras.h>
#import <WebKit/WebView.h>

#import <WebFoundation/WebNSStringExtras.h>
#import <WebFoundation/WebNSURLExtras.h>

#define WebDragImageAlpha    			0.75
#define WebMaxDragImageSize 			NSMakeSize(400, 400)

#ifdef DEBUG_VIEWS
@interface NSObject (Foo)
- (void*)_renderFramePart;
- (id)frameForView: (id)aView;
- (id)_controller;
@end
#endif

@implementation NSView (WebExtras)

- (NSView *)_web_superviewOfClass:(Class)class
{
    NSView *view;

    view = self;
    while ((view = [view superview]) != nil) {
        if ([view isKindOfClass:class]) {
            return view;
        }
    }

    return nil;
}

- (WebView *)_web_parentWebView
{
    WebView *view = (WebView *)[[[self superview] superview] superview];
    
    if ([view isKindOfClass: [WebView class]])
        return view;
    return nil;
}

/* Determine whether a mouse down should turn into a drag; started as copy of NSTableView code */
- (BOOL)_web_dragShouldBeginFromMouseDown:(NSEvent *)mouseDownEvent
                           withExpiration:(NSDate *)expiration
                              xHysteresis:(float)xHysteresis
                              yHysteresis:(float)yHysteresis
{
    NSEvent *nextEvent, *firstEvent, *dragEvent, *mouseUp;
    BOOL dragIt;

    if ([mouseDownEvent type] != NSLeftMouseDown) {
        return NO;
    }

    nextEvent = nil;
    firstEvent = nil;
    dragEvent = nil;
    mouseUp = nil;
    dragIt = NO;

    while ((nextEvent = [[self window] nextEventMatchingMask:(NSLeftMouseUpMask | NSLeftMouseDraggedMask)
                                                   untilDate:expiration
                                                      inMode:NSEventTrackingRunLoopMode
                                                     dequeue:YES]) != nil) {
        if (firstEvent == nil) {
            firstEvent = nextEvent;
        }

        if ([nextEvent type] == NSLeftMouseDragged) {
            float deltax = ABS([nextEvent locationInWindow].x - [mouseDownEvent locationInWindow].x);
            float deltay = ABS([nextEvent locationInWindow].y - [mouseDownEvent locationInWindow].y);
            dragEvent = nextEvent;

            if (deltax >= xHysteresis) {
                dragIt = YES;
                break;
            }

            if (deltay >= yHysteresis) {
                dragIt = YES;
                break;
            }
        } else if ([nextEvent type] == NSLeftMouseUp) {
            mouseUp = nextEvent;
            break;
        }
    }

    // Since we've been dequeuing the events (If we don't, we'll never see the mouse up...),
    // we need to push some of the events back on.  It makes sense to put the first and last
    // drag events and the mouse up if there was one.
    if (mouseUp != nil) {
        [NSApp postEvent:mouseUp atStart:YES];
    }
    if (dragEvent != nil) {
        [NSApp postEvent:dragEvent atStart:YES];
    }
    if (firstEvent != mouseUp && firstEvent != dragEvent) {
        [NSApp postEvent:firstEvent atStart:YES];
    }

    return dragIt;
}

- (BOOL)_web_dragShouldBeginFromMouseDown:(NSEvent *)mouseDownEvent
                           withExpiration:(NSDate *)expiration
{
    return [self _web_dragShouldBeginFromMouseDown:mouseDownEvent
                                    withExpiration:expiration
                                       xHysteresis:WebDragStartHysteresisX
                                       yHysteresis:WebDragStartHysteresisY];
}


- (NSDragOperation)_web_dragOperationForDraggingInfo:(id <NSDraggingInfo>)sender
{
    if([sender draggingSource] != self && [[sender draggingPasteboard] _web_bestURL]) {
        return NSDragOperationCopy;
    } else {
        return NSDragOperationNone;
    }
}

#ifdef DEBUG_VIEWS
- (void)_web_printViewHierarchy: (int)level
{
    NSArray *subviews;
    int _level = level, i;
    NSRect f;
    NSView *subview;
    void *rfp = 0;
    
    subviews = [self subviews];
    _level = level;
    while (_level-- > 0)
        printf (" ");
    f = [self frame];
    
    if ([self respondsToSelector: @selector(_controller)]){
        id aController = [self _controller];
        id aFrame = [aController frameForView: self];
        rfp = [aFrame _renderFramePart];
    }
    
    printf ("%s renderFramePart %p (%f,%f) w %f, h %f\n", [[[self class] className] cString], rfp, f.origin.x, f.origin.y, f.size.width, f.size.height);
    for (i = 0; i < (int)[subviews count]; i++){
        subview = [subviews objectAtIndex: i];
        [subview _web_printViewHierarchy: level + 1];
    }
}
#endif

- (void)_web_dragPromisedImage:(NSImage *)image
                    fromOrigin:(NSPoint)origin
                       withURL:(NSURL *)URL
                         title:(NSString *)title
                         event:(NSEvent *)event
{
    image = [[image copy] autorelease];
    
    NSSize originalSize = [image size];
    [image _web_scaleToMaxSize:WebMaxDragImageSize];
    NSSize newSize = [image size];

    [image _web_dissolveToFraction:WebDragImageAlpha];

    NSPoint mouseDownPoint = [self convertPoint:[event locationInWindow] fromView:nil];
    NSPoint currentPoint = [self convertPoint:[[_window currentEvent] locationInWindow] fromView:nil];

    // Properly orient the drag image and orient it differently if it's smaller than the original
    origin.x = mouseDownPoint.x - (((mouseDownPoint.x - origin.x) / originalSize.width) * newSize.width);
    origin.y = origin.y + originalSize.height;
    origin.y = mouseDownPoint.y - (((mouseDownPoint.y - origin.y) / originalSize.height) * newSize.height);

    NSSize offset = NSMakeSize(currentPoint.x - mouseDownPoint.x, currentPoint.y - mouseDownPoint.y);

    NSArray *filesTypes = [NSArray arrayWithObject:[[URL path] pathExtension]];
    
    NSPasteboard *pboard = [NSPasteboard pasteboardWithName:NSDragPboard];
    [pboard declareTypes:[NSArray arrayWithObjects:NSFilesPromisePboardType, NSTIFFPboardType, nil] owner:self];
    [pboard setPropertyList:filesTypes forType:NSFilesPromisePboardType];
    [pboard setData:[image TIFFRepresentation] forType:NSTIFFPboardType];
    [pboard _web_writeURL:URL andTitle:title withOwner:self];

    id source = [[NSFilePromiseDragSource alloc] initWithSource:(id)self];
    [source setTypes:filesTypes onPasteboard:pboard];
    
    [self dragImage:image at:origin offset:offset event:event pasteboard:pboard source:source slideBack:YES];
}

@end

@implementation WebFilePromiseDragSource

- initWithSource:(id)draggingSource
{
    [super initWithSource:draggingSource];
    _draggingSource = draggingSource;
    return self;
}

- (id)draggingSource
{
    return _draggingSource;
}

- (void)dealloc
{
    //[super dealloc];
}

@end;