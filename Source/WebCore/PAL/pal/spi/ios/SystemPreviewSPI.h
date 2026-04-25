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

#pragma once

// FIXME: Remove the `__has_feature(modules)` condition when possible.
#if !__has_feature(modules)

#import <wtf/Compiler.h>
#import <wtf/Platform.h>

#if PLATFORM(IOS_FAMILY)

DECLARE_SYSTEM_HEADER

// FIXME: Remove the `__has_feature(modules)` condition when possible.
#if USE(APPLE_INTERNAL_SDK) && !__has_feature(modules)

#if HAVE(ARKIT_QUICK_LOOK_PREVIEW_ITEM)
#import <AssetViewer/ARQuickLookWebKitItem.h>
#endif

#if PLATFORM(IOS) || PLATFORM(VISION)
#import <AssetViewer/ASVThumbnailView.h>
#endif

#if PLATFORM(VISION)
#import <AssetViewer/ASVLaunchPreview.h>
#endif

#else

#import <UIKit/UIKit.h>

#if PLATFORM(IOS) || PLATFORM(VISION)
#import <pal/spi/ios/QuickLookSPI.h>

@class ASVThumbnailView;
@class QLPreviewController;

NS_ASSUME_NONNULL_BEGIN

@protocol ASVThumbnailViewDelegate <NSObject>
- (void)thumbnailView:(ASVThumbnailView *)thumbnailView wantsToPresentPreviewController:(QLPreviewController *)previewController forItem:(QLItem *)item;
@end

@interface ASVThumbnailView : UIView
@property (nonatomic, weak) id<ASVThumbnailViewDelegate> delegate;
@property (nonatomic, assign) QLItem *thumbnailItem;
@property (nonatomic) CGSize maxThumbnailSize;
@end

NS_ASSUME_NONNULL_END

#endif

#if HAVE(ARKIT_QUICK_LOOK_PREVIEW_ITEM)
#import <ARKit/ARKit.h>

NS_ASSUME_NONNULL_BEGIN

#if PLATFORM(VISION) && !HAVE(LIQUID_GLASS)
@interface ARQuickLookPreviewItem : NSObject
@property (nonatomic, strong, nullable) NSURL *canonicalWebPageURL;
- (instancetype)initWithFileAtURL:(NSURL *)url;
@end
#endif

@protocol ARQuickLookWebKitItemDelegate
@end

@class ARQuickLookWebKitItem;

@interface ARQuickLookWebKitItem : QLItem
- (instancetype)initWithPreviewItemProvider:(NSItemProvider *)itemProvider contentType:(NSString *)contentType previewTitle:(NSString *)previewTitle fileSize:(NSNumber *)fileSize previewItem:(ARQuickLookPreviewItem *)previewItem;
- (void)setDelegate:(id <ARQuickLookWebKitItemDelegate>)delegate;
@end

NS_ASSUME_NONNULL_END

#endif

#endif

#endif // PLATFORM(IOS_FAMILY)

#endif // !__has_feature(modules)
