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
#import "DragAndDropSimulator.h"

#if ENABLE(DRAG_SUPPORT) && PLATFORM(MAC) && WK_API_ENABLED

#import "PlatformUtilities.h"
#import "TestDraggingInfo.h"
#import "TestWKWebView.h"
#import <cmath>
#import <wtf/WeakObjCPtr.h>

@class DragAndDropTestWKWebView;

@interface DragAndDropSimulator ()
- (void)beginDraggingSessionInWebView:(DragAndDropTestWKWebView *)webView withItems:(NSArray<NSDraggingItem *> *)items source:(id<NSDraggingSource>)source;
- (void)performDragInWebView:(DragAndDropTestWKWebView *)webView atLocation:(NSPoint)viewLocation withImage:(NSImage *)image pasteboard:(NSPasteboard *)pasteboard source:(id)source;
@property (nonatomic, readonly) NSDraggingSession *draggingSession;
@end

@interface DragAndDropTestWKWebView : TestWKWebView
@end

@implementation DragAndDropTestWKWebView {
    WeakObjCPtr<DragAndDropSimulator> _dragAndDropSimulator;
}

- (instancetype)initWithFrame:(CGRect)frame configuration:(WKWebViewConfiguration *)configuration simulator:(DragAndDropSimulator *)simulator
{
    if (self = [super initWithFrame:frame configuration:configuration])
        _dragAndDropSimulator = simulator;
    return self;
}

IGNORE_WARNINGS_BEGIN("deprecated-implementations")
- (void)dragImage:(NSImage *)image at:(NSPoint)viewLocation offset:(NSSize)initialOffset event:(NSEvent *)event pasteboard:(NSPasteboard *)pboard source:(id)sourceObj slideBack:(BOOL)slideFlag
IGNORE_WARNINGS_END
{
    [_dragAndDropSimulator performDragInWebView:self atLocation:viewLocation withImage:image pasteboard:pboard source:sourceObj];
}

- (NSDraggingSession *)beginDraggingSessionWithItems:(NSArray<NSDraggingItem *> *)items event:(NSEvent *)event source:(id<NSDraggingSource>)source
{
    [_dragAndDropSimulator beginDraggingSessionInWebView:self withItems:items source:source];
    return [_dragAndDropSimulator draggingSession];
}

- (void)waitForPendingMouseEvents
{
    __block bool doneProcessMouseEvents = false;
    [self _doAfterProcessingAllPendingMouseEvents:^{
        doneProcessMouseEvents = true;
    }];
    TestWebKitAPI::Util::run(&doneProcessMouseEvents);
}

@end

// This exceeds the default drag hysteresis of all potential drag types.
const double initialMouseDragDistance = 45;
const double dragUpdateProgressIncrement = 0.05;

static NSImage *defaultExternalDragImage()
{
    return [[[NSImage alloc] initWithContentsOfURL:[[NSBundle mainBundle] URLForResource:@"icon" withExtension:@"png" subdirectory:@"TestWebKitAPI.resources"]] autorelease];
}

@implementation DragAndDropSimulator {
    RetainPtr<DragAndDropTestWKWebView> _webView;
    RetainPtr<TestDraggingInfo> _draggingInfo;
    RetainPtr<NSPasteboard> _externalDragPasteboard;
    RetainPtr<NSImage> _externalDragImage;
    RetainPtr<NSArray<NSURL *>> _externalPromisedFiles;
    RetainPtr<NSMutableArray<_WKAttachment *>> _insertedAttachments;
    RetainPtr<NSMutableArray<_WKAttachment *>> _removedAttachments;
    RetainPtr<NSMutableArray<NSURL *>> _filePromiseDestinationURLs;
    RetainPtr<NSDraggingSession> _draggingSession;
    RetainPtr<NSMutableArray<NSFilePromiseProvider *>> _filePromiseProviders;
    BlockPtr<void()> _willEndDraggingHandler;
    NSPoint _startLocationInWindow;
    NSPoint _endLocationInWindow;
    double _progress;
    bool _doneWaitingForDraggingSession;
    bool _doneWaitingForDrop;
}

