/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include "config.h"

#if PLATFORM(MAC)

#import "PlatformUtilities.h"
#import "TestWKWebView.h"
#import <AppKit/NSDragging.h>
#import <WebKit/WebView.h>
#import <wtf/BlockPtr.h>
#import <wtf/RetainPtr.h>

@interface FrameLoadCompletionListener : NSObject<WebFrameLoadDelegate> {
    BlockPtr<void()> _completionBlock;
}
+ (instancetype)listenerWithCompletionBlock:(dispatch_block_t)completionBlock;
@end

@implementation FrameLoadCompletionListener

+ (instancetype)listenerWithCompletionBlock:(dispatch_block_t)completionBlock
{
    return [[[FrameLoadCompletionListener alloc] initWithCompletionBlock:completionBlock] autorelease];
}

- (instancetype)initWithCompletionBlock:(dispatch_block_t)completionBlock
{
    if (self = [super init])
        _completionBlock = completionBlock;

    return self;
}

- (void)webView:(WebView *)sender didFinishLoadForFrame:(WebFrame *)frame
{
    if (_completionBlock)
        _completionBlock();
}
@end

@interface DragSource : NSObject
- (NSDragOperation)draggingSourceOperationMaskForLocal:(BOOL)flag;
@end

@implementation DragSource

- (NSDragOperation)draggingSourceOperationMaskForLocal:(BOOL)flag
{
    return NSDragOperationCopy;
}

@end

@interface DragInfo : NSObject<NSDraggingInfo> {
    NSPoint _lastMousePosition;
    RetainPtr<NSImage> _image;
    RetainPtr<NSPasteboard> _pasteboard;
    RetainPtr<DragSource> _source;
    RetainPtr<NSWindow> _window;
    NSSize _offset;
    NSInteger _numberOfValidItemsForDrop;
}
@property (nonatomic) NSPoint lastMousePosition;
@end

@implementation DragInfo

- (id)initWithImage:(NSImage *)image offset:(NSSize)offset pasteboard:(NSPasteboard *)pasteboard source:(DragSource *)source destinationWindow:(NSWindow *)destinationWindow
{
    if (self = [super init]) {
        _image = image;
        _pasteboard = pasteboard;
        _source = source;
        _window = destinationWindow;
        _offset = offset;
    }
    return self;
}

- (NSPoint)lastMousePosition
{
    return _lastMousePosition;
}

- (void)setLastMousePosition:(NSPoint)lastMousePosition
{
    _lastMousePosition = lastMousePosition;
}

- (NSWindow *)draggingDestinationWindow
{
    return _window.get();
}

- (NSDragOperation)draggingSourceOperationMask
{
    return NSDragOperationCopy;
}

- (NSPoint)draggingLocation
{
    return _lastMousePosition;
}

- (NSPoint)draggedImageLocation
{
    return NSMakePoint(_lastMousePosition.x + _offset.width, _lastMousePosition.y + _offset.height);
}

- (NSImage *)draggedImage
{
    return _image.get();
}

- (NSPasteboard *)draggingPasteboard
{
    return _pasteboard.get();
}

- (id)draggingSource
{
    return _source.get();
}

- (NSInteger)draggingSequenceNumber
{
    return 0;
}

- (void)slideDraggedImageTo:(NSPoint)screenPoint
{
}

IGNORE_WARNINGS_BEGIN("deprecated-implementations")
- (NSArray *)namesOfPromisedFilesDroppedAtDestination:(NSURL *)dropDestination
IGNORE_WARNINGS_END
{
    return nil;
}

- (NSDraggingFormation)draggingFormation
{
    return NSDraggingFormationDefault;
}

- (void)setDraggingFormation:(NSDraggingFormation)formation
{
}

- (BOOL)animatesToDestination
{
    return NO;
}

- (void)setAnimatesToDestination:(BOOL)flag
{
}

- (NSInteger)numberOfValidItemsForDrop
{
    return _numberOfValidItemsForDrop;
}

