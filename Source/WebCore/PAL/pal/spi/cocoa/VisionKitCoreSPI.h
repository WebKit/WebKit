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

#include <wtf/Compiler.h>
#include <wtf/Platform.h>

#if !PLATFORM(IOS_SIMULATOR) || !__has_feature(modules)

DECLARE_SYSTEM_HEADER

#if HAVE(VK_IMAGE_ANALYSIS)

// FIXME: Remove the `__has_feature(modules)` condition when possible.
#if USE(APPLE_INTERNAL_SDK) && !__has_feature(modules)

#if __has_include(<TextRecognition/CRRegion.h>)
#import <TextRecognition/CRRegion.h>
#endif

// FIXME: Remove this after rdar://109896407 is resolved
IGNORE_WARNINGS_BEGIN("undef")
#import <VisionKitCore/VKImageAnalysis_WebKit.h>
IGNORE_WARNINGS_END
#import <VisionKitCore/VisionKitCore.h>

#else

#import <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>

#if PLATFORM(IOS_FAMILY)
#import <UIKit/UIKit.h>
#elif PLATFORM(MAC)
#import <AppKit/AppKit.h>
#endif

#if HAVE(VK_IMAGE_ANALYSIS_FOR_MACHINE_READABLE_CODES)
@class BCSAction;
#endif

NS_ASSUME_NONNULL_BEGIN

typedef int32_t VKImageAnalysisRequestID;

typedef NS_OPTIONS(NSUInteger, VKImageAnalysisInteractionTypes) {
    VKImageAnalysisInteractionTypeTextSelection = 1 << 0,
    VKImageAnalysisInteractionTypeDataDetectors = 1 << 1,
    VKImageAnalysisInteractionTypeVisualSearch  = 1 << 2,
    VKImageAnalysisInteractionTypeImageSubject  = 1 << 3,
    VKImageAnalysisInteractionTypeNone = 0,
    VKImageAnalysisInteractionTypeAll = NSUIntegerMax,
};

typedef NS_OPTIONS(NSUInteger, VKAnalysisTypes) {
    VKAnalysisTypeText                 = 1 << 0,
    VKAnalysisTypeTextDataDetector     = 1 << 1,
    VKAnalysisTypeMachineReadableCode  = 1 << 2,
    VKAnalysisTypeAppClip              = 1 << 3,
    VKAnalysisTypeVisualSearch         = 1 << 4,
    VKAnalysisTypeImageSegmentation    = 1 << 5,
    VKAnalysisTypeNone = 0,
    VKAnalysisTypeAll = NSUIntegerMax,
};

typedef NS_ENUM(NSUInteger, VKImageAnalyzerRequestImageSource) {
    VKImageAnalyzerRequestImageSourceDefault,
    VKImageAnalyzerRequestImageSourceScreenshot,
    VKImageAnalyzerRequestImageSourceVideoFrame,
};

#if PLATFORM(IOS_FAMILY)

typedef UIImage VKImageClass;
typedef UIImageOrientation VKImageOrientation;

#define VKImageOrientationUp            UIImageOrientationUp
#define VKImageOrientationDown          UIImageOrientationDown
#define VKImageOrientationLeft          UIImageOrientationLeft
#define VKImageOrientationRight         UIImageOrientationRight
#define VKImageOrientationUpMirrored    UIImageOrientationUpMirrored
#define VKImageOrientationDownMirrored  UIImageOrientationDownMirrored
#define VKImageOrientationLeftMirrored  UIImageOrientationLeftMirrored
#define VKImageOrientationRightMirrored UIImageOrientationRightMirrored

#else

typedef NSImage VKImageClass;

typedef NS_ENUM(NSInteger, VKImageOrientation) {
    VKImageOrientationUp,
    VKImageOrientationDown,
    VKImageOrientationLeft,
    VKImageOrientationRight,
    VKImageOrientationUpMirrored,
    VKImageOrientationDownMirrored,
    VKImageOrientationLeftMirrored,
    VKImageOrientationRightMirrored,
};