@synthesize currentDragOperation=_currentDragOperation;
@synthesize initialDragImageLocationInView=_initialDragImageLocationInView;

- (instancetype)initWithWebViewFrame:(CGRect)frame
{
    return [self initWithWebViewFrame:frame configuration:nil];
}

- (instancetype)initWithWebViewFrame:(CGRect)frame configuration:(WKWebViewConfiguration *)configuration
{
    if (self = [super init]) {
        _webView = adoptNS([[DragAndDropTestWKWebView alloc] initWithFrame:frame configuration:configuration ?: [[[WKWebViewConfiguration alloc] init] autorelease] simulator:self]);
        _filePromiseDestinationURLs = adoptNS([NSMutableArray new]);
        [_webView setUIDelegate:self];
        self.dragDestinationAction = WKDragDestinationActionAny & ~WKDragDestinationActionLoad;
    }
    return self;
}

- (void)dealloc
{
    for (NSURL *url in _filePromiseDestinationURLs.get())
        [[NSFileManager defaultManager] removeItemAtURL:url error:nil];

    [super dealloc];
}

- (NSPoint)flipAboutXAxisInHostWindow:(NSPoint)point
{
    return { point.x, NSHeight([[_webView hostWindow] frame]) - point.y };
}

- (NSPoint)locationInViewForCurrentProgress
{
    return {
        _startLocationInWindow.x + (_endLocationInWindow.x - _startLocationInWindow.x) * _progress,
        _startLocationInWindow.y + (_endLocationInWindow.y - _startLocationInWindow.y) * _progress
    };
}

- (double)initialProgressForMouseDrag
{
    double totalDistance = std::sqrt(std::pow(_startLocationInWindow.x - _endLocationInWindow.x, 2) + std::pow(_startLocationInWindow.y - _endLocationInWindow.y, 2));
    return !totalDistance ? 1 : std::min<double>(1, initialMouseDragDistance / totalDistance);
}

- (void)runFrom:(CGPoint)flippedStartLocation to:(CGPoint)flippedEndLocation
{
    _insertedAttachments = adoptNS([NSMutableArray new]);
    _removedAttachments = adoptNS([NSMutableArray new]);
    _doneWaitingForDraggingSession = true;
    _doneWaitingForDrop = true;
    _startLocationInWindow = [self flipAboutXAxisInHostWindow:flippedStartLocation];
    _endLocationInWindow = [self flipAboutXAxisInHostWindow:flippedEndLocation];
    _currentDragOperation = NSDragOperationNone;
    _draggingInfo = nil;
    _draggingSession = nil;
    _progress = 0;
    _filePromiseProviders = adoptNS([NSMutableArray new]);

    if (NSPasteboard *pasteboard = self.externalDragPasteboard) {
        NSPoint startLocationInView = [_webView convertPoint:_startLocationInWindow fromView:nil];
        NSImage *dragImage = self.externalDragImage ?: defaultExternalDragImage();
        [self performDragInWebView:_webView.get() atLocation:startLocationInView withImage:dragImage pasteboard:pasteboard source:nil];
        TestWebKitAPI::Util::run(&_doneWaitingForDrop);
        return;
    }

    _progress = [self initialProgressForMouseDrag];
    if (_progress == 1) {
        [NSException raise:@"DragAndDropSimulator" format:@"Drag start (%@) and drag end (%@) locations are too close!", NSStringFromPoint(flippedStartLocation), NSStringFromPoint(flippedEndLocation)];
        return;
    }

    [_webView mouseEnterAtPoint:_startLocationInWindow];
    [_webView mouseMoveToPoint:_startLocationInWindow withFlags:0];
    [_webView mouseDownAtPoint:_startLocationInWindow simulatePressure:NO];
    [_webView mouseDragToPoint:[self locationInViewForCurrentProgress]];
    [_webView waitForPendingMouseEvents];

    TestWebKitAPI::Util::run(&_doneWaitingForDraggingSession);

    [_webView mouseUpAtPoint:_endLocationInWindow];
    [_webView waitForPendingMouseEvents];

    TestWebKitAPI::Util::run(&_doneWaitingForDrop);
}

