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

#if HAVE(VK_IMAGE_ANALYSIS)

#if USE(APPLE_INTERNAL_SDK)

#import <VisionKitCore/VKImageAnalysis_WebKit.h>
#import <VisionKitCore/VisionKitCore.h>

#else

#import <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>

#if PLATFORM(IOS_FAMILY)
#import <UIKit/UIKit.h>
#endif

NS_ASSUME_NONNULL_BEGIN

typedef int32_t VKImageAnalysisRequestID;

typedef NS_OPTIONS(NSUInteger, VKAnalysisTypes) {
    VKAnalysisTypeText                 = 1 << 0,
    VKAnalysisTypeTextDataDetector     = 1 << 1,
    VKAnalysisTypeMachineReadableCode  = 1 << 2,
    VKAnalysisTypeAppClip              = 1 << 3,
    VKAnalysisTypeVisualSearch         = 1 << 4,
    VKAnalysisTypeNone = 0,
    VKAnalysisTypeAll = NSUIntegerMax,
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
#endif
@end

NS_ASSUME_NONNULL_END

#endif

#endif // HAVE(VK_IMAGE_ANALYSIS)