#endif

@interface VKImageAnalysis : NSObject <NSSecureCoding>
- (BOOL)hasResultsForAnalysisTypes:(VKAnalysisTypes)analysisTypes;
@end

@protocol VKFeedbackAssetsProvider <NSObject>
- (BOOL)saveAssetsToFeedbackAttachmentsFolder:(NSURL *)attachmentsFolderURL error:(NSError *)error;
@end

@interface VKImageAnalyzerRequest : NSObject <NSCopying, VKFeedbackAssetsProvider>
- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithImage:(VKImageClass *)image orientation:(VKImageOrientation)orientation requestType:(VKAnalysisTypes)analysisType;
- (instancetype)initWithCGImage:(CGImageRef)image orientation:(VKImageOrientation)orientation requestType:(VKAnalysisTypes)analysisType;
@end

@interface VKImageAnalyzerRequest (VI)
@property (nonatomic) NSURL *imageURL;
@property (nonatomic) NSURL *pageURL;
@end

@interface VKImageAnalyzer : NSObject
@property (nonatomic, strong, nullable) dispatch_queue_t callbackQueue;
- (VKImageAnalysisRequestID)processRequest:(VKImageAnalyzerRequest *)request progressHandler:(void (^_Nullable)(double progress))progressHandler completionHandler:(void (^)(VKImageAnalysis *_Nullable analysis, NSError *_Nullable error))completionHandler;
- (void)cancelAllRequests;
@end

@interface VKQuad : NSObject <NSCopying, NSSecureCoding>
@property (nonatomic, readonly) CGPoint topLeft;
@property (nonatomic, readonly) CGPoint topRight;
@property (nonatomic, readonly) CGPoint bottomLeft;
@property (nonatomic, readonly) CGPoint bottomRight;
@property (nonatomic, readonly) CGRect boundingBox;
@end

@interface VKWKTextInfo : NSObject
@property (nonatomic, readonly) NSString *string;
@property (nonatomic, readonly) VKQuad *quad;
@end

@interface VKWKLineInfo : VKWKTextInfo
@property (nonatomic, readonly) NSArray<VKWKTextInfo *> *children;
@property (nonatomic, readonly) BOOL shouldWrap;
@end

@class DDScannerResult;

@interface VKWKDataDetectorInfo : NSObject
@property (nonatomic, readonly) DDScannerResult *result;
@property (nonatomic, readonly) NSArray<VKQuad *> *boundingQuads;
@end

@interface VKImageAnalysis (WebKitSPI)
@property (nonatomic, readonly) NSArray<VKWKLineInfo *> *allLines;
@property (nonatomic, readonly) NSArray<VKWKDataDetectorInfo *> *textDataDetectors;
#if HAVE(VK_IMAGE_ANALYSIS_FOR_MACHINE_READABLE_CODES)
@property (nonatomic) UIMenu *mrcMenu;
@property (nonatomic, nullable, weak) UIViewController *presentingViewControllerForMrcAction;
@property (nonatomic) CGRect rectForMrcActionInPresentingViewController;
@property (nonatomic, readonly) NSArray<BCSAction *> *barcodeActions;
#endif
@end

NS_ASSUME_NONNULL_END

#endif

@interface VKWKLineInfo (Staging_85139101)
@property (nonatomic, readonly) NSUInteger layoutDirection;
@end

#if HAVE(VK_IMAGE_ANALYSIS_FOR_MACHINE_READABLE_CODES)
@interface VKImageAnalysis (Staging_127892794)
@property (nonatomic) CGRect rectForMrcActionInPresentingViewController;
@end
#endif

#if ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)

#if __has_include(<VisionKitCore/VKCImageAnalysisTranslation.h>) && !__has_feature(modules)
#import <VisionKitCore/VKCImageAnalysisTranslation.h>
#else

NS_ASSUME_NONNULL_BEGIN

