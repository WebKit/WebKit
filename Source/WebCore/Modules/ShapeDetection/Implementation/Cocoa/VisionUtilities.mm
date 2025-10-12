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
#import "VisionUtilities.h"

#if HAVE(SHAPE_DETECTION_API_IMPLEMENTATION) && HAVE(VISION)

#import "FloatPoint.h"
#import "FloatRect.h"
#import "FloatSize.h"
#import <CoreML/CoreML.h>
#import <pal/cocoa/CoreMLSoftLink.h>
#import <pal/cocoa/VisionSoftLink.h>

namespace WebCore::ShapeDetection {

FloatRect convertRectFromVisionToWeb(const FloatSize& imageSize, const FloatRect& rect)
{
    auto x = rect.x() * imageSize.width();
    auto y = rect.y() * imageSize.height();
    auto maxX = rect.maxX() * imageSize.width();
    auto maxY = rect.maxY() * imageSize.height();
    return { x, imageSize.height() - maxY, maxX - x, maxY - y };
}

FloatPoint convertPointFromVisionToWeb(const FloatSize& imageSize, const FloatPoint& point)
{
    return { point.x() * imageSize.width(), imageSize.height() - point.y() * imageSize.height() };
}

FloatPoint convertPointFromUnnormalizedVisionToWeb(const FloatSize& imageSize, const FloatPoint& point)
{
    return { point.x(), imageSize.height() - point.y() };
}

Vector<FloatPoint> convertCornerPoints(const FloatSize& imageSize, VNRectangleObservation *observation)
{
    auto bottomLeft = convertPointFromVisionToWeb(imageSize, observation.bottomLeft);
    auto bottomRight = convertPointFromVisionToWeb(imageSize, observation.bottomRight);
    auto topLeft = convertPointFromVisionToWeb(imageSize, observation.topLeft);
    auto topRight = convertPointFromVisionToWeb(imageSize, observation.topRight);

    // The spec says "in clockwise direction and starting with top-left"
    return { topLeft, topRight, bottomRight, bottomLeft };
}

void configureRequestToUseCPUOrGPU(VNRequest *request)
{
#if USE(VISION_CPU_ONLY_PROPERTY)
    request.usesCPUOnly = YES;
#else
    NSError *error = nil;
    auto *supportedComputeStageDevices = [request supportedComputeStageDevicesAndReturnError:&error];
    if (!supportedComputeStageDevices || error)
        return;

    for (VNComputeStage computeStage in supportedComputeStageDevices) {
        bool set = false;
        for (id<MLComputeDeviceProtocol> device in supportedComputeStageDevices[computeStage]) {
            // FIXME: This is a safer cpp false positive (rdar://160259918).
            SUPPRESS_UNRETAINED_ARG if ([device isKindOfClass:PAL::getMLGPUComputeDeviceClassSingleton()]) {
                [request setComputeDevice:device forComputeStage:computeStage];
                set = true;
                break;
            }
        }
        if (!set) {
            for (id<MLComputeDeviceProtocol> device in supportedComputeStageDevices[computeStage]) {
                // FIXME: This is a safer cpp false positive (rdar://160259918).
                SUPPRESS_UNRETAINED_ARG if ([device isKindOfClass:PAL::getMLGPUComputeDeviceClassSingleton()]) {
                    [request setComputeDevice:device forComputeStage:computeStage];
                    break;
                }
            }
        }
    }
#endif
}

} // namespace WebCore::ShapeDetection

#endif // HAVE(SHAPE_DETECTION_API_IMPLEMENTATION) && HAVE(VISION)
