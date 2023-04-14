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

#include "BarcodeFormatInterface.h"
#include <cstdint>

namespace WebCore {

enum class BarcodeFormat : uint8_t {
    Aztec,
    Code_128,
    Code_39,
    Code_93,
    Codabar,
    Data_matrix,
    Ean_13,
    Ean_8,
    Itf,
    Pdf417,
    Qr_code,
    Unknown,
    Upc_a,
    Upc_e,
};

inline ShapeDetection::BarcodeFormat convertToBacking(BarcodeFormat barcodeFormat)
{
    switch (barcodeFormat) {
    case BarcodeFormat::Aztec:
        return ShapeDetection::BarcodeFormat::Aztec;
    case BarcodeFormat::Code_128:
        return ShapeDetection::BarcodeFormat::Code_128;
    case BarcodeFormat::Code_39:
        return ShapeDetection::BarcodeFormat::Code_39;
    case BarcodeFormat::Code_93:
        return ShapeDetection::BarcodeFormat::Code_93;
    case BarcodeFormat::Codabar:
        return ShapeDetection::BarcodeFormat::Codabar;
    case BarcodeFormat::Data_matrix:
        return ShapeDetection::BarcodeFormat::Data_matrix;
    case BarcodeFormat::Ean_13:
        return ShapeDetection::BarcodeFormat::Ean_13;
    case BarcodeFormat::Ean_8:
        return ShapeDetection::BarcodeFormat::Ean_8;
    case BarcodeFormat::Itf:
        return ShapeDetection::BarcodeFormat::Itf;
    case BarcodeFormat::Pdf417:
        return ShapeDetection::BarcodeFormat::Pdf417;
    case BarcodeFormat::Qr_code:
        return ShapeDetection::BarcodeFormat::Qr_code;
    case BarcodeFormat::Unknown:
        return ShapeDetection::BarcodeFormat::Unknown;
    case BarcodeFormat::Upc_a:
        return ShapeDetection::BarcodeFormat::Upc_a;
    case BarcodeFormat::Upc_e:
        return ShapeDetection::BarcodeFormat::Upc_e;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

inline BarcodeFormat convertFromBacking(ShapeDetection::BarcodeFormat barcodeFormat)
{
    switch (barcodeFormat) {
    case ShapeDetection::BarcodeFormat::Aztec:
        return BarcodeFormat::Aztec;
    case ShapeDetection::BarcodeFormat::Code_128:
        return BarcodeFormat::Code_128;
    case ShapeDetection::BarcodeFormat::Code_39:
        return BarcodeFormat::Code_39;
    case ShapeDetection::BarcodeFormat::Code_93:
        return BarcodeFormat::Code_93;
    case ShapeDetection::BarcodeFormat::Codabar:
        return BarcodeFormat::Codabar;
    case ShapeDetection::BarcodeFormat::Data_matrix:
        return BarcodeFormat::Data_matrix;
    case ShapeDetection::BarcodeFormat::Ean_13:
        return BarcodeFormat::Ean_13;
    case ShapeDetection::BarcodeFormat::Ean_8:
        return BarcodeFormat::Ean_8;
    case ShapeDetection::BarcodeFormat::Itf:
        return BarcodeFormat::Itf;
    case ShapeDetection::BarcodeFormat::Pdf417:
        return BarcodeFormat::Pdf417;
    case ShapeDetection::BarcodeFormat::Qr_code:
        return BarcodeFormat::Qr_code;
    case ShapeDetection::BarcodeFormat::Unknown:
        return BarcodeFormat::Unknown;
    case ShapeDetection::BarcodeFormat::Upc_a:
        return BarcodeFormat::Upc_a;
    case ShapeDetection::BarcodeFormat::Upc_e:
        return BarcodeFormat::Upc_e;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

} // namespace WebCore
