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
- (void)performDragInWebView:(DragAndDropTestWKWebView *)webView atLocation:(NSPoint)viewLocation withImage:(NSImage *)image pasteboard:(NSPasteboard *)pasteboard source:(id)source;
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

- (void)dragImage:(NSImage *)image at:(NSPoint)viewLocation offset:(NSSize)initialOffset event:(NSEvent *)event pasteboard:(NSPasteboard *)pboard source:(id)sourceObj slideBack:(BOOL)slideFlag
{
    [_dragAndDropSimulator performDragInWebView:self atLocation:viewLocation withImage:image pasteboard:pboard source:sourceObj];
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
    BlockPtr<void()> _willEndDraggingHandler;
    NSPoint _startLocationInWindow;
    NSPoint _endLocationInWindow;
    double _progress;
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
        [_webView setUIDelegate:self];
    }
    return self;
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
    _startLocationInWindow = [self flipAboutXAxisInHostWindow:flippedStartLocation];
    _endLocationInWindow = [self flipAboutXAxisInHostWindow:flippedEndLocation];
    _currentDragOperation = NSDragOperationNone;
    _draggingInfo = nil;
    _progress = 0;

    if (NSPasteboard *pasteboard = self.externalDragPasteboard) {
        NSPoint startLocationInView = [_webView convertPoint:_startLocationInWindow fromView:nil];
        NSImage *dragImage = self.externalDragImage ?: defaultExternalDragImage();
        [self performDragInWebView:_webView.get() atLocation:startLocationInView withImage:dragImage pasteboard:pasteboard source:nil];
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

    [_webView mouseUpAtPoint:_endLocationInWindow];
    [_webView waitForPendingMouseEvents];
}

- (void)performDragInWebView:(DragAndDropTestWKWebView *)webView atLocation:(NSPoint)viewLocation withImage:(NSImage *)image pasteboard:(NSPasteboard *)pasteboard source:(id)source
{
    _initialDragImageLocationInView = viewLocation;
    _draggingInfo = adoptNS([[TestDraggingInfo alloc] initWithDragAndDropSimulator:self]);
    [_draggingInfo setDraggedImage:image];
    [_draggingInfo setDraggingPasteboard:pasteboard];
    [_draggingInfo setDraggingSource:source];
    [_draggingInfo setDraggingLocation:[self locationInViewForCurrentProgress]];
    [_draggingInfo setDraggingSourceOperationMask:NSDragOperationEvery];
    [_draggingInfo setNumberOfValidItemsForDrop:pasteboard.pasteboardItems.count];

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

    if (_currentDragOperation != NSDragOperationNone && [_webView prepareForDragOperation:_draggingInfo.get()])
        [_webView performDragOperation:_draggingInfo.get()];
    else if (_currentDragOperation == NSDragOperationNone)
        [_webView draggingExited:_draggingInfo.get()];
    [_webView waitForNextPresentationUpdate];

    if (!self.externalDragPasteboard) {
        [_webView draggedImage:[_draggingInfo draggedImage] endedAt:_endLocationInWindow operation:_currentDragOperation];
        [_webView waitForNextPresentationUpdate];
    }
}

- (NSArray<_WKAttachment *> *)insertedAttachments
{
    return @[ ];
}

- (NSArray<_WKAttachment *> *)removedAttachments
{
    return @[ ];
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

@end

#endif // ENABLE(DRAG_SUPPORT) && PLATFORM(MAC) && WK_API_ENABLED
