/*
        WebNSViewExtras.m
        Copyright (c) 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebFrameView.h>
#import <WebKit/WebNSImageExtras.h>
#import <WebKit/WebNSPasteboardExtras.h>
#import <WebKit/WebNSViewExtras.h>

#import <WebFoundation/NSString_NSURLExtras.h>
#import <WebFoundation/NSURL_NSURLExtras.h>

#define WebDragStartHysteresisX			5.0
#define WebDragStartHysteresisY			5.0
#define WebMaxDragImageSize 			NSMakeSize(400, 400)

#ifdef DEBUG_VIEWS
@interface NSObject (Foo)
- (void*)_renderFramePart;
- (id)_frameForView: (id)aView;
- (id)_webView;
@end
#endif

@interface NSFilePromiseDragSource : NSObject
- initWithSource:(id)draggingSource;
- (void)setTypes:(NSArray *)types onPasteboard:(NSPasteboard *)pboard;
@end

@implementation NSView (WebExtras)

- (NSView *)_web_superviewOfClass:(Class)class stoppingAtClass:(Class)limitClass
{
    NSView *view = self;
    while ((view = [view superview]) != nil) {
        if ([view isKindOfClass:class]) {
            return view;
        } else if (limitClass && [view isKindOfClass:limitClass]) {
            break;
        }
    }

    return nil;
}

- (NSView *)_web_superviewOfClass:(Class)class
{
    return [self _web_superviewOfClass:class stoppingAtClass:nil];
}

- (WebFrameView *)_web_parentWebFrameView
{
    WebFrameView *view = (WebFrameView *)[[[self superview] superview] superview];
    
    if ([view isKindOfClass: [WebFrameView class]])
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
    
    if ([self respondsToSelector: @selector(_webView)]){
        id aWebView = [self _webView];
        id aFrame = [aWebView _frameForView: self];
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
                          rect:(NSRect)rect
                           URL:(NSURL *)URL
                      fileType:(NSString *)fileType
                         title:(NSString *)title
                         event:(NSEvent *)event
{    
    NSSize originalSize = rect.size;
    NSPoint origin = rect.origin;
    
    NSImage *dragImage = [[image copy] autorelease];
    [dragImage setScalesWhenResized:YES];
    [dragImage setSize:originalSize];

    [dragImage _web_scaleToMaxSize:WebMaxDragImageSize];
    NSSize newSize = [dragImage size];

    [dragImage _web_dissolveToFraction:WebDragImageAlpha];

    NSPoint mouseDownPoint = [self convertPoint:[event locationInWindow] fromView:nil];
    NSPoint currentPoint = [self convertPoint:[[_window currentEvent] locationInWindow] fromView:nil];

    // Properly orient the drag image and orient it differently if it's smaller than the original
    origin.x = mouseDownPoint.x - (((mouseDownPoint.x - origin.x) / originalSize.width) * newSize.width);
    origin.y = origin.y + originalSize.height;
    origin.y = mouseDownPoint.y - (((mouseDownPoint.y - origin.y) / originalSize.height) * newSize.height);

    NSSize offset = NSMakeSize(currentPoint.x - mouseDownPoint.x, currentPoint.y - mouseDownPoint.y);

    NSArray *filesTypes = [NSArray arrayWithObject:fileType];
    
    NSPasteboard *pboard = [NSPasteboard pasteboardWithName:NSDragPboard];
    NSMutableArray *types = [NSMutableArray arrayWithObjects:NSFilesPromisePboardType, NSTIFFPboardType, nil];
    [types addObjectsFromArray:[NSPasteboard _web_dragTypesForURL]];
    [pboard _web_writeURL:URL andTitle:title withOwner:self types:types];
    [pboard setPropertyList:filesTypes forType:NSFilesPromisePboardType];
    [pboard setData:[image TIFFRepresentation] forType:NSTIFFPboardType];
    
    id source = [[NSFilePromiseDragSource alloc] initWithSource:(id)self];
    [source setTypes:filesTypes onPasteboard:pboard];
    
    [self dragImage:dragImage at:origin offset:offset event:event pasteboard:pboard source:source slideBack:YES];
}

@end