@interface VKCTranslatedParagraph : NSObject
@property (nonatomic, readonly) VKQuad *quad;
@property (nonatomic, readonly) NSString *text;
@property (nonatomic, readonly) BOOL isPassthrough;
@end

@interface VKCImageAnalysisTranslation : NSObject
@property (nonatomic, readonly) NSArray<VKCTranslatedParagraph *> *paragraphs;
@end

NS_ASSUME_NONNULL_END

#endif

#if __has_include(<VisionKitCore/VKCImageAnalysis.h>) && !__has_feature(modules)
#import <VisionKitCore/VKCImageAnalysis.h>
#else

NS_ASSUME_NONNULL_BEGIN

@interface VKCImageAnalysis : VKImageAnalysis
- (NSAttributedString *)_attributedStringForRange:(NSRange)range;
- (void)translateTo:(NSString *)targetLanguage withCompletion:(void (^)(VKCImageAnalysisTranslation *translation, NSError * _Nullable))completion;
- (void)translateFrom:(NSString *)sourceLanguage to:(NSString *)targetLanguage withCompletion:(void (^)(VKCImageAnalysisTranslation *translation, NSError *))completion;
@end

NS_ASSUME_NONNULL_END

#endif

#if __has_include(<VisionKitCore/VKImageClass_Private.h>) && !__has_feature(modules)
#import <VisionKitCore/VKImageClass_Private.h>
#else

NS_ASSUME_NONNULL_BEGIN

typedef void (^VKCGImageRemoveBackgroundCompletion)(CGImageRef, CGRect cropRect, NSError *);

WTF_EXTERN_C_BEGIN

void vk_cgImageRemoveBackground(CGImageRef, BOOL cropToFit, VKCGImageRemoveBackgroundCompletion);
void vk_cgImageRemoveBackgroundWithDownsizing(CGImageRef, BOOL canDownsize, BOOL cropToFit, void(^completion)(CGImageRef, NSError *));

WTF_EXTERN_C_END

NS_ASSUME_NONNULL_END

#endif

#if __has_include(<VisionKitCore/VKCRemoveBackgroundRequestHandler.h>) && !__has_feature(modules)
#import <VisionKitCore/VKCRemoveBackgroundRequest.h>
#import <VisionKitCore/VKCRemoveBackgroundRequestHandler.h>
#import <VisionKitCore/VKCRemoveBackgroundResult.h>
#else

NS_ASSUME_NONNULL_BEGIN

@interface VKCRemoveBackgroundResult : NSObject
@property (nonatomic, readonly) CGRect cropRect;
- (CGImageRef)createCGImage;
@end

@interface VKCRemoveBackgroundRequest : NSObject
- (instancetype)initWithCGImage:(CGImageRef)image;
@property (nonatomic, readonly) CGImageRef CGImage;
@end

@interface VKCRemoveBackgroundRequestHandler : NSObject
- (void)performRequest:(VKCRemoveBackgroundRequest *)request completion:(void (^)(VKCRemoveBackgroundResult *result, NSError *error))completion;
@end

NS_ASSUME_NONNULL_END

#endif

#if __has_include(<VisionKitCore/VKCImageAnalyzer.h>) && !__has_feature(modules)
#import <VisionKitCore/VKCImageAnalyzer.h>
#import <VisionKitCore/VKCImageAnalyzerRequest.h>
#else

NS_ASSUME_NONNULL_BEGIN

@interface VKCImageAnalyzerRequest : NSObject <NSCopying, VKFeedbackAssetsProvider>
@property (nonatomic, copy, nullable) NSURL *imageURL;
@property (nonatomic, copy, nullable) NSURL *pageURL;
@property (nonatomic) VKImageAnalyzerRequestImageSource imageSource;
- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithCGImage:(CGImageRef)image orientation:(VKImageOrientation)orientation requestType:(VKAnalysisTypes)analysisType;
@end

