/*
 * Copyright (C) 2005, 2006, 2013 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "WebKitTestRunnerDraggingInfo.h"

#import "EventSenderProxy.h"
#import "PlatformWebView.h"
#import "TestController.h"
#import "TestRunnerWKWebView.h"

using namespace WTR;

@implementation WebKitTestRunnerDraggingInfo

- (id)initWithImage:(NSImage *)image offset:(NSSize)offset pasteboard:(NSPasteboard *)pasteboard source:(id)source
{
    self = [super init];
    if (!self)
        return nil;

    _draggedImage = [image retain];
    _draggingPasteboard = [pasteboard retain];
    _draggingSource = [source retain];
    _offset = offset;
    
    return self;
}

- (void)dealloc
{
    [_draggedImage release];
    [_draggingPasteboard release];
    [_draggingSource release];
    [super dealloc];
}

- (NSWindow *)draggingDestinationWindow 
{
    return [TestController::singleton().mainWebView()->platformView() window];
}

- (NSDragOperation)draggingSourceOperationMask 
{
    // WKView currently implements neither draggingSourceOperationMaskForLocal: nor draggingSession:sourceOperationMaskForDraggingContext:.
    return NSDragOperationAll;
}

- (NSPoint)draggingLocation
{
    WKPoint location = TestController::singleton().eventSenderProxy()->position();
    return NSMakePoint(location.x, location.y);
}

- (NSPoint)draggedImageLocation 
{
    WKPoint location = TestController::singleton().eventSenderProxy()->position();
    return NSMakePoint(location.x + _offset.width, location.y + _offset.height);
}

- (NSImage *)draggedImage
{
    return _draggedImage;
}

- (NSPasteboard *)draggingPasteboard
{
    return _draggingPasteboard;
}

- (id)draggingSource
{
    return _draggingSource;
}

- (int)draggingSequenceNumber
{
    NSLog(@"WebKitTestRunner doesn't support draggingSequenceNumber");
    return 0;
}

- (void)slideDraggedImageTo:(NSPoint)screenPoint
{
    NSLog(@"WebKitTestRunner doesn't support slideDraggedImageTo:");
}

- (NSArray *)namesOfPromisedFilesDroppedAtDestination:(NSURL *)dropDestination
{
    NSLog(@"WebKitTestRunner doesn't support namesOfPromisedFilesDroppedAtDestination:");
    return nil;
}

- (NSDraggingFormation)draggingFormation
{
    return NSDraggingFormationDefault;
}

- (void)setDraggingFormation:(NSDraggingFormation)formation
{
    // Ignored.
}

- (BOOL)animatesToDestination
{
    return NO;
}

- (void)setAnimatesToDestination:(BOOL)flag
{
    // Ignored.
}

- (NSInteger)numberOfValidItemsForDrop
{
    return 1;
}

- (void)setNumberOfValidItemsForDrop:(NSInteger)number
{
    // Ignored.
}

- (void)enumerateDraggingItemsWithOptions:(NSEnumerationOptions)enumOpts forView:(NSView *)view classes:(NSArray *)classArray searchOptions:(NSDictionary *)searchOptions usingBlock:(void (^)(NSDraggingItem *draggingItem, NSInteger idx, BOOL *stop))block
{
    // Ignored.
}

-(NSSpringLoadingHighlight)springLoadingHighlight
{
    return NSSpringLoadingHighlightNone;
}

- (void)resetSpringLoading
{
}

@end