- (void)beginDraggingSessionInWebView:(DragAndDropTestWKWebView *)webView withItems:(NSArray<NSDraggingItem *> *)items source:(id<NSDraggingSource>)source
{
    NSMutableArray *pasteboardObjects = [NSMutableArray arrayWithCapacity:items.count];
    NSMutableArray<NSString *> *promisedFileTypes = [NSMutableArray array];
    for (NSDraggingItem *item in items) {
        id pasteboardObject = item.item;
        [pasteboardObjects addObject:pasteboardObject];
        if ([pasteboardObject isKindOfClass:[NSFilePromiseProvider class]]) {
            [_filePromiseProviders addObject:pasteboardObject];
            [promisedFileTypes addObject:[(NSFilePromiseProvider *)pasteboardObject fileType]];
        }
    }

    NSPasteboard *pasteboard = [NSPasteboard pasteboardWithName:NSDragPboard];
    [pasteboard clearContents];
    [pasteboard writeObjects:pasteboardObjects];
    if (promisedFileTypes.count) {
        // Match AppKit behavior by writing legacy file promise types to the pasteboard as well.
        [pasteboard setPropertyList:promisedFileTypes forType:NSFilesPromisePboardType];
        [pasteboard addTypes:@[@"NSPromiseContentsPboardType", (NSString *)kPasteboardTypeFileURLPromise] owner:nil];
    }

    _draggingSession = adoptNS([[NSDraggingSession alloc] init]);
    _doneWaitingForDraggingSession = false;
    _initialDragImageLocationInView = items[0].draggingFrame.origin;
    id dragImageContents = items[0].imageComponents.firstObject.contents;
    [self initializeDraggingInfo:pasteboard dragImage:[dragImageContents isKindOfClass:[NSImage class]] ? dragImageContents : nil source:source];

    _currentDragOperation = [_webView draggingEntered:_draggingInfo.get()];
    [_webView waitForNextPresentationUpdate];
    [self performSelector:@selector(continueDragSession) withObject:nil afterDelay:0];
}

- (void)continueDragSession
{
    _progress = std::min<double>(1, _progress + dragUpdateProgressIncrement);

    if (_progress < 1) {
        [_draggingInfo setDraggingLocation:[self locationInViewForCurrentProgress]];
        _currentDragOperation = [_webView draggingUpdated:_draggingInfo.get()];
        [_webView waitForNextPresentationUpdate];
        [self performSelector:@selector(continueDragSession) withObject:nil afterDelay:0];
        return;
    }

    [_draggingInfo setDraggingLocation:_endLocationInWindow];

    if (_willEndDraggingHandler)
        _willEndDraggingHandler();

    if (_currentDragOperation != NSDragOperationNone && [_webView prepareForDragOperation:_draggingInfo.get()]) {
        _doneWaitingForDrop = false;
        [_webView performDragOperation:_draggingInfo.get()];
    } else if (_currentDragOperation == NSDragOperationNone)
        [_webView draggingExited:_draggingInfo.get()];
    [_webView waitForNextPresentationUpdate];
    [(id <NSDraggingSource>)_webView.get() draggingSession:_draggingSession.get() endedAtPoint:_endLocationInWindow operation:_currentDragOperation];

    _doneWaitingForDraggingSession = true;
}

