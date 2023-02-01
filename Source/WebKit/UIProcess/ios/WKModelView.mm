/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "WKModelView.h"

#if ENABLE(ARKIT_INLINE_PREVIEW_IOS)

#import "Logging.h"
#import "RemoteLayerTreeViews.h"
#import "WKModelInteractionGestureRecognizer.h"
#import "WebPageProxy.h"
#import "WebsiteDataStore.h"
#import <WebCore/Model.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <pal/spi/ios/SystemPreviewSPI.h>
#import <wtf/Assertions.h>
#import <wtf/FileSystem.h>
#import <wtf/RetainPtr.h>
#import <wtf/SoftLinking.h>
#import <wtf/UUID.h>

SOFT_LINK_PRIVATE_FRAMEWORK(AssetViewer);
SOFT_LINK_CLASS(AssetViewer, ASVInlinePreview);

@implementation WKModelView {
    RetainPtr<ASVInlinePreview> _preview;
    RetainPtr<WKModelInteractionGestureRecognizer> _modelInteractionGestureRecognizer;
    String _filePath;
    CGRect _lastBounds;
    WebCore::GraphicsLayer::PlatformLayerID _layerID;
    WeakPtr<WebKit::WebPageProxy> _page;
}

- (ASVInlinePreview *)preview
{
    return _preview.get();
}

- (instancetype)initWithFrame:(CGRect)frame
{
    return nil;
}

- (instancetype)initWithCoder:(NSCoder *)coder
{
    return nil;
}

- (instancetype)initWithModel:(WebCore::Model&)model layerID:(WebCore::GraphicsLayer::PlatformLayerID)layerID page:(WebKit::WebPageProxy&)page
{
    _lastBounds = CGRectZero;
    self = [super initWithFrame:_lastBounds];
    if (!self)
        return nil;

    _layerID = layerID;
    _page = page;

    [self createFileForModel:model];
    [self updateBounds];

    return self;
}

- (BOOL)createFileForModel:(WebCore::Model&)model
{
    auto pathToDirectory = WebKit::WebsiteDataStore::defaultModelElementCacheDirectory();
    if (pathToDirectory.isEmpty())
        return NO;

    auto directoryExists = FileSystem::fileExists(pathToDirectory);
    if (directoryExists && FileSystem::fileTypeFollowingSymlinks(pathToDirectory) != FileSystem::FileType::Directory) {
        ASSERT_NOT_REACHED();
        return NO;
    }

    if (!directoryExists && !FileSystem::makeAllDirectories(pathToDirectory)) {
        ASSERT_NOT_REACHED();
        return NO;
    }

    String fileName = makeString(UUID::createVersion4(), ".usdz"_s);
    auto filePath = FileSystem::pathByAppendingComponent(pathToDirectory, fileName);
    auto file = FileSystem::openFile(filePath, FileSystem::FileOpenMode::Truncate);
    if (file <= 0)
        return NO;

    auto byteCount = static_cast<std::size_t>(FileSystem::writeToFile(file, model.data()->data(), model.data()->size()));
    ASSERT_UNUSED(byteCount, byteCount == model.data()->size());
    FileSystem::closeFile(file);
    _filePath = filePath;

    return YES;
}

- (void)createPreview
{
    if (!_filePath)
        return;

    auto bounds = self.bounds;
    ASSERT(!CGRectEqualToRect(bounds, CGRectZero));

    _preview = adoptNS([allocASVInlinePreviewInstance() initWithFrame:bounds]);
    [self.layer addSublayer:[_preview layer]];

    auto url = adoptNS([[NSURL alloc] initFileURLWithPath:_filePath]);

    [_preview setupRemoteConnectionWithCompletionHandler:^(NSError *contextError) {
        if (contextError) {
            LOG(ModelElement, "Unable to create remote connection, error: %@", [contextError localizedDescription]);
            _page->modelInlinePreviewDidFailToLoad(_layerID, WebCore::ResourceError { contextError });
            return;
        }

        [_preview preparePreviewOfFileAtURL:url.get() completionHandler:^(NSError *loadError) {
            if (loadError) {
                LOG(ModelElement, "Unable to load file, error: %@", [loadError localizedDescription]);
                _page->modelInlinePreviewDidFailToLoad(_layerID, WebCore::ResourceError { loadError });
                return;
            }

            LOG(ModelElement, "File loaded successfully.");
            _page->modelInlinePreviewDidLoad(_layerID);
        }];
    }];

    _modelInteractionGestureRecognizer = adoptNS([[WKModelInteractionGestureRecognizer alloc] init]);
    [self addGestureRecognizer:_modelInteractionGestureRecognizer.get()];
}

- (void)layoutSubviews
{
    [super layoutSubviews];
    [self updateBounds];
}

- (void)updateBounds
{
    auto bounds = self.bounds;
    if (CGRectEqualToRect(_lastBounds, bounds))
        return;

    _lastBounds = bounds;

    if (!_preview) {
        [self createPreview];
        return;
    }

    [_preview updateFrame:bounds completionHandler:^(CAFenceHandle *fenceHandle, NSError *error) {
        if (error) {
            LOG(ModelElement, "Unable to update frame, error: %@", [error localizedDescription]);
            [fenceHandle invalidate];
            return;
        }

        dispatch_async(dispatch_get_main_queue(), ^{
            [self.layer.context addFence:fenceHandle];
            [_preview setFrameWithinFencedTransaction:bounds];
            [fenceHandle invalidate];
        });
    }];
}

- (UIView *)hitTest:(CGPoint)point withEvent:(UIEvent *)event
{
    // The layer of this view is empty and the sublayer is rendered remotely, so the basic implementation
    // of hitTest:withEvent: will return nil due to ignoring empty subviews. So we can simply check whether
    // the hit-testing point is within bounds.
    return [self pointInside:point withEvent:event] ? self : nil;
}

@end

#endif

