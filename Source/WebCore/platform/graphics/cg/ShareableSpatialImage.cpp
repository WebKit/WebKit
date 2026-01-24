/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include "ShareableSpatialImage.h"

#if ENABLE(SPATIAL_IMAGE_DETECTION)

#include "GraphicsContextCG.h"
#include <ImageIO/ImageIO.h>
#include <wtf/RetainPtr.h>

namespace WebCore {

static RetainPtr<CFDictionaryRef> spatialImageEyePropertiesToDictionary(const SpatialImageEyeProperties& properties, const CFStringRef groupImageSide)
{
    // Returns:
    //     kCGImagePropertyGroups groupsDict
    //         kCGImagePropertyGroupIndex
    //         kCGImagePropertyGroupType
    //         kCGImagePropertyGroupImageIsLeftImage / kCGImagePropertyGroupImageIsRightImage
    //         kCGImagePropertyGroupImageDisparityAdjustment
    //     kCGImagePropertyHEIFDictionary heifDict
    //         kIIOMetadata_CameraExtrinsicsKey extrinsicsDict
    //             kIIOCameraExtrinsics_Position positionArray
    //             kIIOCameraExtrinsics_Rotation rotationArray
    //         kIIOMetadata_CameraModelKey modelDict
    //             kIIOCameraModel_Intrinsics intrinsicsArray

    RetainPtr result = adoptCF(CFDictionaryCreateMutable(nullptr, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

    // Build kCGImagePropertyGroups dictionary.
    RetainPtr groupsDict = adoptCF(CFDictionaryCreateMutable(nullptr, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

    RetainPtr groupIndex = adoptCF(CFNumberCreate(nullptr, kCFNumberIntType, &properties.groupMetadata.groupIndex));
    CFDictionarySetValue(groupsDict.get(), kCGImagePropertyGroupIndex, groupIndex.get());

    CFDictionarySetValue(groupsDict.get(), kCGImagePropertyGroupType, kCGImagePropertyGroupTypeStereoPair);

    CFDictionarySetValue(groupsDict.get(), groupImageSide, kCFBooleanTrue);

    RetainPtr disparity = adoptCF(CFNumberCreate(nullptr, kCFNumberFloatType, &properties.groupMetadata.disparityAdjustment));
    CFDictionarySetValue(groupsDict.get(), kCGImagePropertyGroupImageDisparityAdjustment, disparity.get());

    CFDictionarySetValue(result.get(), kCGImagePropertyGroups, groupsDict.get());

    // Build kCGImagePropertyHEIFDictionary.
    RetainPtr heifDict = adoptCF(CFDictionaryCreateMutable(nullptr, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

    // Camera extrinsics.
    RetainPtr extrinsicsDict = adoptCF(CFDictionaryCreateMutable(nullptr, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

    RetainPtr positionArray = adoptCF(CFArrayCreateMutable(nullptr, 3, &kCFTypeArrayCallBacks));
    for (auto& value : properties.cameraMetadata.extrinsics.position) {
        RetainPtr num = adoptCF(CFNumberCreate(nullptr, kCFNumberFloatType, &value));
        CFArrayAppendValue(positionArray.get(), num.get());
    }
    CFDictionarySetValue(extrinsicsDict.get(), kIIOCameraExtrinsics_Position, positionArray.get());

    RetainPtr rotationArray = adoptCF(CFArrayCreateMutable(nullptr, 9, &kCFTypeArrayCallBacks));
    for (auto& value : properties.cameraMetadata.extrinsics.rotation) {
        RetainPtr num = adoptCF(CFNumberCreate(nullptr, kCFNumberFloatType, &value));
        CFArrayAppendValue(rotationArray.get(), num.get());
    }
    CFDictionarySetValue(extrinsicsDict.get(), kIIOCameraExtrinsics_Rotation, rotationArray.get());

    CFDictionarySetValue(heifDict.get(), kIIOMetadata_CameraExtrinsicsKey, extrinsicsDict.get());

    // Camera intrinsics.
    RetainPtr modelDict = adoptCF(CFDictionaryCreateMutable(nullptr, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

    RetainPtr intrinsicsArray = adoptCF(CFArrayCreateMutable(nullptr, 9, &kCFTypeArrayCallBacks));
    for (auto& value : properties.cameraMetadata.intrinsics.matrix) {
        RetainPtr num = adoptCF(CFNumberCreate(nullptr, kCFNumberFloatType, &value));
        CFArrayAppendValue(intrinsicsArray.get(), num.get());
    }
    CFDictionarySetValue(modelDict.get(), kIIOCameraModel_Intrinsics, intrinsicsArray.get());

    CFDictionarySetValue(heifDict.get(), kIIOMetadata_CameraModelKey, modelDict.get());

    CFDictionarySetValue(result.get(), kCGImagePropertyHEIFDictionary, heifDict.get());

    return result;
}

ShareableSpatialImage::ShareableSpatialImage(ShareableBitmap::Handle&& leftHandle, ShareableBitmap::Handle&& rightHandle, const SpatialImageEyeProperties& leftMetadata, const SpatialImageEyeProperties& rightMetadata)
    : m_leftHandle(WTF::move(leftHandle))
    , m_rightHandle(WTF::move(rightHandle))
    , m_leftMetadata(leftMetadata)
    , m_rightMetadata(rightMetadata)
{
}

std::optional<ShareableSpatialImage> ShareableSpatialImage::create(BitmapImage& image)
{
    if (!image.isSpatial())
        return std::nullopt;

    auto leftIndex = image.spatialLeftEyeFrameIndex();
    auto rightIndex = image.spatialRightEyeFrameIndex();

    if (!leftIndex || !rightIndex || leftIndex == rightIndex)
        return std::nullopt;

    auto leftMetadata = image.spatialEyePropertiesAtIndex(*leftIndex);
    auto rightMetadata = image.spatialEyePropertiesAtIndex(*rightIndex);
    if (!leftMetadata || !rightMetadata)
        return std::nullopt;

    RefPtr leftImage = image.nativeImageAtIndex(*leftIndex);
    RefPtr rightImage = image.nativeImageAtIndex(*rightIndex);
    if (!leftImage || !rightImage)
        return std::nullopt;

    RefPtr leftShareableBitmap = ShareableBitmap::createFromImagePixels(*leftImage);
    RefPtr rightShareableBitmap = ShareableBitmap::createFromImagePixels(*rightImage);
    if (!leftShareableBitmap || !rightShareableBitmap)
        return std::nullopt;

    auto leftHandle = leftShareableBitmap->createReadOnlyHandle();
    auto rightHandle = rightShareableBitmap->createReadOnlyHandle();
    if (!leftHandle || !rightHandle)
        return std::nullopt;

    return ShareableSpatialImage(WTF::move(*leftHandle), WTF::move(*rightHandle), *leftMetadata, *rightMetadata);
}

RetainPtr<CFDataRef> ShareableSpatialImage::createHEICData() const
{
    RefPtr leftBitmap = ShareableBitmap::create(ShareableBitmap::Handle(m_leftHandle), SharedMemory::Protection::ReadOnly);
    RefPtr rightBitmap = ShareableBitmap::create(ShareableBitmap::Handle(m_rightHandle), SharedMemory::Protection::ReadOnly);
    if (!leftBitmap || !rightBitmap)
        return nullptr;

    RetainPtr leftImage = leftBitmap->createPlatformImage();
    RetainPtr rightImage = rightBitmap->createPlatformImage();

    if (!leftImage || !rightImage)
        return nullptr;

    RetainPtr leftDict = spatialImageEyePropertiesToDictionary(m_leftMetadata, kCGImagePropertyGroupImageIsLeftImage);
    RetainPtr rightDict = spatialImageEyePropertiesToDictionary(m_rightMetadata, kCGImagePropertyGroupImageIsRightImage);

    RetainPtr destinationData = adoptCF(CFDataCreateMutable(nullptr, 0));
    RetainPtr destination = adoptCF(CGImageDestinationCreateWithData(destinationData.get(), CFSTR("public.heic"), 2, nullptr));
    CGImageDestinationAddImage(destination.get(), leftImage.get(), leftDict.get());
    CGImageDestinationAddImage(destination.get(), rightImage.get(), rightDict.get());

    if (!CGImageDestinationFinalize(destination.get()))
        return nullptr;

    return destinationData;
}

} // namespace WebCore

#endif // USE(CG)
