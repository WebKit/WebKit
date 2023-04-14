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

#include "config.h"
#include "FaceDetector.h"

#include "DetectedFace.h"
#include "FaceDetectorOptions.h"
#include "JSDOMPromiseDeferred.h"
#include "JSDetectedFace.h"

#if PLATFORM(COCOA)
// FIXME: Use the GPU Process.
#include "FaceDetectorImplementation.h"
#endif

namespace WebCore {

ExceptionOr<Ref<FaceDetector>> FaceDetector::create(const FaceDetectorOptions& faceDetectorOptions)
{
#if PLATFORM(COCOA)
    // FIXME: Use the GPU Process.
    auto backing = ShapeDetection::FaceDetectorImpl::create(faceDetectorOptions.convertToBacking());
    return adoptRef(*new FaceDetector(WTFMove(backing)));
#else
    UNUSED_PARAM(faceDetectorOptions);
    return Exception { AbortError };
#endif
}

FaceDetector::FaceDetector(Ref<ShapeDetection::FaceDetector>&& backing)
    : m_backing(WTFMove(backing))
{
}

FaceDetector::~FaceDetector() = default;

void FaceDetector::detect(const ImageBitmap::Source&, DetectPromise&& promise)
{
    m_backing->detect([promise = WTFMove(promise)](Vector<ShapeDetection::DetectedFace>&& detectedFaces) mutable {
        promise.resolve(detectedFaces.map([](const auto& detectedFace) {
            return convertFromBacking(detectedFace);
        }));
    });
}

} // namespace WebCore
