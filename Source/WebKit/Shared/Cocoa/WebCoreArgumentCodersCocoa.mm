/*
 * Copyright (C) 2016-2021 Apple Inc. All rights reserved.
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
#import "WebCoreArgumentCoders.h"

#import "ArgumentCodersCF.h"
#import "ArgumentCodersCocoa.h"
#import "SharedMemory.h"
#import <CoreText/CoreText.h>
#import <WebCore/AttributedString.h>
#import <WebCore/DataDetectorElementInfo.h>
#import <WebCore/DictionaryPopupInfo.h>
#import <WebCore/Font.h>
#import <WebCore/FontAttributes.h>
#import <WebCore/FontCustomPlatformData.h>
#import <WebCore/ResourceRequest.h>
#import <WebCore/TextRecognitionResult.h>
#import <pal/spi/cf/CoreTextSPI.h>

#if PLATFORM(IOS_FAMILY)
#import <UIKit/UIFont.h>
#endif

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
#import <WebCore/MediaPlaybackTargetContext.h>
#import <objc/runtime.h>
#endif

#if USE(APPLE_INTERNAL_SDK)
#include <WebKitAdditions/WebCoreArgumentCodersCocoaAdditions.mm>
#endif

#if USE(AVFOUNDATION)
#import <wtf/MachSendRight.h>
#endif

#if ENABLE(APPLE_PAY)
#import "DataReference.h"
#import <pal/cocoa/PassKitSoftLink.h>
#endif

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
#import <pal/cocoa/AVFoundationSoftLink.h>
#endif

#if ENABLE(DATA_DETECTION)
#import <pal/cocoa/DataDetectorsCoreSoftLink.h>
#endif

#if USE(AVFOUNDATION)
#import <WebCore/CoreVideoSoftLink.h>
#endif

#import <pal/cocoa/VisionKitCoreSoftLink.h>

namespace IPC {

void ArgumentCoder<WebCore::Font>::encodePlatformData(Encoder& encoder, const WebCore::Font& font)
{
    const auto& platformData = font.platformData();
    encoder << platformData.orientation();
    encoder << platformData.widthVariant();
    encoder << platformData.textRenderingMode();
    encoder << platformData.size();
    encoder << platformData.syntheticBold();
    encoder << platformData.syntheticOblique();

    auto ctFont = platformData.font();
    auto fontDescriptor = adoptCF(CTFontCopyFontDescriptor(ctFont));
    auto attributes = adoptCF(CTFontDescriptorCopyAttributes(fontDescriptor.get()));
    encoder << attributes;

    const auto& creationData = platformData.creationData();
    encoder << static_cast<bool>(creationData);
    if (creationData) {
        std::optional<WebKit::SharedMemory::Handle> handle;
        {
            auto sharedMemoryBuffer = WebKit::SharedMemory::copyBuffer(creationData->fontFaceData);
            handle = sharedMemoryBuffer->createHandle(WebKit::SharedMemory::Protection::ReadOnly);
        }
        encoder << creationData->fontFaceData->size();
        encoder << WTFMove(handle);
        encoder << creationData->itemInCollection;
    } else {
        auto options = CTFontDescriptorGetOptions(fontDescriptor.get());
        encoder << options;
        auto referenceURL = adoptCF(static_cast<CFURLRef>(CTFontCopyAttribute(ctFont, kCTFontReferenceURLAttribute)));
        auto string = CFURLGetString(referenceURL.get());
        encoder << string;
        encoder << adoptCF(CTFontCopyPostScriptName(ctFont)).get();
    }
}

std::optional<WebCore::FontPlatformData> ArgumentCoder<WebCore::Font>::decodePlatformData(Decoder& decoder)
{
    std::optional<WebCore::FontOrientation> orientation;
    decoder >> orientation;
    if (!orientation)
        return std::nullopt;

    std::optional<WebCore::FontWidthVariant> widthVariant;
    decoder >> widthVariant;
    if (!widthVariant)
        return std::nullopt;

    std::optional<WebCore::TextRenderingMode> textRenderingMode;
    decoder >> textRenderingMode;
    if (!textRenderingMode)
        return std::nullopt;

    std::optional<float> size;
    decoder >> size;
    if (!size)
        return std::nullopt;

    std::optional<bool> syntheticBold;
    decoder >> syntheticBold;
    if (!syntheticBold)
        return std::nullopt;

    std::optional<bool> syntheticOblique;
    decoder >> syntheticOblique;
    if (!syntheticOblique)
        return std::nullopt;

    std::optional<RetainPtr<CFDictionaryRef>> attributes;
    decoder >> attributes;
    if (!attributes)
        return std::nullopt;

    std::optional<bool> includesCreationData;
    decoder >> includesCreationData;
    if (!includesCreationData)
        return std::nullopt;

    if (*includesCreationData) {
        std::optional<uint64_t> bufferSize;
        decoder >> bufferSize;
        if (!bufferSize)
            return std::nullopt;

        auto handle = decoder.decode<std::optional<WebKit::SharedMemory::Handle>>();
        if (UNLIKELY(!decoder.isValid()))
            return std::nullopt;

        if (!*handle)
            return std::nullopt;

        auto sharedMemoryBuffer = WebKit::SharedMemory::map(WTFMove(**handle), WebKit::SharedMemory::Protection::ReadOnly);
        if (!sharedMemoryBuffer)
            return std::nullopt;

        if (sharedMemoryBuffer->size() < *bufferSize)
            return std::nullopt;

        auto fontFaceData = sharedMemoryBuffer->createSharedBuffer(*bufferSize);

        std::optional<String> itemInCollection;
        decoder >> itemInCollection;
        if (!itemInCollection)
            return std::nullopt;

        auto fontCustomPlatformData = createFontCustomPlatformData(fontFaceData, *itemInCollection);
        if (!fontCustomPlatformData)
            return std::nullopt;
        auto baseFontDescriptor = fontCustomPlatformData->fontDescriptor.get();
        if (!baseFontDescriptor)
            return std::nullopt;
        auto fontDescriptor = adoptCF(CTFontDescriptorCreateCopyWithAttributes(baseFontDescriptor, attributes->get()));
        auto ctFont = adoptCF(CTFontCreateWithFontDescriptor(fontDescriptor.get(), *size, nullptr));

        return WebCore::FontPlatformData(ctFont.get(), *size, *syntheticBold, *syntheticOblique, *orientation, *widthVariant, *textRenderingMode, fontCustomPlatformData.get());
    }

    std::optional<CTFontDescriptorOptions> options;
    decoder >> options;
    if (!options)
        return std::nullopt;

    std::optional<RetainPtr<CFStringRef>> referenceURL;
    decoder >> referenceURL;
    if (!referenceURL || !*referenceURL)
        return std::nullopt;

    std::optional<RetainPtr<CFStringRef>> postScriptName;
    decoder >> postScriptName;
    if (!postScriptName || !*postScriptName)
        return std::nullopt;

    auto ctFont = WebCore::createCTFont(attributes->get(), *size, *options, referenceURL->get(), postScriptName->get());
    if (!ctFont)
        return std::nullopt;

    return WebCore::FontPlatformData(ctFont.get(), *size, *syntheticBold, *syntheticOblique, *orientation, *widthVariant, *textRenderingMode);
}

void ArgumentCoder<WebCore::FontPlatformData::Attributes>::encodePlatformData(Encoder& encoder, const WebCore::FontPlatformData::Attributes& data)
{
    encoder << data.m_attributes;
    encoder << data.m_options;
    encoder << data.m_url;
    encoder << data.m_psName;
}

bool ArgumentCoder<WebCore::FontPlatformData::Attributes>::decodePlatformData(Decoder& decoder, WebCore::FontPlatformData::Attributes& data)
{
    std::optional<RetainPtr<CFDictionaryRef>> attributes;
    decoder >> attributes;
    if (!attributes)
        return false;

    std::optional<CTFontDescriptorOptions> options;
    decoder >> options;
    if (!options)
        return false;


    std::optional<RetainPtr<CFStringRef>> url;
    decoder >> url;
    if (!url)
        return false;

    std::optional<RetainPtr<CFStringRef>> psName;
    decoder >> psName;
    if (!psName)
        return false;

    data.m_attributes = attributes.value();
    data.m_options = options.value();
    data.m_url = url.value();
    data.m_psName = psName.value();
    return true;
}

#if ENABLE(DATA_DETECTION)

void ArgumentCoder<WebCore::DataDetectorElementInfo>::encode(Encoder& encoder, const WebCore::DataDetectorElementInfo& info)
{
    encoder << info.result.get();
    encoder << info.elementBounds;
}

std::optional<WebCore::DataDetectorElementInfo> ArgumentCoder<WebCore::DataDetectorElementInfo>::decode(Decoder& decoder)
{
    auto result = decoder.decodeWithAllowedClasses<DDScannerResult>();
    if (!result)
        return std::nullopt;

    std::optional<WebCore::IntRect> elementBounds;
    decoder >> elementBounds;
    if (!elementBounds)
        return std::nullopt;

    return std::make_optional<WebCore::DataDetectorElementInfo>({ WTFMove(*result), WTFMove(*elementBounds) });
}

#endif // ENABLE(DATA_DETECTION)

#if ENABLE(WIRELESS_PLAYBACK_TARGET)

void ArgumentCoder<WebCore::MediaPlaybackTargetContext>::encodePlatformData(Encoder& encoder, const WebCore::MediaPlaybackTargetContext& target)
{
    if (target.type() == WebCore::MediaPlaybackTargetContext::Type::AVOutputContext) {
        if ([PAL::getAVOutputContextClass() conformsToProtocol:@protocol(NSSecureCoding)])
            encoder << target.outputContext();
    } else if (target.type() == WebCore::MediaPlaybackTargetContext::Type::SerializedAVOutputContext) {
        encoder << target.serializedOutputContext();
        encoder << target.hasActiveRoute();
    } else
        ASSERT_NOT_REACHED();
}

bool ArgumentCoder<WebCore::MediaPlaybackTargetContext>::decodePlatformData(Decoder& decoder, WebCore::MediaPlaybackTargetContext::Type contextType, WebCore::MediaPlaybackTargetContext& target)
{
    ASSERT(contextType != WebCore::MediaPlaybackTargetContext::Type::Mock);

    if (contextType == WebCore::MediaPlaybackTargetContext::Type::AVOutputContext) {
        if (![PAL::getAVOutputContextClass() conformsToProtocol:@protocol(NSSecureCoding)])
            return false;

        auto outputContext = decoder.decodeWithAllowedClasses<AVOutputContext>();
        if (!outputContext)
            return false;

        target = WebCore::MediaPlaybackTargetContext { WTFMove(*outputContext) };
        return true;
    }

    if (contextType == WebCore::MediaPlaybackTargetContext::Type::SerializedAVOutputContext) {
        std::optional<RetainPtr<NSData>> serializedOutputContext = decoder.decode<RetainPtr<NSData>>();
        if (!serializedOutputContext)
            return false;

        bool hasActiveRoute;
        if (!decoder.decode(hasActiveRoute))
            return false;

        target = WebCore::MediaPlaybackTargetContext { WTFMove(*serializedOutputContext), hasActiveRoute };
        return true;
    }

    return false;
}
#endif


#if ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)

template<> Class getClass<VKCImageAnalysis>()
{
    return PAL::getVKCImageAnalysisClass();
}

void ArgumentCoder<RetainPtr<VKCImageAnalysis>>::encode(Encoder& encoder, const RetainPtr<VKCImageAnalysis>& data)
{
    if (!PAL::isVisionKitCoreFrameworkAvailable())
        return;

    encoder << data.get();
}

std::optional<RetainPtr<VKCImageAnalysis>> ArgumentCoder<RetainPtr<VKCImageAnalysis>>::decode(Decoder& decoder)
{
    if (!PAL::isVisionKitCoreFrameworkAvailable())
        return nil;

    return decoder.decodeWithAllowedClasses<VKCImageAnalysis>();
}

#endif // ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)

#if USE(AVFOUNDATION)

void ArgumentCoder<RetainPtr<CVPixelBufferRef>>::encode(Encoder& encoder, const RetainPtr<CVPixelBufferRef>& pixelBuffer)
{
    // Use IOSurface as the means to transfer CVPixelBufferRef.
    MachSendRight sendRight;
    if (pixelBuffer) {
        if (auto surface = WebCore::CVPixelBufferGetIOSurface(pixelBuffer.get()))
            sendRight = MachSendRight::adopt(IOSurfaceCreateMachPort(surface));
    }
    encoder << WTFMove(sendRight);
}

std::optional<RetainPtr<CVPixelBufferRef>> ArgumentCoder<RetainPtr<CVPixelBufferRef>>::decode(Decoder& decoder)
{
    std::optional<MachSendRight> sendRight;
    decoder >> sendRight;
    if (!sendRight)
        return std::nullopt;
    RetainPtr<CVPixelBufferRef> pixelBuffer;
    if (!*sendRight)
        return pixelBuffer;
    {
        auto surface = adoptCF(IOSurfaceLookupFromMachPort(sendRight->sendRight()));
        if (!surface)
            return std::nullopt;
        CVPixelBufferRef rawBuffer = nullptr;
        auto status = WebCore::CVPixelBufferCreateWithIOSurface(kCFAllocatorDefault, surface.get(), nullptr, &rawBuffer);
        if (status != noErr || !rawBuffer)
            return std::nullopt;
        pixelBuffer = adoptCF(rawBuffer);
    }
    return pixelBuffer;
}

#endif

} // namespace IPC
