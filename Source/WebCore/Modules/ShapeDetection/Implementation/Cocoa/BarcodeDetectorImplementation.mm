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

#if HAVE(SHAPE_DETECTION_API_IMPLEMENTATION) && HAVE(VISION)

#import "BarcodeDetectorOptionsInterface.h"
#import "BarcodeFormatInterface.h"
#import "DetectedBarcodeInterface.h"
#import "ImageBuffer.h"
#import "NativeImage.h"
#import "VisionUtilities.h"
#import <optional>
#import <wtf/RetainPtr.h>
#import <wtf/TZoneMallocInlines.h>
#import <pal/cocoa/VisionSoftLink.h>

namespace WebCore::ShapeDetection {

static BarcodeFormat convertSymbology(VNBarcodeSymbology symbology)
{
    if ([symbology isEqual:PAL::get_Vision_VNBarcodeSymbologyAztecSingleton()])
        return BarcodeFormat::Aztec;
    if ([symbology isEqual:PAL::get_Vision_VNBarcodeSymbologyCodabarSingleton()])
        return BarcodeFormat::Codabar;
    if ([symbology isEqual:PAL::get_Vision_VNBarcodeSymbologyCode39Singleton()])
        return BarcodeFormat::Code_39;
    if ([symbology isEqual:PAL::get_Vision_VNBarcodeSymbologyCode39ChecksumSingleton()])
        return BarcodeFormat::Code_39;
    if ([symbology isEqual:PAL::get_Vision_VNBarcodeSymbologyCode39FullASCIISingleton()])
        return BarcodeFormat::Code_39;
    if ([symbology isEqual:PAL::get_Vision_VNBarcodeSymbologyCode39FullASCIIChecksumSingleton()])
        return BarcodeFormat::Code_39;
    if ([symbology isEqual:PAL::get_Vision_VNBarcodeSymbologyCode93Singleton()])
        return BarcodeFormat::Code_93;
    if ([symbology isEqual:PAL::get_Vision_VNBarcodeSymbologyCode93iSingleton()])
        return BarcodeFormat::Code_93;
    if ([symbology isEqual:PAL::get_Vision_VNBarcodeSymbologyCode128Singleton()])
        return BarcodeFormat::Code_128;
    if ([symbology isEqual:PAL::get_Vision_VNBarcodeSymbologyDataMatrixSingleton()])
        return BarcodeFormat::Data_matrix;
    if ([symbology isEqual:PAL::get_Vision_VNBarcodeSymbologyEAN8Singleton()])
        return BarcodeFormat::Ean_8;
    if ([symbology isEqual:PAL::get_Vision_VNBarcodeSymbologyEAN13Singleton()])
        return BarcodeFormat::Ean_13;
    if ([symbology isEqual:PAL::get_Vision_VNBarcodeSymbologyGS1DataBarSingleton()])
        return BarcodeFormat::Unknown;
    if ([symbology isEqual:PAL::get_Vision_VNBarcodeSymbologyGS1DataBarExpandedSingleton()])
        return BarcodeFormat::Unknown;
    if ([symbology isEqual:PAL::get_Vision_VNBarcodeSymbologyGS1DataBarLimitedSingleton()])
        return BarcodeFormat::Unknown;
    if ([symbology isEqual:PAL::get_Vision_VNBarcodeSymbologyI2of5Singleton()])
        return BarcodeFormat::Itf;
    if ([symbology isEqual:PAL::get_Vision_VNBarcodeSymbologyI2of5ChecksumSingleton()])
        return BarcodeFormat::Itf;
    if ([symbology isEqual:PAL::get_Vision_VNBarcodeSymbologyITF14Singleton()])
        return BarcodeFormat::Itf;
    if ([symbology isEqual:PAL::get_Vision_VNBarcodeSymbologyMicroPDF417Singleton()])
        return BarcodeFormat::Pdf417;
    if ([symbology isEqual:PAL::get_Vision_VNBarcodeSymbologyMicroQRSingleton()])
        return BarcodeFormat::Qr_code;
    if ([symbology isEqual:PAL::get_Vision_VNBarcodeSymbologyPDF417Singleton()])
        return BarcodeFormat::Pdf417;
    if ([symbology isEqual:PAL::get_Vision_VNBarcodeSymbologyQRSingleton()])
        return BarcodeFormat::Qr_code;
    if ([symbology isEqual:PAL::get_Vision_VNBarcodeSymbologyUPCESingleton()])
        return BarcodeFormat::Upc_e;
    return BarcodeFormat::Unknown;
}

static Vector<VNBarcodeSymbology> convertBarcodeFormat(BarcodeFormat barcodeFormat)
{
    switch (barcodeFormat) {
    case BarcodeFormat::Aztec:
        return { PAL::get_Vision_VNBarcodeSymbologyAztecSingleton() };
    case BarcodeFormat::Code_128:
        return { PAL::get_Vision_VNBarcodeSymbologyCode128Singleton() };
    case BarcodeFormat::Code_39:
        return { PAL::get_Vision_VNBarcodeSymbologyCode39Singleton(), PAL::get_Vision_VNBarcodeSymbologyCode39ChecksumSingleton(), PAL::get_Vision_VNBarcodeSymbologyCode39FullASCIISingleton(), PAL::get_Vision_VNBarcodeSymbologyCode39FullASCIIChecksumSingleton() };
    case BarcodeFormat::Code_93:
        return { PAL::get_Vision_VNBarcodeSymbologyCode93Singleton(), PAL::get_Vision_VNBarcodeSymbologyCode93iSingleton() };
    case BarcodeFormat::Codabar:
        return { PAL::get_Vision_VNBarcodeSymbologyCodabarSingleton() };
    case BarcodeFormat::Data_matrix:
        return { PAL::get_Vision_VNBarcodeSymbologyDataMatrixSingleton() };
    case BarcodeFormat::Ean_13:
        return { PAL::get_Vision_VNBarcodeSymbologyEAN13Singleton() };
    case BarcodeFormat::Ean_8:
        return { PAL::get_Vision_VNBarcodeSymbologyEAN8Singleton() };
    case BarcodeFormat::Itf:
        return { PAL::get_Vision_VNBarcodeSymbologyI2of5Singleton(), PAL::get_Vision_VNBarcodeSymbologyI2of5ChecksumSingleton(), PAL::get_Vision_VNBarcodeSymbologyITF14Singleton() };
    case BarcodeFormat::Pdf417:
        return { PAL::get_Vision_VNBarcodeSymbologyMicroPDF417Singleton(), PAL::get_Vision_VNBarcodeSymbologyPDF417Singleton() };
    case BarcodeFormat::Qr_code:
        return { PAL::get_Vision_VNBarcodeSymbologyMicroQRSingleton(), PAL::get_Vision_VNBarcodeSymbologyQRSingleton() };
    case BarcodeFormat::Unknown:
        return { PAL::get_Vision_VNBarcodeSymbologyGS1DataBarSingleton(), PAL::get_Vision_VNBarcodeSymbologyGS1DataBarExpandedSingleton(), PAL::get_Vision_VNBarcodeSymbologyGS1DataBarLimitedSingleton() };
    case BarcodeFormat::Upc_a:
        return { };
    case BarcodeFormat::Upc_e:
        return { PAL::get_Vision_VNBarcodeSymbologyUPCESingleton() };
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

WTF_MAKE_TZONE_ALLOCATED_IMPL(BarcodeDetectorImpl);

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

    RetainPtr result = adoptNS([PAL::allocVNDetectBarcodesRequestInstance() init]);
    configureRequestToUseCPUOrGPU(result.get());
    return result;
}

void BarcodeDetectorImpl::getSupportedFormats(CompletionHandler<void(Vector<BarcodeFormat>&&)>&& completionHandler)
{
    NSError *error = nil;
    RetainPtr<NSArray<VNBarcodeSymbology>> supportedSymbologies = [request() supportedSymbologiesAndReturnError:&error];

    BarcodeFormatSet barcodeFormatsSet;
    barcodeFormatsSet.reserveInitialCapacity([supportedSymbologies count]);
    for (VNBarcodeSymbology symbology in supportedSymbologies.get())
        barcodeFormatsSet.add(convertSymbology(symbology));

    auto barcodeFormatsVector = copyToVector(barcodeFormatsSet);
    std::ranges::sort(barcodeFormatsVector);

    completionHandler(WTFMove(barcodeFormatsVector));
}

void BarcodeDetectorImpl::detect(const NativeImage& image, CompletionHandler<void(Vector<DetectedBarcode>&&)>&& completionHandler)
{
    auto platformImage = image.platformImage();
    if (!platformImage) {
        completionHandler({ });
        return;
    }

    auto request = ShapeDetection::request();

    if (m_requestedBarcodeFormatSet) {
        NSMutableSet<VNBarcodeSymbology> *requestedSymbologies = [NSMutableSet setWithCapacity:m_requestedBarcodeFormatSet->size()];
        for (auto barcodeFormat : *m_requestedBarcodeFormatSet) {
            for (RetainPtr symbology : convertBarcodeFormat(barcodeFormat))
                [requestedSymbologies addObject:symbology.get()];
        }
        request.get().symbologies = requestedSymbologies.allObjects;
    }

    RetainPtr imageRequestHandler = adoptNS([PAL::allocVNImageRequestHandlerInstance() initWithCGImage:platformImage.get() options:@{ }]);

    NSError *error = nil;
    auto result = [imageRequestHandler performRequests:@[request.get()] error:&error];
    if (!result || error) {
        completionHandler({ });
        return;
    }

    auto imageSize = image.size();
    Vector<DetectedBarcode> results;
    results.reserveInitialCapacity(request.get().results.count);
    for (VNBarcodeObservation *observation in request.get().results) {
        results.append({
            convertRectFromVisionToWeb(imageSize, observation.boundingBox),
            observation.payloadStringValue,
            convertSymbology(observation.symbology),
            convertCornerPoints(imageSize, observation),
        });
    }

    completionHandler(WTFMove(results));
}

} // namespace WebCore::ShapeDetection

#endif // HAVE(SHAPE_DETECTION_API_IMPLEMENTATION) && HAVE(VISION)
