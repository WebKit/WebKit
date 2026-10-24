/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#import "config.h"
#import "FaceDetectorImplementation.h"

#if HAVE(SHAPE_DETECTION_API_IMPLEMENTATION) && HAVE(VISION)

#import "DetectedFaceInterface.h"
#import "FaceDetectorOptionsInterface.h"
#import "ImageBuffer.h"
#import "LandmarkInterface.h"
#import "NativeImage.h"
#import "VisionUtilities.h"
#import <wtf/TZoneMallocInlines.h>
#import <pal/cocoa/VisionSoftLink.h>

namespace WebCore::ShapeDetection {

WTF_MAKE_TZONE_ALLOCATED_IMPL(FaceDetectorImpl);

FaceDetectorImpl::FaceDetectorImpl(const FaceDetectorOptions& faceDetectorOptions)
    : m_maxDetectedFaces(faceDetectorOptions.maxDetectedFaces)
{
}

FaceDetectorImpl::~FaceDetectorImpl() = default;

static std::span<const CGPoint> pointsInImageOfSize(VNFaceLandmarkRegion2D *landmark, const FloatSize& imageSize)
{
    return unsafeMakeSpan([landmark pointsInImageOfSize:imageSize], landmark.pointCount);
}

static Vector<FloatPoint> convertLandmark(VNFaceLandmarkRegion2D *landmark, const FloatSize& imageSize)
{
    return WTF::map(pointsInImageOfSize(landmark, imageSize), [&imageSize](const CGPoint& point) {
        return convertPointFromUnnormalizedVisionToWeb(imageSize, point);
    });
}

static Vector<Landmark> convertLandmarks(VNFaceLandmarks2D *landmarks, const FloatSize& imageSize)
{
    return {
        {
            convertLandmark(retainPtr(landmarks.leftEye).get(), imageSize),
            WebCore::ShapeDetection::LandmarkType::Eye,
        }, {
            convertLandmark(retainPtr(landmarks.rightEye).get(), imageSize),
            WebCore::ShapeDetection::LandmarkType::Eye,
        }, {
            convertLandmark(retainPtr(landmarks.nose).get(), imageSize),
            WebCore::ShapeDetection::LandmarkType::Nose,
        },
    };
}

void FaceDetectorImpl::detect(const NativeImage& image, CompletionHandler<void(Vector<DetectedFace>&&)>&& completionHandler)
{
    auto platformImage = image.platformImage();
    if (!platformImage) {
        completionHandler({ });
        return;
    }

    RetainPtr request = adoptNS([PAL::allocVNDetectFaceLandmarksRequestInstance() init]);
    configureRequestToUseCPUOrGPU(request.get());

    RetainPtr imageRequestHandler = adoptNS([PAL::allocVNImageRequestHandlerInstance() initWithCGImage:platformImage.get() options:@{ }]);

    NSError *error = nil;
    auto result = [imageRequestHandler performRequests:@[request.get()] error:&error];
    if (!result || error) {
        completionHandler({ });
        return;
    }

    auto imageSize = image.size();
    Vector<DetectedFace> results;
    results.reserveInitialCapacity(std::min<size_t>(m_maxDetectedFaces, request.get().results.count));
    for (VNFaceObservation *observation in request.get().results) {
        results.append({
            convertRectFromVisionToWeb(imageSize, observation.boundingBox),
            { convertLandmarks(retainPtr(observation.landmarks).get(), imageSize) },
        });
        if (results.size() >= m_maxDetectedFaces)
            break;
    }

    completionHandler(WTF::move(results));
}

} // namespace WebCore::ShapeDetection

#endif // HAVE(SHAPE_DETECTION_API_IMPLEMENTATION) && HAVE(VISION)
