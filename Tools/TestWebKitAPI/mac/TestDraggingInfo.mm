/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#import "TestDraggingInfo.h"

#if ENABLE(DRAG_SUPPORT) && PLATFORM(MAC) && WK_API_ENABLED

#import <wtf/WeakObjCPtr.h>

@implementation TestDraggingInfo {
    WeakObjCPtr<id> _source;
    RetainPtr<NSPasteboard> _pasteboard;
    RetainPtr<NSImage> _draggedImage;
}

@synthesize draggingSourceOperationMask=_draggingSourceOperationMask;
@synthesize draggingLocation=_draggingLocation;
@synthesize draggingFormation=_draggingFormation;
@synthesize numberOfValidItemsForDrop=_numberOfValidItemsForDrop;

- (NSPasteboard *)draggingPasteboard
{
    return _pasteboard.get();
}

- (void)setDraggingPasteboard:(NSPasteboard *)draggingPasteboard
{
    _pasteboard = draggingPasteboard;
}

- (id)draggingSource
{
    return _source.get().get();
}

- (void)setDraggingSource:(id)draggingSource
{
    _source = draggingSource;
}

- (void)enumerateDraggingItemsWithOptions:(NSDraggingItemEnumerationOptions)enumOpts forView:(NSView *)view classes:(NSArray<Class> *)classArray searchOptions:(NSDictionary<NSString *, id> *)searchOptions usingBlock:(void (^)(NSDraggingItem *, NSInteger, BOOL *))block
{
    // FIXME: Implement this to test file promise drop handling.
}

// The following methods are not currently used by WebKit.

@synthesize draggedImageLocation=_draggedImageLocation;
@synthesize draggingSequenceNumber=_draggingSequenceNumber;
@synthesize animatesToDestination=_animatesToDestination;
@synthesize springLoadingHighlight=_springLoadingHighlight;

- (NSWindow *)draggingDestinationWindow
{
    return nil;
}

- (NSImage *)draggedImage
{
    return _draggedImage.get();
}

- (void)setDraggedImage:(NSImage *)draggedImage
{
    _draggedImage = draggedImage;
}

- (void)slideDraggedImageTo:(NSPoint)screenPoint
{
}

- (NSArray<NSString *> *)namesOfPromisedFilesDroppedAtDestination:(NSURL *)dropDestination
{
    return @[ ];
}

- (void)resetSpringLoading
{
}

@end

#endif // ENABLE(DRAG_SUPPORT) && PLATFORM(MAC) && WK_API_ENABLED
