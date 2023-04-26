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
#import "BarcodeDetectorImplementation.h"

#if HAVE(SHAPE_DETECTION_API_IMPLEMENTATION)

#import "BarcodeDetectorOptionsInterface.h"
#import "BarcodeFormatInterface.h"
#import "DetectedBarcodeInterface.h"
#import "ImageBuffer.h"
#import "NativeImage.h"
#import "VisionUtilities.h"
#import <Vision/Vision.h>
#import <optional>
#import <wtf/RetainPtr.h>

namespace WebCore::ShapeDetection {

static BarcodeFormat convertSymbology(VNBarcodeSymbology symbology)
{
    if ([symbology isEqual:VNBarcodeSymbologyAztec])
        return BarcodeFormat::Aztec;
    if ([symbology isEqual:VNBarcodeSymbologyCodabar])
        return BarcodeFormat::Codabar;
    if ([symbology isEqual:VNBarcodeSymbologyCode39])
        return BarcodeFormat::Code_39;
    if ([symbology isEqual:VNBarcodeSymbologyCode39Checksum])
        return BarcodeFormat::Code_39;
    if ([symbology isEqual:VNBarcodeSymbologyCode39FullASCII])
        return BarcodeFormat::Code_39;
    if ([symbology isEqual:VNBarcodeSymbologyCode39FullASCIIChecksum])
        return BarcodeFormat::Code_39;
    if ([symbology isEqual:VNBarcodeSymbologyCode93])
        return BarcodeFormat::Code_93;
    if ([symbology isEqual:VNBarcodeSymbologyCode93i])
        return BarcodeFormat::Code_93;
    if ([symbology isEqual:VNBarcodeSymbologyCode128])
        return BarcodeFormat::Code_128;
    if ([symbology isEqual:VNBarcodeSymbologyDataMatrix])
        return BarcodeFormat::Data_matrix;
    if ([symbology isEqual:VNBarcodeSymbologyEAN8])
        return BarcodeFormat::Ean_8;
    if ([symbology isEqual:VNBarcodeSymbologyEAN13])
        return BarcodeFormat::Ean_13;
    if ([symbology isEqual:VNBarcodeSymbologyGS1DataBar])
        return BarcodeFormat::Unknown;
    if ([symbology isEqual:VNBarcodeSymbologyGS1DataBarExpanded])
        return BarcodeFormat::Unknown;
    if ([symbology isEqual:VNBarcodeSymbologyGS1DataBarLimited])
        return BarcodeFormat::Unknown;
    if ([symbology isEqual:VNBarcodeSymbologyI2of5])
        return BarcodeFormat::Itf;
    if ([symbology isEqual:VNBarcodeSymbologyI2of5Checksum])
        return BarcodeFormat::Itf;
    if ([symbology isEqual:VNBarcodeSymbologyITF14])
        return BarcodeFormat::Itf;
    if ([symbology isEqual:VNBarcodeSymbologyMicroPDF417])
        return BarcodeFormat::Pdf417;
    if ([symbology isEqual:VNBarcodeSymbologyMicroQR])
        return BarcodeFormat::Qr_code;
    if ([symbology isEqual:VNBarcodeSymbologyPDF417])
        return BarcodeFormat::Pdf417;
    if ([symbology isEqual:VNBarcodeSymbologyQR])
        return BarcodeFormat::Qr_code;
    if ([symbology isEqual:VNBarcodeSymbologyUPCE])
        return BarcodeFormat::Upc_e;
    return BarcodeFormat::Unknown;
}

static Vector<VNBarcodeSymbology> convertBarcodeFormat(BarcodeFormat barcodeFormat)
{
    switch (barcodeFormat) {
    case BarcodeFormat::Aztec:
        return { VNBarcodeSymbologyAztec };
    case BarcodeFormat::Code_128:
        return { VNBarcodeSymbologyCode128 };
    case BarcodeFormat::Code_39:
        return { VNBarcodeSymbologyCode39, VNBarcodeSymbologyCode39Checksum, VNBarcodeSymbologyCode39FullASCII, VNBarcodeSymbologyCode39FullASCIIChecksum };
    case BarcodeFormat::Code_93:
        return { VNBarcodeSymbologyCode93, VNBarcodeSymbologyCode93i };
    case BarcodeFormat::Codabar:
        return { VNBarcodeSymbologyCodabar };
    case BarcodeFormat::Data_matrix:
        return { VNBarcodeSymbologyDataMatrix };
    case BarcodeFormat::Ean_13:
        return { VNBarcodeSymbologyEAN13 };
    case BarcodeFormat::Ean_8:
        return { VNBarcodeSymbologyEAN8 };
    case BarcodeFormat::Itf:
        return { VNBarcodeSymbologyI2of5, VNBarcodeSymbologyI2of5Checksum, VNBarcodeSymbologyITF14 };
    case BarcodeFormat::Pdf417:
        return { VNBarcodeSymbologyMicroPDF417, VNBarcodeSymbologyPDF417 };
    case BarcodeFormat::Qr_code:
        return { VNBarcodeSymbologyMicroQR, VNBarcodeSymbologyQR };
    case BarcodeFormat::Unknown:
        return { VNBarcodeSymbologyGS1DataBar, VNBarcodeSymbologyGS1DataBarExpanded, VNBarcodeSymbologyGS1DataBarLimited };
    case BarcodeFormat::Upc_a:
        return { };
    case BarcodeFormat::Upc_e:
        return { VNBarcodeSymbologyUPCE };
    }
}

static std::optional<BarcodeDetectorImpl::BarcodeFormatSet> convertRequestedBarcodeFormatSet(const Vector<BarcodeFormat>& formats)
{
    // FIXME: "formats" is supposed to be optional

    BarcodeDetectorImpl::BarcodeFormatSet result;
    result.reserveInitialCapacity(formats.size());
    for (auto format : formats)
        result.add(format);
    return result;
}

BarcodeDetectorImpl::BarcodeDetectorImpl(const BarcodeDetectorOptions& barcodeDetectorOptions)
    : m_requestedBarcodeFormatSet(convertRequestedBarcodeFormatSet(barcodeDetectorOptions.formats))
{
}

BarcodeDetectorImpl::~BarcodeDetectorImpl() = default;

static RetainPtr<VNDetectBarcodesRequest> request()
{
    // It's important that both getSupportedFormats() and detect() use a VNDetectBarcodesRequest that is
    // configured the same way. This function is intended to make sure both VNDetectBarcodesRequests are
    // configured accordingly.

    auto result = adoptNS([VNDetectBarcodesRequest new]);
    configureRequestToUseCPUOrGPU(result.get());
    return result;
}

void BarcodeDetectorImpl::getSupportedFormats(CompletionHandler<void(Vector<BarcodeFormat>&&)>&& completionHandler)
{
    NSError *error = nil;
    NSArray<VNBarcodeSymbology> *supportedSymbologies = [request() supportedSymbologiesAndReturnError:&error];

    BarcodeFormatSet barcodeFormatsSet;
    barcodeFormatsSet.reserveInitialCapacity(supportedSymbologies.count);
    for (VNBarcodeSymbology symbology in supportedSymbologies)
        barcodeFormatsSet.add(convertSymbology(symbology));

    Vector<BarcodeFormat> barcodeFormatsVector;
    barcodeFormatsVector.reserveInitialCapacity(barcodeFormatsSet.size());
    for (auto barcodeFormat : barcodeFormatsSet)
        barcodeFormatsVector.uncheckedAppend(barcodeFormat);

    std::sort(std::begin(barcodeFormatsVector), std::end(barcodeFormatsVector));

    completionHandler(WTFMove(barcodeFormatsVector));
}

void BarcodeDetectorImpl::detect(Ref<ImageBuffer>&& imageBuffer, CompletionHandler<void(Vector<DetectedBarcode>&&)>&& completionHandler)
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