- (void)performDragInWebView:(DragAndDropTestWKWebView *)webView atLocation:(NSPoint)viewLocation withImage:(NSImage *)image pasteboard:(NSPasteboard *)pasteboard source:(id)source
{
    _initialDragImageLocationInView = viewLocation;
    [self initializeDraggingInfo:pasteboard dragImage:image source:source];

    _currentDragOperation = [_webView draggingEntered:_draggingInfo.get()];
    [_webView waitForNextPresentationUpdate];

    while (_progress != 1) {
        _progress = std::min<double>(1, _progress + dragUpdateProgressIncrement);
        [_draggingInfo setDraggingLocation:[self locationInViewForCurrentProgress]];
        _currentDragOperation = [_webView draggingUpdated:_draggingInfo.get()];
        [_webView waitForNextPresentationUpdate];
    }

    [_draggingInfo setDraggingLocation:_endLocationInWindow];

    if (_willEndDraggingHandler)
        _willEndDraggingHandler();

    if (_currentDragOperation != NSDragOperationNone && [_webView prepareForDragOperation:_draggingInfo.get()]) {
        _doneWaitingForDrop = false;
        [_webView performDragOperation:_draggingInfo.get()];
    } else if (_currentDragOperation == NSDragOperationNone)
        [_webView draggingExited:_draggingInfo.get()];
    [_webView waitForNextPresentationUpdate];

    if (!self.externalDragPasteboard) {
        [_webView draggedImage:[_draggingInfo draggedImage] endedAt:_endLocationInWindow operation:_currentDragOperation];
        [_webView waitForNextPresentationUpdate];
    }
}

- (void)initializeDraggingInfo:(NSPasteboard *)pasteboard dragImage:(NSImage *)image source:(id)source
{
    _draggingInfo = adoptNS([[TestDraggingInfo alloc] initWithDragAndDropSimulator:self]);
    [_draggingInfo setDraggedImage:image];
    [_draggingInfo setDraggingPasteboard:pasteboard];
    [_draggingInfo setDraggingSource:source];
    [_draggingInfo setDraggingLocation:[self locationInViewForCurrentProgress]];
    [_draggingInfo setDraggingSourceOperationMask:NSDragOperationEvery];
    [_draggingInfo setNumberOfValidItemsForDrop:pasteboard.pasteboardItems.count];
}

- (NSArray<_WKAttachment *> *)insertedAttachments
{
    return _insertedAttachments.get();
}

- (NSArray<_WKAttachment *> *)removedAttachments
{
    return _removedAttachments.get();
}

- (TestWKWebView *)webView
{
    return _webView.get();
}

- (void)setExternalDragPasteboard:(NSPasteboard *)externalDragPasteboard
{
    _externalDragPasteboard = externalDragPasteboard;
}

- (NSPasteboard *)externalDragPasteboard
{
    return _externalDragPasteboard.get();
}

- (void)setExternalDragImage:(NSImage *)externalDragImage
{
    _externalDragImage = externalDragImage;
}

- (NSImage *)externalDragImage
{
    return _externalDragImage.get();
}

- (NSDraggingSession *)draggingSession
{
    return _draggingSession.get();
}

- (id <NSDraggingInfo>)draggingInfo
{
    return _draggingInfo.get();
}

- (dispatch_block_t)willEndDraggingHandler
{
    return _willEndDraggingHandler.get();
}

- (void)setWillEndDraggingHandler:(dispatch_block_t)willEndDraggingHandler
{
    _willEndDraggingHandler = makeBlockPtr(willEndDraggingHandler);
}

- (NSArray<NSURL *> *)externalPromisedFiles
{
    return _externalPromisedFiles.get();
}

- (void)clearExternalDragInformation
{
    _externalPromisedFiles = nil;
    _externalDragImage = nil;
    _externalDragPasteboard = nil;
}

static BOOL getFilePathsAndTypeIdentifiers(NSArray<NSURL *> *fileURLs, NSArray<NSString *> **outFilePaths, NSArray<NSString *> **outTypeIdentifiers)
{
    NSMutableArray *filePaths = [NSMutableArray arrayWithCapacity:fileURLs.count];
    NSMutableArray *typeIdentifiers = [NSMutableArray arrayWithCapacity:fileURLs.count];
    for (NSURL *url in fileURLs) {
        NSString *typeIdentifier = nil;
        NSError *error = nil;
        BOOL foundUTI = [url getResourceValue:&typeIdentifier forKey:NSURLTypeIdentifierKey error:&error];
        if (!foundUTI || error) {
            [NSException raise:@"DragAndDropSimulator" format:@"Failed to get UTI for promised file: %@ with error: %@", url, error];
            continue;
        }
        [typeIdentifiers addObject:typeIdentifier];
        [filePaths addObject:url.path];
    }

    if (fileURLs.count != filePaths.count)
        return NO;

    if (outTypeIdentifiers)
        *outTypeIdentifiers = typeIdentifiers;

    if (outFilePaths)
        *outFilePaths = filePaths;

    return YES;
}

