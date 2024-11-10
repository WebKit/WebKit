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

#if ENABLE(DRAG_SUPPORT) && PLATFORM(MAC)

#import "AppKitTestSPI.h"
#import "DragAndDropSimulator.h"
#import "TestFilePromiseReceiver.h"
#import <wtf/WeakObjCPtr.h>
#import <wtf/cocoa/TypeCastsCocoa.h>

@implementation TestDraggingInfo {
    WeakObjCPtr<id> _source;
    WeakObjCPtr<DragAndDropSimulator> _dragAndDropSimulator;
    RetainPtr<NSPasteboard> _pasteboard;
    RetainPtr<NSImage> _draggedImage;
    RetainPtr<NSMutableArray<NSFilePromiseReceiver *>> _filePromiseReceivers;
}

- (instancetype)initWithDragAndDropSimulator:(DragAndDropSimulator *)simulator
{
    if (self = [super init]) {
        _filePromiseReceivers = adoptNS([NSMutableArray new]);
        _dragAndDropSimulator = simulator;
    }
    return self;
}

@synthesize draggingSourceOperationMask = _draggingSourceOperationMask;
@synthesize draggingLocation = _draggingLocation;
@synthesize draggingFormation = _draggingFormation;
@synthesize numberOfValidItemsForDrop = _numberOfValidItemsForDrop;

- (NSArray<NSFilePromiseReceiver *> *)filePromiseReceivers
{
    return _filePromiseReceivers.get();
}

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

- (void)enumerateDraggingItemsWithOptions:(NSDraggingItemEnumerationOptions)enumerationOptions forView:(NSView *)view classes:(NSArray<Class> *)classes searchOptions:(NSDictionary<NSString *, id> *)searchOptions usingBlock:(void (^)(NSDraggingItem *, NSInteger, BOOL *))block
{
    // FIXME: Much of this can be shared with existing drag and drop testing code in DumpRenderTree,
    // perhaps by putting it in Tools/TestRunnerShared.

    if (enumerationOptions) {
        [NSException raise:@"DragAndDropSimulator" format:@"Dragging item enumeration options are currently unsupported. (got: %tu)", enumerationOptions];
        return;
    }

    if (searchOptions.count) {
        [NSException raise:@"DragAndDropSimulator" format:@"Search options are currently unsupported. (got: %@)", searchOptions];
        return;
    }

    BOOL stop = NO;
    for (Class classObject in classes) {
        if (classObject != NSFilePromiseReceiver.class)
            continue;

        RetainPtr promisedTypeIdentifiers = dynamic_objc_cast<NSArray>([_pasteboard propertyListForType:NSFilesPromisePboardType]);
        RetainPtr externalPromisedFiles = [_dragAndDropSimulator externalPromisedFiles];
        RELEASE_ASSERT([promisedTypeIdentifiers count] == [externalPromisedFiles count]);

        for (id object in promisedTypeIdentifiers.get())
            RELEASE_ASSERT([object isKindOfClass:NSString.class]);

        NSUInteger itemIndex = 0;
        for (NSURL *promisedFileURL in externalPromisedFiles.get()) {
            RetainPtr receiver = adoptNS([[TestFilePromiseReceiver alloc] initWithTypeIdentifier:[promisedTypeIdentifiers objectAtIndex:itemIndex] fileURL:promisedFileURL]);
            [receiver setDraggingSource:_source.get().get()];
            [_filePromiseReceivers addObject:receiver.get()];

            RetainPtr item = adoptNS([[NSDraggingItem alloc] _initWithItem:receiver.get()]);
            block(item.get(), 0, &stop);
            if (stop)
                break;

            itemIndex++;
        }
    }
}

// The following methods are not currently used by WebKit.

@synthesize draggedImageLocation = _draggedImageLocation;
@synthesize draggingSequenceNumber = _draggingSequenceNumber;
@synthesize animatesToDestination = _animatesToDestination;
@synthesize springLoadingHighlight = _springLoadingHighlight;

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

IGNORE_WARNINGS_BEGIN("deprecated-implementations")
- (NSArray<NSString *> *)namesOfPromisedFilesDroppedAtDestination:(NSURL *)dropDestination
IGNORE_WARNINGS_END
{
    return @[ ];
}

- (void)resetSpringLoading
{
}

- (id)localContext
{
    return nil;
}

@end

#endif // ENABLE(DRAG_SUPPORT) && PLATFORM(MAC)