    auto request = ShapeDetection::request();

    if (m_requestedBarcodeFormatSet) {
        NSMutableSet<VNBarcodeSymbology> *requestedSymbologies = [NSMutableSet setWithCapacity:m_requestedBarcodeFormatSet->size()];
        for (auto barcodeFormat : *m_requestedBarcodeFormatSet) {
            for (auto symbology : convertBarcodeFormat(barcodeFormat))
                [requestedSymbologies addObject:symbology];
        }
        request.get().symbologies = requestedSymbologies.allObjects;
    }

    auto imageRequestHandler = adoptNS([[VNImageRequestHandler alloc] initWithCGImage:platformImage.get() options:@{ }]);

    NSError *error = nil;
    auto result = [imageRequestHandler performRequests:@[request.get()] error:&error];
    if (!result || error) {
        completionHandler({ });
        return;
    }

    Vector<DetectedBarcode> results;
    results.reserveInitialCapacity(request.get().results.count);
    for (VNBarcodeObservation *observation in request.get().results) {
        results.uncheckedAppend({
            convertRectFromVisionToWeb(nativeImage->size(), observation.boundingBox),
            observation.payloadStringValue,
            convertSymbology(observation.symbology),
            convertCornerPoints(nativeImage->size(), observation),
        });
    }

    completionHandler(WTFMove(results));
}

} // namespace WebCore::ShapeDetection

#endif // HAVE(SHAPE_DETECTION_API_IMPLEMENTATION)
