/*
        WebNSViewExtras.mm
        Copyright (c) 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebController.h>
#import <WebKit/WebNSViewExtras.h>
#import <WebKit/WebView.h>

#import <WebFoundation/WebNSStringExtras.h>
#import <WebFoundation/WebNSURLExtras.h>

#define DragStartHysteresis  		5.0

#ifdef DEBUG_VIEWS
@interface NSObject (Foo)
- (void*)_renderFramePart;
- (id)frameForView: (id)aView;
- (id)_controller;
@end
#endif

@implementation NSView (WebExtras)

- (NSView *) _web_superviewWithName:(NSString *)viewName
{
    NSView *view;
    
    view = self;
    while(view){
        view = [view superview];
        if([[view className] isEqualToString:viewName]){
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
- (BOOL)_web_dragShouldBeginFromMouseDown: (NSEvent *)mouseDownEvent withExpiration:(NSDate *)expiration
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

            if (deltax >= DragStartHysteresis) {
                dragIt = YES;
                break;
            }

            if (deltay >= DragStartHysteresis) {
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
        [NSApp postEvent: mouseUp atStart: YES];
    }
    if (dragEvent != nil) {
        [NSApp postEvent: dragEvent atStart: YES];
    }
    if (firstEvent != mouseUp && firstEvent != dragEvent) {
        [NSApp postEvent: firstEvent atStart: YES];
    }

    return dragIt;
}

- (NSArray *)_web_acceptableDragTypes
{
    return [NSArray arrayWithObjects:NSURLPboardType, NSStringPboardType, NSFilenamesPboardType, nil];
}

- (NSURL *)_web_bestURLForDraggingInfo:(id <NSDraggingInfo>)sender
{
    NSPasteboard *draggingPasteboard = [sender draggingPasteboard];        
    NSURL *bestURL = [NSURL URLFromPasteboard:draggingPasteboard];
    NSString *scheme = [bestURL scheme];
    
    if(!bestURL || ![scheme isEqualToString:@"http"] || ![scheme isEqualToString:@"https"]){

        NSString *URLString = [[draggingPasteboard stringForType:NSStringPboardType] _web_stringByTrimmingWhitespace];
        if(URLString && [URLString _web_looksLikeAbsoluteURL]){
            bestURL = [NSURL _web_URLWithString:URLString];
        }

        if(!bestURL){
            NSArray *files = [draggingPasteboard propertyListForType:NSFilenamesPboardType];
            if(files && [files count] == 1){
                NSString *file = [files objectAtIndex:0];
                if([WebController canShowFile:file]){
                    bestURL = [NSURL fileURLWithPath:file];
                }
            }
        }
    }

    return bestURL;
}

- (NSDragOperation)_web_dragOperationForDraggingInfo:(id <NSDraggingInfo>)sender
{
    if([self _web_bestURLForDraggingInfo:sender] && [sender draggingSource] != self){
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

@end
