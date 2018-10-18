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
#include "WebIconUtilities.h"

#if PLATFORM(IOS_FAMILY)

#import "UIKitSPI.h"
#import <AVFoundation/AVFoundation.h>
#import <CoreGraphics/CoreGraphics.h>
#import <CoreMedia/CoreMedia.h>
#import <ImageIO/ImageIO.h>
#import <MobileCoreServices/MobileCoreServices.h>
#import <wtf/MathExtras.h>
#import <wtf/RetainPtr.h>
#import <wtf/SoftLinking.h>

SOFT_LINK_FRAMEWORK(AVFoundation);
SOFT_LINK_CLASS(AVFoundation, AVAssetImageGenerator);
SOFT_LINK_CLASS(AVFoundation, AVURLAsset);

SOFT_LINK_FRAMEWORK(CoreMedia);
SOFT_LINK_CONSTANT(CoreMedia, kCMTimeZero, CMTime);

#define kCMTimeZero getkCMTimeZero()

namespace WebKit {

static const CGFloat iconSideLength = 100;

static CGRect squareCropRectForSize(CGSize size)
{
    CGFloat smallerSide = std::min(size.width, size.height);
    CGRect cropRect = CGRectMake(0, 0, smallerSide, smallerSide);

    if (size.width < size.height)
        cropRect.origin.y = std::round((size.height - smallerSide) / 2);
    else
        cropRect.origin.x = std::round((size.width - smallerSide) / 2);

    return cropRect;
}

static UIImage *squareImage(CGImageRef image)
{
    if (!image)
        return nil;

    CGSize imageSize = CGSizeMake(CGImageGetWidth(image), CGImageGetHeight(image));
    if (imageSize.width == imageSize.height)
        return [UIImage imageWithCGImage:image];

    CGRect squareCropRect = squareCropRectForSize(imageSize);
    RetainPtr<CGImageRef> squareImage = adoptCF(CGImageCreateWithImageInRect(image, squareCropRect));
    return [UIImage imageWithCGImage:squareImage.get()];
}

static UIImage *thumbnailSizedImageForImage(CGImageRef image)
{
    UIImage *squaredImage = squareImage(image);
    if (!squaredImage)
        return nil;

    CGRect destRect = CGRectMake(0, 0, iconSideLength, iconSideLength);
    UIGraphicsBeginImageContext(destRect.size);
    CGContextSetInterpolationQuality(UIGraphicsGetCurrentContext(), kCGInterpolationHigh);
    [squaredImage drawInRect:destRect];
    UIImage *resultImage = UIGraphicsGetImageFromCurrentImageContext();
    UIGraphicsEndImageContext();
    return resultImage;
}

UIImage* fallbackIconForFile(NSURL *file)
{
    ASSERT_ARG(file, [file isFileURL]);

    UIDocumentInteractionController *interactionController = [UIDocumentInteractionController interactionControllerWithURL:file];
    return thumbnailSizedImageForImage(interactionController.icons[0].CGImage);
}

UIImage* iconForImageFile(NSURL *file)
{
    ASSERT_ARG(file, [file isFileURL]);

    NSDictionary *options = @{
        (id)kCGImageSourceCreateThumbnailFromImageIfAbsent: @YES,
        (id)kCGImageSourceThumbnailMaxPixelSize: @(iconSideLength),
        (id)kCGImageSourceCreateThumbnailWithTransform: @YES,
    };
    RetainPtr<CGImageSource> imageSource = adoptCF(CGImageSourceCreateWithURL((CFURLRef)file, 0));
    RetainPtr<CGImageRef> thumbnail = adoptCF(CGImageSourceCreateThumbnailAtIndex(imageSource.get(), 0, (CFDictionaryRef)options));
    if (!thumbnail) {
        LOG_ERROR("Error creating thumbnail image for image: %@", file);
        return fallbackIconForFile(file);
    }

    return thumbnailSizedImageForImage(thumbnail.get());
}

UIImage* iconForVideoFile(NSURL *file)
{
    ASSERT_ARG(file, [file isFileURL]);

    RetainPtr<AVURLAsset> asset = adoptNS([allocAVURLAssetInstance() initWithURL:file options:nil]);
    RetainPtr<AVAssetImageGenerator> generator = adoptNS([allocAVAssetImageGeneratorInstance() initWithAsset:asset.get()]);
    [generator setAppliesPreferredTrackTransform:YES];

    NSError *error = nil;
    RetainPtr<CGImageRef> imageRef = adoptCF([generator copyCGImageAtTime:kCMTimeZero actualTime:nil error:&error]);
    if (!imageRef) {
        LOG_ERROR("Error creating image for video '%@': %@", file, error);
        return fallbackIconForFile(file);
    }

    return thumbnailSizedImageForImage(imageRef.get());
}

UIImage* iconForFile(NSURL *file)
{
    ASSERT_ARG(file, [file isFileURL]);

    NSString *fileExtension = file.pathExtension;
    if (!fileExtension.length)
        return nil;

    RetainPtr<CFStringRef> fileUTI = adoptCF(UTTypeCreatePreferredIdentifierForTag(kUTTagClassFilenameExtension, (CFStringRef)fileExtension, 0));

    if (UTTypeConformsTo(fileUTI.get(), kUTTypeImage))
        return iconForImageFile(file);

    if (UTTypeConformsTo(fileUTI.get(), kUTTypeMovie))
        return iconForVideoFile(file);

    return fallbackIconForFile(file);
}

}

#endif
