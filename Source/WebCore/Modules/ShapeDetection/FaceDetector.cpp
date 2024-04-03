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

#include "Chrome.h"
#include "DetectedFace.h"
#include "Document.h"
#include "FaceDetectorOptions.h"
#include "ImageBitmap.h"
#include "ImageBitmapOptions.h"
#include "JSDOMPromiseDeferred.h"
#include "JSDetectedFace.h"
#include "Page.h"
#include "ScriptExecutionContext.h"
#include "WorkerGlobalScope.h"

namespace WebCore {

ExceptionOr<Ref<FaceDetector>> FaceDetector::create(ScriptExecutionContext& scriptExecutionContext, const FaceDetectorOptions& faceDetectorOptions)
{
    if (RefPtr document = dynamicDowncast<Document>(scriptExecutionContext)) {
        RefPtr page = document->page();
        if (!page)
            return Exception { ExceptionCode::AbortError };
        auto backing = page->chrome().createFaceDetector(faceDetectorOptions.convertToBacking());
        if (!backing)
            return Exception { ExceptionCode::AbortError };
        return adoptRef(*new FaceDetector(backing.releaseNonNull()));
    }

    if (is<WorkerGlobalScope>(scriptExecutionContext)) {
        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=255380 Make the Shape Detection API work in Workers
        return Exception { ExceptionCode::AbortError };
    }

    return Exception { ExceptionCode::AbortError };
}

FaceDetector::FaceDetector(Ref<ShapeDetection::FaceDetector>&& backing)
    : m_backing(WTFMove(backing))
{
}

FaceDetector::~FaceDetector() = default;

void FaceDetector::detect(ScriptExecutionContext& scriptExecutionContext, ImageBitmap::Source&& source, DetectPromise&& promise)
{
    ImageBitmap::createCompletionHandler(scriptExecutionContext, WTFMove(source), { }, [backing = m_backing.copyRef(), promise = WTFMove(promise)](ExceptionOr<Ref<ImageBitmap>>&& imageBitmap) mutable {
        if (imageBitmap.hasException()) {
            promise.resolve({ });
            return;
        }

        auto imageBuffer = imageBitmap.releaseReturnValue()->takeImageBuffer();
        if (!imageBuffer) {
            promise.resolve({ });
            return;
        }

        backing->detect(imageBuffer.releaseNonNull(), [promise = WTFMove(promise)](Vector<ShapeDetection::DetectedFace>&& detectedFaces) mutable {
            promise.resolve(detectedFaces.map([](const auto& detectedFace) {
                return convertFromBacking(detectedFace);
            }));
        });
    });
}

} // namespace WebCore
