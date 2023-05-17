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

#pragma once

#if HAVE(SHAPE_DETECTION_API_IMPLEMENTATION) && HAVE(VISION)

#import <wtf/Vector.h>

@class VNRectangleObservation;
@class VNRequest;

namespace WebCore {
class FloatPoint;
class FloatRect;
class FloatSize;
}

namespace WebCore::ShapeDetection {

// Vision uses normalized coordinates (0 .. 1 across the image), and
// also uses an increasing-y-goes-up coordinate system.
// The web expects the results are in unnormalized coordinates (0 .. size across the image), and
// an increasing-y-goes-down coordinate system.
// These functions perform the necessary conversions.

FloatRect convertRectFromVisionToWeb(const FloatSize& imageSize, const FloatRect&);
FloatPoint convertPointFromVisionToWeb(const FloatSize& imageSize, const FloatPoint&);
FloatPoint convertPointFromUnnormalizedVisionToWeb(const FloatSize& imageSize, const FloatPoint&);
Vector<FloatPoint> convertCornerPoints(const FloatSize& imageSize, VNRectangleObservation *);

void configureRequestToUseCPUOrGPU(VNRequest *);

} // namespace WebCore::ShapeDetection

#endif // HAVE(SHAPE_DETECTION_API_IMPLEMENTATION) && HAVE(VISION)
