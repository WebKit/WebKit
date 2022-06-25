/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#if HAVE(SCREEN_CAPTURE_KIT) && HAVE(SC_CONTENT_SHARING_SESSION)

#import <ScreenCaptureKit/ScreenCaptureKit.h>

#if USE(APPLE_INTERNAL_SDK)

#import <ScreenCaptureKit/SCContentFilterPrivate.h>
#import <ScreenCaptureKit/SCContentSharingSession.h>
#import <ScreenCaptureKit/SCStream_Private.h>

#else // USE(APPLE_INTERNAL_SDK)

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, SCContentFilterType) {
    SCContentFilterTypeNothing,
    SCContentFilterTypeDisplay,
    SCContentFilterTypeAppsAndWindowsPinnedToDisplay,
    SCContentFilterTypeDesktopIndependentWindow,
    SCContentFilterTypeClientShouldImplementDefault
};

@interface SCContentFilterDesktopIndependentWindowInformation : NSObject
@property (nonatomic, readonly) SCWindow *window;
@end

@interface SCContentFilterDisplayInformation : NSObject
@property (nonatomic, readonly) SCDisplay *display;
@end

@interface SCContentFilter ()
@property (nonatomic, strong) SCWindow *window;
@property (nonatomic, strong) SCDisplay *display;
@property (nonatomic, assign) SCContentFilterType type;
@property (nullable, nonatomic, readonly) SCContentFilterDesktopIndependentWindowInformation *desktopIndependentWindowInfo;
@property (nullable, nonatomic, readonly) SCContentFilterDisplayInformation *displayInfo;
@end

@class SCContentSharingSession;

@protocol SCContentSharingSessionProtocol <NSObject>
@optional
- (void)sessionDidEnd:(SCContentSharingSession *)session;
- (void)sessionDidChangeContent:(SCContentSharingSession *)session;
- (void)pickerCanceledForSession:(SCContentSharingSession *)session;
@end

@interface SCContentSharingSession : NSObject
@property (nonatomic, weak) id<SCContentSharingSessionProtocol> delegate;
@property (nonatomic, readonly, copy) SCContentFilter *content;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithTitle:(NSString *)title;
- (BOOL)isEqualToSharingSession:(SCContentSharingSession *)other;
- (void)showPickerForType:(SCContentFilterType)type;
- (void)updateContent:(SCContentFilter *)content;
- (void)end;
@end

@interface SCStream (SCContentSharing) <SCContentSharingSessionProtocol>
- (instancetype)initWithSharingSession:(SCContentSharingSession *)session captureOutputProperties:(SCStreamConfiguration *)streamConfig delegate:(id<SCStreamDelegate>)delegate;
@end

NS_ASSUME_NONNULL_END

#endif // USE(APPLE_INTERNAL_SDK)

#endif // HAVE(SCREEN_CAPTURE_KIT)
