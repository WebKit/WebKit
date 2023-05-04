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

#if HAVE(VISION)

#import <wtf/SoftLinking.h>

SOFT_LINK_FRAMEWORK_FOR_SOURCE_WITH_EXPORT(PAL, Vision, PAL_EXPORT)

SOFT_LINK_CLASS_FOR_SOURCE_OPTIONAL_WITH_EXPORT(PAL, Vision, VNDetectBarcodesRequest, PAL_EXPORT)
SOFT_LINK_CLASS_FOR_SOURCE_OPTIONAL(PAL, Vision, VNDetectFaceLandmarksRequest)
SOFT_LINK_CLASS_FOR_SOURCE_OPTIONAL_WITH_EXPORT(PAL, Vision, VNImageRequestHandler, PAL_EXPORT)
SOFT_LINK_CLASS_FOR_SOURCE_OPTIONAL(PAL, Vision, VNRecognizeTextRequest)

SOFT_LINK_CONSTANT_FOR_SOURCE(PAL, Vision, VNBarcodeSymbologyAztec, NSString *)
SOFT_LINK_CONSTANT_FOR_SOURCE(PAL, Vision, VNBarcodeSymbologyCodabar, NSString *)
SOFT_LINK_CONSTANT_FOR_SOURCE(PAL, Vision, VNBarcodeSymbologyCode39, NSString *)
SOFT_LINK_CONSTANT_FOR_SOURCE(PAL, Vision, VNBarcodeSymbologyCode39Checksum, NSString *)
SOFT_LINK_CONSTANT_FOR_SOURCE(PAL, Vision, VNBarcodeSymbologyCode39FullASCII, NSString *)
SOFT_LINK_CONSTANT_FOR_SOURCE(PAL, Vision, VNBarcodeSymbologyCode39FullASCIIChecksum, NSString *)
SOFT_LINK_CONSTANT_FOR_SOURCE(PAL, Vision, VNBarcodeSymbologyCode93, NSString *)
SOFT_LINK_CONSTANT_FOR_SOURCE(PAL, Vision, VNBarcodeSymbologyCode93i, NSString *)
SOFT_LINK_CONSTANT_FOR_SOURCE(PAL, Vision, VNBarcodeSymbologyCode128, NSString *)
SOFT_LINK_CONSTANT_FOR_SOURCE(PAL, Vision, VNBarcodeSymbologyDataMatrix, NSString *)
SOFT_LINK_CONSTANT_FOR_SOURCE(PAL, Vision, VNBarcodeSymbologyEAN8, NSString *)
SOFT_LINK_CONSTANT_FOR_SOURCE(PAL, Vision, VNBarcodeSymbologyEAN13, NSString *)
SOFT_LINK_CONSTANT_FOR_SOURCE(PAL, Vision, VNBarcodeSymbologyGS1DataBar, NSString *)
SOFT_LINK_CONSTANT_FOR_SOURCE(PAL, Vision, VNBarcodeSymbologyGS1DataBarExpanded, NSString *)
SOFT_LINK_CONSTANT_FOR_SOURCE(PAL, Vision, VNBarcodeSymbologyGS1DataBarLimited, NSString *)
SOFT_LINK_CONSTANT_FOR_SOURCE(PAL, Vision, VNBarcodeSymbologyI2of5, NSString *)
SOFT_LINK_CONSTANT_FOR_SOURCE(PAL, Vision, VNBarcodeSymbologyI2of5Checksum, NSString *)
SOFT_LINK_CONSTANT_FOR_SOURCE(PAL, Vision, VNBarcodeSymbologyITF14, NSString *)
SOFT_LINK_CONSTANT_FOR_SOURCE(PAL, Vision, VNBarcodeSymbologyMicroPDF417, NSString *)
SOFT_LINK_CONSTANT_FOR_SOURCE(PAL, Vision, VNBarcodeSymbologyMicroQR, NSString *)
SOFT_LINK_CONSTANT_FOR_SOURCE(PAL, Vision, VNBarcodeSymbologyPDF417, NSString *)
SOFT_LINK_CONSTANT_FOR_SOURCE_WITH_EXPORT(PAL, Vision, VNBarcodeSymbologyQR, NSString *, PAL_EXPORT)
SOFT_LINK_CONSTANT_FOR_SOURCE(PAL, Vision, VNBarcodeSymbologyUPCE, NSString *)

#endif // HAVE(VISION)
