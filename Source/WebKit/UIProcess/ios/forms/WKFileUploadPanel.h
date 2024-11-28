/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#if PLATFORM(IOS_FAMILY)

#import <UIKit/UIViewController.h>

@class WKContentView;
@protocol WKFileUploadPanelDelegate;

namespace API {
class OpenPanelParameters;
}

namespace WebKit {
class WebOpenPanelResultListenerProxy;
enum class PickerDismissalReason : uint8_t;
enum class MovedSuccessfully : bool { No, Yes };
enum class KeyboardIsDismissing : bool { No, Yes };

struct TemporaryFileMoveResults {
    MovedSuccessfully operationResult;
    RetainPtr<NSURL> maybeMovedURL;
    RetainPtr<NSURL> temporaryDirectoryURL;
};
}

@interface WKFileUploadPanel : UIViewController
@property (nonatomic, weak) id <WKFileUploadPanelDelegate> delegate;
- (instancetype)initWithView:(WKContentView *)view;
- (void)presentWithParameters:(API::OpenPanelParameters*)parameters resultListener:(WebKit::WebOpenPanelResultListenerProxy*)listener;

- (BOOL)dismissIfNeededWithReason:(WebKit::PickerDismissalReason)reason;

#if USE(UICONTEXTMENU)
- (void)repositionContextMenuIfNeeded:(WebKit::KeyboardIsDismissing)isKeyboardBeingDismissed;
#endif

- (NSArray<NSString *> *)currentAvailableActionTitles;
- (NSArray<NSString *> *)acceptedTypeIdentifiers;

+ (WebKit::TemporaryFileMoveResults)_moveToNewTemporaryDirectory:(NSURL *)originalURL fileCoordinator:(NSFileCoordinator *)fileCoordinator fileManager:(NSFileManager *)fileManager asCopy:(BOOL)asCopy;
@end

@protocol WKFileUploadPanelDelegate <NSObject>
@optional
- (void)fileUploadPanelDidDismiss:(WKFileUploadPanel *)fileUploadPanel;
- (BOOL)fileUploadPanelDestinationIsManaged:(WKFileUploadPanel *)fileUploadPanel;
#if HAVE(PHOTOS_UI)
- (BOOL)fileUploadPanelPhotoPickerPrefersOriginalImageFormat:(WKFileUploadPanel *)fileUploadPanel;
#endif
@end

#endif // PLATFORM(IOS_FAMILY)
