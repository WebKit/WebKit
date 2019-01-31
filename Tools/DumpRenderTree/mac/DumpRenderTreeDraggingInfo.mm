/*
 * Copyright (C) 2005-2018 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "DumpRenderTreeDraggingInfo.h"

#if !PLATFORM(IOS_FAMILY)

#import "DumpRenderTree.h"
#import "DumpRenderTreeFileDraggingSource.h"
#import "DumpRenderTreePasteboard.h"
#import "EventSendingController.h"
#import <WebKit/WebKit.h>
#import <wtf/RetainPtr.h>

@interface NSDraggingItem ()
- (void)setItem:(id)item;
@end

@interface DumpRenderTreeFilePromiseReceiver : NSFilePromiseReceiver {
    RetainPtr<NSArray<NSString *>> _promisedUTIs;
    RetainPtr<NSMutableArray<NSURL *>> _destinationURLs;
    DumpRenderTreeFileDraggingSource *_draggingSource;
}

- (instancetype)initWithPromisedUTIs:(NSArray<NSString *> *)promisedUTIs;

@property (nonatomic, retain) DumpRenderTreeFileDraggingSource *draggingSource;

@end

@implementation DumpRenderTreeFilePromiseReceiver

@synthesize draggingSource=_draggingSource;

- (instancetype)initWithPromisedUTIs:(NSArray<NSString *> *)promisedUTIs
{
    if (!(self = [super init]))
        return nil;

    _promisedUTIs = adoptNS([promisedUTIs copy]);
    _destinationURLs = adoptNS([NSMutableArray new]);
    return self;
}

- (NSArray<NSString *> *)fileTypes
{
    return _promisedUTIs.get();
}

- (NSArray<NSString *> *)fileNames
{
    NSMutableArray *fileNames = [NSMutableArray arrayWithCapacity:[_destinationURLs count]];
    for (NSURL *url in _destinationURLs.get())
        [fileNames addObject:url.lastPathComponent];
    return fileNames;
}

- (void)dealloc
{
    // WebKit does not delete promised files it receives into NSTemporaryDirectory() (it should!),
    // so we need to. Failing to do so could result in unpredictable file names in a subsequent test
    // that promises a file with the same name as one of these destination URLs.

    for (NSURL *destinationURL in _destinationURLs.get()) {
        assert([destinationURL.path hasPrefix:NSTemporaryDirectory()]);
        [NSFileManager.defaultManager removeItemAtURL:destinationURL error:nil];
    }

    [_draggingSource release];
    [super dealloc];
}

static std::pair<NSURL *, NSError *> copyFile(NSURL *sourceURL, NSURL *destinationDirectory)
{
    // Emulate how CFPasteboard finds unique destination file names by inserting " 2", " 3", and so
    // on between the file name's base and extension until a new file is successfully created in
    // the destination directory.

    NSUInteger number = 2;
    NSString *fileName = sourceURL.lastPathComponent;
    NSURL *destinationURL = [NSURL fileURLWithPath:fileName relativeToURL:destinationDirectory];
    NSError *error;
    while (![NSFileManager.defaultManager copyItemAtURL:sourceURL toURL:destinationURL error:&error]) {
        if (error.domain != NSCocoaErrorDomain || error.code != NSFileWriteFileExistsError)
            return { nil, error };

        NSString *newFileName = [NSString stringWithFormat:@"%@ %lu.%@", fileName.stringByDeletingPathExtension, (unsigned long)number++, fileName.pathExtension];
        destinationURL = [NSURL fileURLWithPath:newFileName relativeToURL:destinationDirectory];
    }

    return { destinationURL, nil };
}

- (void)receivePromisedFilesAtDestination:(NSURL *)destinationDirectory options:(NSDictionary *)options operationQueue:(NSOperationQueue *)operationQueue reader:(void (^)(NSURL *fileURL, NSError * __nullable errorOrNil))reader
{
    // Layout tests need files to be received in a predictable order, so execute operations in serial.
    operationQueue.maxConcurrentOperationCount = 1;

    NSArray<NSURL *> *sourceURLs = _draggingSource.promisedFileURLs;
    for (NSURL *sourceURL in sourceURLs) {
        [operationQueue addOperationWithBlock:^{
            NSError *error;
            NSURL *destinationURL;
            std::tie(destinationURL, error) = copyFile(sourceURL, destinationDirectory);
            if (destinationURL) {
                dispatch_async(dispatch_get_main_queue(), ^{
                    [_destinationURLs addObject:destinationURL];
                });
            }

            reader(destinationURL, error);
        }];
    }
}

@end

@implementation DumpRenderTreeDraggingInfo

- (id)initWithImage:(NSImage *)anImage offset:(NSSize)o pasteboard:(NSPasteboard *)pboard source:(id)source
{
    draggedImage = [anImage retain];
    draggingPasteboard = [pboard retain];
    draggingSource = [source retain];
    offset = o;
    
    return [super init];
}

- (void)dealloc
{
    [draggedImage release];
    [draggingPasteboard release];
    [draggingSource release];
    [super dealloc];
}

- (NSWindow *)draggingDestinationWindow 
{
    return [[mainFrame webView] window];
}

- (NSDragOperation)draggingSourceOperationMask 
{
    return [draggingSource draggingSourceOperationMaskForLocal:YES];
}

- (NSPoint)draggingLocation
{ 
    return lastMousePosition; 
}

- (NSPoint)draggedImageLocation 
{
    return NSMakePoint(lastMousePosition.x + offset.width, lastMousePosition.y + offset.height);
}

- (NSImage *)draggedImage
{
    return draggedImage;
}

- (NSPasteboard *)draggingPasteboard
{
    return draggingPasteboard;
}

- (id)draggingSource
{
    return draggingSource;
}

- (int)draggingSequenceNumber
{
    NSLog(@"DumpRenderTree doesn't support draggingSequenceNumber");
    return 0;
}

- (void)slideDraggedImageTo:(NSPoint)screenPoint
{
    NSLog(@"DumpRenderTree doesn't support slideDraggedImageTo:");
}

- (NSArray *)namesOfPromisedFilesDroppedAtDestination:(NSURL *)dropDestination
{
    NSLog(@"DumpRenderTree doesn't support namesOfPromisedFilesDroppedAtDestination:");
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

static NSMutableArray<NSFilePromiseReceiver *> *allFilePromiseReceivers()
{
    static NSMutableArray<NSFilePromiseReceiver *> *allReceivers = [[NSMutableArray alloc] init];
    return allReceivers;
}

+ (void)clearAllFilePromiseReceivers
{
    [allFilePromiseReceivers() removeAllObjects];
}

- (void)enumerateDraggingItemsWithOptions:(NSEnumerationOptions)enumOptions forView:(NSView *)view classes:(NSArray *)classArray searchOptions:(NSDictionary *)searchOptions usingBlock:(void (^)(NSDraggingItem *draggingItem, NSInteger idx, BOOL *stop))block
{
    assert(!enumOptions);
    assert(!searchOptions.count);

    BOOL stop = NO;
    for (Class classObject in classArray) {
        if (classObject != NSFilePromiseReceiver.class)
            continue;

        id promisedUTIs = [draggingPasteboard propertyListForType:NSFilesPromisePboardType];
        if (![promisedUTIs isKindOfClass:NSArray.class])
            return;

        for (id object in promisedUTIs) {
            if (![object isKindOfClass:NSString.class])
                return;
        }

        auto receiver = adoptNS([[DumpRenderTreeFilePromiseReceiver alloc] initWithPromisedUTIs:promisedUTIs]);
        [receiver setDraggingSource:draggingSource];
        [allFilePromiseReceivers() addObject:receiver.get()];

        auto item = adoptNS([[NSDraggingItem alloc] initWithPasteboardWriter:(id <NSPasteboardWriting>)receiver.get()]); // FIXME: <https://webkit.org/b/194060> Pass an object of the right type.
        [item setItem:receiver.get()];

        block(item.get(), 0, &stop);
        if (stop)
            return;
    }
}

-(NSSpringLoadingHighlight)springLoadingHighlight
{
    return NSSpringLoadingHighlightNone;
}

- (void)resetSpringLoading
{
}

@end

#endif // !PLATFORM(IOS_FAMILY)
