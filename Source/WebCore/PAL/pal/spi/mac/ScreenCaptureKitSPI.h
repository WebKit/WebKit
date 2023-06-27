/*
 * Copyright (C) 2022-2023 Apple Inc. All rights reserved.
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

#if HAVE(SCREEN_CAPTURE_KIT)

#import <ScreenCaptureKit/ScreenCaptureKit.h>

#if USE(APPLE_INTERNAL_SDK)

#import <ScreenCaptureKit/SCContentFilterPrivate.h>
#import <ScreenCaptureKit/SCContentSharingSession.h>
#import <ScreenCaptureKit/SCStream_Private.h>

#if HAVE(SC_CONTENT_SHARING_PICKER)
#if __has_include(<ScreenCaptureKit/SCContentSharingPicker.h>)
#import <ScreenCaptureKit/SCContentSharingPicker.h>
#endif

#if __has_include(<ScreenCaptureKit/SCContentSharingPicker_Private.h>)
#import <ScreenCaptureKit/SCContentSharingPicker_Private.h>
#endif
#endif // HAVE(SC_CONTENT_SHARING_PICKER)

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
- (void)showPickerForType:(SCContentFilterType)type;
- (void)end;
@end

@interface SCStream (SCContentSharing) <SCContentSharingSessionProtocol>
- (instancetype)initWithSharingSession:(SCContentSharingSession *)session captureOutputProperties:(SCStreamConfiguration *)streamConfig delegate:(id<SCStreamDelegate>)delegate;
@end

@protocol SCContentSharingPickerDelegate <NSObject>
@required
@optional
- (void)contentSharingPicker:(SCContentSharingPicker *)picker didCancelForStream:(nullable SCStream *)stream;
- (void)contentSharingPickerStartDidFailWithError:(NSError *)error;
- (void)contentSharingPickerDidCancel:(SCContentSharingPicker *)picker forStream:(nullable SCStream *)stream;
@end

typedef NS_OPTIONS(NSUInteger, SCContentSharingAllowedPickerModes) {
    SCContentSharingPickerAllowedModeSingleWindow          = 1 << 0,
    SCContentSharingPickerAllowedModeMultipleWindows       = 1 << 1,
    SCContentSharingPickerAllowedModeSingleApplication     = 1 << 2,
    SCContentSharingPickerAllowedModeSingleDisplay         = 1 << 3,
};

// FIXME: Drop deprecated method names once we no longer support Ventura. These
// SPI methods all have public equivalents in Sonoma.
@interface SCContentSharingPickerConfiguration
#if HAVE(SC_CONTENT_SHARING_PICKER)
    ()
#else
    : NSObject
#endif
@property (nonatomic, assign) SCContentSharingAllowedPickerModes allowedPickingModes;
@end

@interface SCContentSharingPicker
#if HAVE(SC_CONTENT_SHARING_PICKER)
    ()
#else
    : NSObject
#endif
@property (nonatomic, weak, nullable) id<SCContentSharingPickerDelegate> delegate;
@property (nonatomic, nullable, strong) NSNumber *maxStreamCount;
@property (nonatomic, nullable, copy) SCContentSharingPickerConfiguration *configuration;
@end

NS_ASSUME_NONNULL_END

#endif // USE(APPLE_INTERNAL_SDK)

#endif // HAVE(SCREEN_CAPTURE_KIT)