@interface VKCImageAnalyzer : NSObject
@property (nonatomic, strong, nullable) dispatch_queue_t callbackQueue;
@property (nonatomic, class, readonly) NSArray<NSString *> *supportedRecognitionLanguages;
- (void)cancelAllRequests;
- (void)cancelRequestID:(VKImageAnalysisRequestID)requestID;
- (VKImageAnalysisRequestID)processRequest:(VKCImageAnalyzerRequest *)request progressHandler:(void (^_Nullable)(double progress))progressHandler completionHandler:(void (^)(VKCImageAnalysis* _Nullable analysis, NSError * _Nullable error))completionHandler;
@end

NS_ASSUME_NONNULL_END
#endif

#if PLATFORM(MAC)
#if __has_include(<VisionKitCore/VKCImageAnalysisOverlayView.h>) && !__has_feature(modules)
#import <VisionKitCore/VKCImageAnalysisOverlayView.h>
#else

NS_ASSUME_NONNULL_BEGIN

@protocol VKCImageAnalysisOverlayViewDelegate <NSObject>
@end

@interface VKCImageAnalysisOverlayView : NSView
@property (nonatomic) VKImageAnalysisInteractionTypes activeInteractionTypes;
@property (nonatomic) BOOL actionInfoLiveTextButtonDisabled;
@property (nonatomic) BOOL actionInfoQuickActionsDisabled;
@property (nullable, nonatomic, strong) VKCImageAnalysis *analysis;
@property (nonatomic, weak) id<VKCImageAnalysisOverlayViewDelegate> delegate;
@property (nonatomic) BOOL wantsAutomaticContentsRectCalculation;

- (BOOL)interactableItemExistsAtPoint:(CGPoint)point;
- (void)setActionInfoViewHidden:(BOOL)hidden animated:(BOOL)animated;
@end

NS_ASSUME_NONNULL_END

#endif
#endif // PLATFORM(MAC)

#if PLATFORM(IOS) || PLATFORM(VISION) || PLATFORM(MACCATALYST)
#if __has_include(<VisionKitCore/VKCImageAnalysisInteraction.h>) && !__has_feature(modules)
#import <VisionKitCore/VKCImageAnalysisInteraction.h>
#else

NS_ASSUME_NONNULL_BEGIN

@protocol VKCImageAnalysisInteractionDelegate <NSObject>
@end

@interface VKCImageAnalysisInteraction : NSObject <UIInteraction>
@property (nonatomic) BOOL actionInfoLiveTextButtonDisabled;
@property (nonatomic) BOOL actionInfoQuickActionsDisabled;
@property (nonatomic) BOOL actionInfoViewHidden;
@property (nonatomic) VKImageAnalysisInteractionTypes activeInteractionTypes;
@property (nullable, nonatomic, strong) VKCImageAnalysis *analysis;
@property (nonatomic, readonly) UIButton *analysisButton;
@property (nonatomic) BOOL analysisButtonRequiresVisibleContentGating;
@property (nonatomic, weak) id<VKCImageAnalysisInteractionDelegate> delegate;
@property (nonatomic, readonly) BOOL hasActiveTextSelection;
@property (nonatomic, readwrite) BOOL highlightSelectableItems;
@property (nonatomic, readwrite, copy, nullable) UIButtonConfigurationUpdateHandler quickActionConfigurationUpdateHandler;
@property (nonatomic) BOOL wantsAutomaticContentsRectCalculation;
- (BOOL)interactableItemExistsAtPoint:(CGPoint)point;
- (void)resetSelection;
- (void)setActionInfoViewHidden:(BOOL)hidden animated:(BOOL)animated;
@end

NS_ASSUME_NONNULL_END

#endif
#endif // PLATFORM(IOS) || PLATFORM(VISION) || PLATFORM(MACCATALYST)

#endif // ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)

#endif // HAVE(VK_IMAGE_ANALYSIS)

#endif // !PLATFORM(IOS_SIMULATOR) || !__has_feature(modules)
