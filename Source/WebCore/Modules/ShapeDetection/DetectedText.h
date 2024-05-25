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

#include "DOMRectReadOnly.h"
#include "DetectedTextInterface.h"
#include "Point2D.h"
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

struct Point2D;

struct DetectedText {
    ShapeDetection::DetectedText convertToBacking() const
    {
        return {
            {
                static_cast<float>(boundingBox->x()),
                static_cast<float>(boundingBox->y()),
                static_cast<float>(boundingBox->width()),
                static_cast<float>(boundingBox->height()),
            },
            rawValue,
            cornerPoints.map([] (const auto& cornerPoint) {
                return cornerPoint.convertToBacking();
            }),
        };
    }

    Ref<DOMRectReadOnly> boundingBox;
    String rawValue;
    Vector<Point2D> cornerPoints;
};

inline DetectedText convertFromBacking(const ShapeDetection::DetectedText& detectedText)
{
    return {
        DOMRectReadOnly::create(detectedText.boundingBox.x(), detectedText.boundingBox.y(), detectedText.boundingBox.width(), detectedText.boundingBox.height()),
        detectedText.rawValue,
        detectedText.cornerPoints.map([] (const auto& cornerPoint) {
            return Point2D { cornerPoint.x(), cornerPoint.y() };
        }),
    };
}

} // namespace WebCore