- (void)setNumberOfValidItemsForDrop:(NSInteger)number
{
    _numberOfValidItemsForDrop = number;
}

- (void)enumerateDraggingItemsWithOptions:(NSEnumerationOptions)enumOpts forView:(NSView *)view classes:(NSArray *)classArray searchOptions:(NSDictionary *)searchOptions usingBlock:(void (^)(NSDraggingItem *draggingItem, NSInteger idx, BOOL *stop))block
{
}

- (NSSpringLoadingHighlight)springLoadingHighlight
{
    return NSSpringLoadingHighlightNone;
}

- (void)resetSpringLoading
{
}

@end

namespace TestWebKitAPI {

static NSImage *getTestImage()
{
    return [[[NSImage alloc] initWithContentsOfURL:[[NSBundle mainBundle] URLForResource:@"icon" withExtension:@"png" subdirectory:@"TestWebKitAPI.resources"]] autorelease];
}

static WebView *webViewAfterPerformingDragOperation(NSPasteboard *pasteboard)
{
    RetainPtr<WebView> destination = adoptNS([[WebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    RetainPtr<NSWindow> hostWindow = adoptNS([[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 400, 400) styleMask:0 backing:NSBackingStoreBuffered defer:NO]);
    [hostWindow setFrameOrigin:NSMakePoint(0, 0)];
    [[hostWindow contentView] addSubview:destination.get()];
    __block bool isDone = false;
    [destination setFrameLoadDelegate:[FrameLoadCompletionListener listenerWithCompletionBlock:^() {
        RetainPtr<DragSource> source = adoptNS([[DragSource alloc] init]);
        RetainPtr<DragInfo> info = adoptNS([[DragInfo alloc] initWithImage:getTestImage() offset:NSMakeSize(0, 0) pasteboard:pasteboard source:source.get() destinationWindow:hostWindow.get()]);
        [info setLastMousePosition:NSMakePoint(0, 200)];
        [destination draggingEntered:info.get()];
        [info setLastMousePosition:NSMakePoint(200, 200)];
        [destination draggingUpdated:info.get()];

        EXPECT_TRUE([destination performDragOperation:info.get()]);
        isDone = true;
    }]];
    [[destination mainFrame] loadRequest:[NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"full-page-contenteditable" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]]];

    TestWebKitAPI::Util::run(&isDone);
    return destination.get();
}

TEST(LegacyDragAndDropTests, DropUTF8PlainText)
{
    NSPasteboard *pasteboard = [NSPasteboard pasteboardWithUniqueName];
    [pasteboard setData:[@"I am a WebKit." dataUsingEncoding:NSUTF8StringEncoding] forType:(__bridge NSString *)kUTTypeUTF8PlainText];

    RetainPtr<WebView> resultingWebView = webViewAfterPerformingDragOperation(pasteboard);
    EXPECT_TRUE([[resultingWebView stringByEvaluatingJavaScriptFromString:@"document.body.textContent"] containsString:@"I am a WebKit."]);
}

TEST(LegacyDragAndDropTests, DropJPEG)
{
    NSPasteboard *pasteboard = [NSPasteboard pasteboardWithUniqueName];
    NSImage *icon = getTestImage();
    NSBitmapImageRep *imageRep = [NSBitmapImageRep imageRepWithData:icon.TIFFRepresentation];
    [pasteboard setData:[imageRep representationUsingType:NSJPEGFileType properties:@{ NSImageCompressionFactor: @(0.9) }] forType:(__bridge NSString *)kUTTypeJPEG];

    RetainPtr<WebView> resultingWebView = webViewAfterPerformingDragOperation(pasteboard);
    EXPECT_TRUE([[resultingWebView stringByEvaluatingJavaScriptFromString:@"document.querySelector('img').tagName === 'IMG'"] isEqualToString:@"true"]);
}

} // namespace TestWebKitAPI

#endif