- (void)writePromisedFiles:(NSArray<NSURL *> *)fileURLs
{
    NSArray *paths = nil;
    NSArray *types = nil;
    if (!getFilePathsAndTypeIdentifiers(fileURLs, &paths, &types))
        return;

    NSMutableArray *names = [NSMutableArray arrayWithCapacity:paths.count];
    for (NSString *path in paths)
        [names addObject:path.lastPathComponent];

    _externalPromisedFiles = fileURLs;
    _externalDragPasteboard = [NSPasteboard pasteboardWithUniqueName];
    [_externalDragPasteboard declareTypes:@[NSFilesPromisePboardType, NSFilenamesPboardType] owner:nil];
    [_externalDragPasteboard setPropertyList:types forType:NSFilesPromisePboardType];
    [_externalDragPasteboard setPropertyList:names forType:NSFilenamesPboardType];
}

- (void)writeFiles:(NSArray<NSURL *> *)fileURLs
{
    NSArray *paths = nil;
    if (!getFilePathsAndTypeIdentifiers(fileURLs, &paths, nil))
        return;

    _externalDragPasteboard = [NSPasteboard pasteboardWithName:NSDragPboard];
    [_externalDragPasteboard declareTypes:@[NSFilenamesPboardType] owner:nil];
    [_externalDragPasteboard setPropertyList:paths forType:NSFilenamesPboardType];
}

- (NSArray<NSURL *> *)receivePromisedFiles
{
    auto destinationURLs = adoptNS([NSMutableArray new]);
    for (NSFilePromiseProvider *provider in _filePromiseProviders.get()) {
        if (!provider.delegate)
            continue;

        int suffix = 1;
        NSString *baseFileName = [provider.delegate filePromiseProvider:provider fileNameForType:provider.fileType];
        NSString *uniqueFileName = baseFileName;
        while ([[NSFileManager defaultManager] fileExistsAtPath:[NSTemporaryDirectory() stringByAppendingPathComponent:uniqueFileName]])
            uniqueFileName = [NSString stringWithFormat:@"%@ %d", baseFileName, ++suffix];

        NSURL *destinationURL = [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:uniqueFileName]];
        __block bool done = false;
        [provider.delegate filePromiseProvider:provider writePromiseToURL:destinationURL completionHandler:^(NSError *) {
            done = true;
        }];
        TestWebKitAPI::Util::run(&done);
        [destinationURLs addObject:destinationURL];
        [_filePromiseDestinationURLs addObject:destinationURL];
    }
    return destinationURLs.autorelease();
}

- (void)endDataTransfer
{
}

#pragma mark - WKUIDelegatePrivate

- (void)_webView:(WKWebView *)webView didInsertAttachment:(_WKAttachment *)attachment withSource:(NSString *)source
{
    [_insertedAttachments addObject:attachment];
}

- (void)_webView:(WKWebView *)webView didRemoveAttachment:(_WKAttachment *)attachment
{
    [_removedAttachments addObject:attachment];
}

- (void)_webView:(WKWebView *)webView didPerformDragOperation:(BOOL)handled
{
    _doneWaitingForDrop = true;
}

- (WKDragDestinationAction)_webView:(WKWebView *)webView dragDestinationActionMaskForDraggingInfo:(id)draggingInfo
{
    return self.dragDestinationAction;
}

@end

#endif // ENABLE(DRAG_SUPPORT) && PLATFORM(MAC) && WK_API_ENABLED
