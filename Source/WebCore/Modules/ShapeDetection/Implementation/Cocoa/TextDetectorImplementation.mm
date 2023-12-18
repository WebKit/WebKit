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
#import "TextDetectorImplementation.h"

#if HAVE(SHAPE_DETECTION_API_IMPLEMENTATION) && HAVE(VISION)

#import "DetectedTextInterface.h"
#import "ImageBuffer.h"
#import "NativeImage.h"
#import "VisionUtilities.h"
#import <pal/cocoa/VisionSoftLink.h>

namespace WebCore::ShapeDetection {

TextDetectorImpl::TextDetectorImpl() = default;

TextDetectorImpl::~TextDetectorImpl() = default;

void TextDetectorImpl::detect(Ref<ImageBuffer>&& imageBuffer, CompletionHandler<void(Vector<DetectedText>&&)>&& completionHandler)
{
    auto nativeImage = imageBuffer->copyNativeImage();
    if (!nativeImage) {
        completionHandler({ });
        return;
    }

    auto platformImage = nativeImage->platformImage();
    if (!platformImage) {
        completionHandler({ });
        return;
    }

    auto request = adoptNS([PAL::allocVNRecognizeTextRequestInstance() init]);
    configureRequestToUseCPUOrGPU(request.get());

    auto imageRequestHandler = adoptNS([PAL::allocVNImageRequestHandlerInstance() initWithCGImage:platformImage.get() options:@{ }]);

    NSError *error = nil;
    auto result = [imageRequestHandler performRequests:@[request.get()] error:&error];
    if (!result || error) {
        completionHandler({ });
        return;
    }

    Vector<DetectedText> results;
    results.reserveInitialCapacity(request.get().results.count);
    for (VNRecognizedTextObservation *observation in request.get().results) {
        results.append({
            convertRectFromVisionToWeb(nativeImage->size(), observation.boundingBox),
            [observation topCandidates:1][0].string,
            convertCornerPoints(nativeImage->size(), observation),
        });
    }

    completionHandler(WTFMove(results));
}

} // namespace WebCore::ShapeDetection

#endif // HAVE(SHAPE_DETECTION_API_IMPLEMENTATION) && HAVE(VISION)
