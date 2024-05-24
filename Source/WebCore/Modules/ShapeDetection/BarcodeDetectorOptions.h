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

#include "BarcodeDetectorOptionsInterface.h"
#include "BarcodeFormat.h"
#include <wtf/Vector.h>

namespace WebCore {

struct BarcodeDetectorOptions {
    static std::optional<Vector<ShapeDetection::BarcodeFormat>> convertToBacking(const std::optional<Vector<BarcodeFormat>>& formats)
    {
        if (!formats)
            return std::nullopt;

        return formats->map([](auto format) {
            return WebCore::convertToBacking(format);
        });
    }

    ShapeDetection::BarcodeDetectorOptions convertToBacking() const
    {
        return {
            convertToBacking(formats),
        };
    }

    std::optional<Vector<BarcodeFormat>> formats;
};

inline std::optional<Vector<BarcodeFormat>> convertFromBacking(const std::optional<Vector<ShapeDetection::BarcodeFormat>>& formats)
{
    if (!formats)
        return std::nullopt;

    return formats->map([](auto format) {
            return WebCore::convertFromBacking(format);
    });
}

inline BarcodeDetectorOptions convertFromBacking(const ShapeDetection::BarcodeDetectorOptions& barcodeDetectorOptions)
{
    return {
        convertFromBacking(barcodeDetectorOptions.formats),
    };
}

} // namespace WebCore
