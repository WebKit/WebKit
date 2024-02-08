/*
 * Copyright (C) 2011-2023 Apple Inc. All rights reserved.
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
#include "WebCoreArgumentCoders.h"

#include "StreamConnectionEncoder.h"
#include <JavaScriptCore/GenericTypedArrayViewInlines.h>
#include <JavaScriptCore/JSGenericTypedArrayViewInlines.h>
#include <WebCore/ARKitBadgeSystemImage.h>
#include <WebCore/ApplePayLogoSystemImage.h>
#include <WebCore/AuthenticationChallenge.h>
#include <WebCore/AuthenticationChallenge.h>
#include <WebCore/BlobPart.h>
#include <WebCore/ButtonPart.h>
#include <WebCore/ByteArrayPixelBuffer.h>
#include <WebCore/COEPInheritenceViolationReportBody.h>
#include <WebCore/CORPViolationReportBody.h>
#include <WebCore/CSPViolationReportBody.h>
#include <WebCore/CSSFilter.h>
#include <WebCore/CacheQueryOptions.h>
#include <WebCore/CacheStorageConnection.h>
#include <WebCore/ColorWellPart.h>
#include <WebCore/CompositionUnderline.h>
#include <WebCore/Credential.h>
#include <WebCore/Cursor.h>
#include <WebCore/DOMCacheEngine.h>
#include <WebCore/DatabaseDetails.h>
#include <WebCore/DecomposedGlyphs.h>
#include <WebCore/DeprecationReportBody.h>
#include <WebCore/DiagnosticLoggingClient.h>
#include <WebCore/DictationAlternative.h>
#include <WebCore/DictionaryPopupInfo.h>
#include <WebCore/DisplayListItems.h>
#include <WebCore/DistantLightSource.h>
#include <WebCore/DragData.h>
#include <WebCore/EventTrackingRegions.h>
#include <WebCore/FEBlend.h>
#include <WebCore/FEColorMatrix.h>
#include <WebCore/FEComponentTransfer.h>
#include <WebCore/FEComposite.h>
#include <WebCore/FEConvolveMatrix.h>
#include <WebCore/FEDiffuseLighting.h>
#include <WebCore/FEDisplacementMap.h>
#include <WebCore/FEDropShadow.h>
#include <WebCore/FEFlood.h>
#include <WebCore/FEGaussianBlur.h>
#include <WebCore/FEImage.h>
#include <WebCore/FEMerge.h>
#include <WebCore/FEMorphology.h>
#include <WebCore/FEOffset.h>
#include <WebCore/FESpecularLighting.h>
#include <WebCore/FETile.h>
#include <WebCore/FETurbulence.h>
#include <WebCore/FetchOptions.h>
#include <WebCore/File.h>
#include <WebCore/FileChooser.h>
#include <WebCore/Filter.h>
#include <WebCore/FilterEffect.h>
#include <WebCore/FilterFunction.h>
#include <WebCore/FilterOperation.h>
#include <WebCore/FilterOperations.h>
#include <WebCore/FloatQuad.h>
#include <WebCore/Font.h>
#include <WebCore/FontCustomPlatformData.h>
#include <WebCore/GraphicsContext.h>
#include <WebCore/GraphicsLayer.h>
#include <WebCore/IDBGetResult.h>
#include <WebCore/IdentityTransformOperation.h>
#include <WebCore/Image.h>
#include <WebCore/ImageControlsButtonPart.h>
#include <WebCore/InnerSpinButtonPart.h>
#include <WebCore/JSDOMExceptionHandling.h>
#include <WebCore/Length.h>
#include <WebCore/LengthBox.h>
#include <WebCore/LightSource.h>
#include <WebCore/Matrix3DTransformOperation.h>
#include <WebCore/MatrixTransformOperation.h>
#include <WebCore/MediaSelectionOption.h>
#include <WebCore/MenuListButtonPart.h>
#include <WebCore/MenuListPart.h>
#include <WebCore/MeterPart.h>
#include <WebCore/NotificationResources.h>
#include <WebCore/Pasteboard.h>
#include <WebCore/Path.h>
#include <WebCore/PerspectiveTransformOperation.h>
#include <WebCore/PluginData.h>
#include <WebCore/PointLightSource.h>
#include <WebCore/ProgressBarPart.h>
#include <WebCore/PromisedAttachmentInfo.h>
#include <WebCore/RectEdges.h>
#include <WebCore/Region.h>
#include <WebCore/RegistrableDomain.h>
#include <WebCore/Report.h>
#include <WebCore/ReportBody.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/ResourceResponse.h>
#include <WebCore/RotateTransformOperation.h>
#include <WebCore/SVGFilter.h>
#include <WebCore/ScaleTransformOperation.h>
#include <WebCore/ScriptBuffer.h>
#include <WebCore/ScriptExecutionContextIdentifier.h>
#include <WebCore/ScrollingConstraints.h>
#include <WebCore/ScrollingCoordinator.h>
#include <WebCore/SearchFieldCancelButtonPart.h>
#include <WebCore/SearchFieldPart.h>
#include <WebCore/SearchFieldResultsPart.h>
#include <WebCore/SearchPopupMenu.h>
#include <WebCore/SecurityOrigin.h>
#include <WebCore/SerializedPlatformDataCueValue.h>
#include <WebCore/SerializedScriptValue.h>
#include <WebCore/ShareData.h>
#include <WebCore/ShareableBitmap.h>
#include <WebCore/ShareableResource.h>
#include <WebCore/SharedBuffer.h>
#include <WebCore/SkewTransformOperation.h>
#include <WebCore/SliderThumbPart.h>
#include <WebCore/SliderTrackPart.h>
#include <WebCore/SourceAlpha.h>
#include <WebCore/SourceGraphic.h>
#include <WebCore/SpotLightSource.h>
#include <WebCore/SwitchThumbPart.h>
#include <WebCore/SwitchTrackPart.h>
#include <WebCore/SystemImage.h>
#include <WebCore/TestReportBody.h>
#include <WebCore/TextAreaPart.h>
#include <WebCore/TextCheckerClient.h>
#include <WebCore/TextFieldPart.h>
#include <WebCore/TextIndicator.h>
#include <WebCore/ToggleButtonPart.h>
#include <WebCore/TransformOperation.h>
#include <WebCore/TransformationMatrix.h>
#include <WebCore/TranslateTransformOperation.h>
#include <WebCore/UserStyleSheet.h>
#include <WebCore/VelocityData.h>
#include <WebCore/ViewportArguments.h>
#include <WebCore/WindowFeatures.h>
#include <wtf/URL.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringHash.h>

#if PLATFORM(COCOA)
#include "ArgumentCodersCF.h"
#endif

#if PLATFORM(IOS_FAMILY)
#include <WebCore/SelectionGeometry.h>
#endif // PLATFORM(IOS_FAMILY)

#if ENABLE(MEDIA_STREAM)
#include <WebCore/CaptureDevice.h>
#include <WebCore/MediaConstraints.h>
#endif

#if ENABLE(IMAGE_ANALYSIS)
#include <WebCore/TextRecognitionResult.h>
#endif

#if ENABLE(APPLE_PAY)
#include <WebCore/ApplePayButtonPart.h>
#endif

#if USE(APPKIT)
#include <WebCore/AppKitControlSystemImage.h>
#endif

// FIXME: Seems like we could use std::tuple to cut down the code below a lot!

namespace IPC {
using namespace WebCore;
using namespace WebKit;

#if !USE(CORE_TEXT)
void ArgumentCoder<WebCore::Font>::encode(Encoder& encoder, const WebCore::Font& font)
{
    encoder << font.attributes();
    // Intentionally don't encode m_isBrokenIdeographFallback because it doesn't affect drawGlyphs().

    encodePlatformData(encoder, font);
}

std::optional<Ref<Font>> ArgumentCoder<Font>::decode(Decoder& decoder)
{
    std::optional<Font::Attributes> attributes;
    decoder >> attributes;
    if (!attributes)
        return std::nullopt;

    auto platformData = decodePlatformData(decoder);
    if (!platformData)
        return std::nullopt;

    return Font::create(*platformData, attributes->origin, attributes->isInterstitial, attributes->visibility, attributes->isTextOrientationFallback, attributes->renderingResourceIdentifier);
}

void ArgumentCoder<WebCore::FontCustomPlatformData>::encode(Encoder& encoder, const WebCore::FontCustomPlatformData& customPlatformData)
{
    std::optional<WebCore::SharedMemory::Handle> handle;
    {
        auto sharedMemoryBuffer = WebCore::SharedMemory::copyBuffer(customPlatformData.creationData.fontFaceData);
        handle = sharedMemoryBuffer->createHandle(WebCore::SharedMemory::Protection::ReadOnly);
    }
    encoder << customPlatformData.creationData.fontFaceData->size();
    encoder << WTFMove(handle);
    encoder << customPlatformData.creationData.itemInCollection;
    encoder << customPlatformData.m_renderingResourceIdentifier;
}

std::optional<Ref<FontCustomPlatformData>> ArgumentCoder<FontCustomPlatformData>::decode(Decoder& decoder)
{
    std::optional<uint64_t> bufferSize;
    decoder >> bufferSize;
    if (!bufferSize)
        return std::nullopt;

    auto handle = decoder.decode<std::optional<WebCore::SharedMemory::Handle>>();
    if (UNLIKELY(!decoder.isValid()))
        return std::nullopt;

    if (!*handle)
        return std::nullopt;

    auto sharedMemoryBuffer = WebCore::SharedMemory::map(WTFMove(**handle), WebCore::SharedMemory::Protection::ReadOnly);
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

    std::optional<RenderingResourceIdentifier> renderingResourceIdentifier;
    decoder >> renderingResourceIdentifier;
    if (!renderingResourceIdentifier)
        return std::nullopt;

    fontCustomPlatformData->m_renderingResourceIdentifier = *renderingResourceIdentifier;

    return fontCustomPlatformData.releaseNonNull();
}

void ArgumentCoder<WebCore::FontPlatformData::Attributes>::encode(Encoder& encoder, const WebCore::FontPlatformData::Attributes& data)
{
    encoder << data.m_orientation;
    encoder << data.m_widthVariant;
    encoder << data.m_textRenderingMode;
    encoder << data.m_size;
    encoder << data.m_syntheticBold;
    encoder << data.m_syntheticOblique;

    encodePlatformData(encoder, data);
}

std::optional<FontPlatformData::Attributes> ArgumentCoder<FontPlatformData::Attributes>::decode(Decoder& decoder)
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

    FontPlatformData::Attributes result(size.value(), orientation.value(), widthVariant.value(), textRenderingMode.value(), syntheticBold.value(), syntheticOblique.value());

    if (!decodePlatformData(decoder, result))
        return std::nullopt;

    return result;
}
#endif

constexpr bool useUnixDomainSockets()
{
#if USE(UNIX_DOMAIN_SOCKETS)
    return true;
#else
    return false;
#endif
}

static constexpr size_t minimumPageSize = 4096;

void ArgumentCoder<WebCore::FragmentedSharedBuffer>::encode(Encoder& encoder, const WebCore::FragmentedSharedBuffer& buffer)
{
    size_t bufferSize = buffer.size();
    encoder << bufferSize;
    if (!bufferSize)
        return;

    if (useUnixDomainSockets() || bufferSize < minimumPageSize) {
        encoder.reserve(encoder.bufferSize() + bufferSize);
        // Do not use shared memory for FragmentedSharedBuffer encoding in Unix, because it's easy to reach the
        // maximum number of file descriptors open per process when sending large data in small chunks
        // over the IPC. ConnectionUnix.cpp already uses shared memory to send any IPC message that is
        // too large. See https://bugs.webkit.org/show_bug.cgi?id=208571.
        for (const auto& element : buffer)
            encoder.encodeSpan(std::span(element.segment->data(), element.segment->size()));
    } else {
        std::optional<SharedMemory::Handle> handle;
        {
            auto sharedMemoryBuffer = SharedMemory::copyBuffer(buffer);
            handle = sharedMemoryBuffer->createHandle(SharedMemory::Protection::ReadOnly);
        }
        encoder << WTFMove(handle);
    }
}

std::optional<Ref<WebCore::FragmentedSharedBuffer>> ArgumentCoder<WebCore::FragmentedSharedBuffer>::decode(Decoder& decoder)
{
    size_t bufferSize = 0;
    if (!decoder.decode(bufferSize))
        return std::nullopt;

    if (!bufferSize)
        return SharedBuffer::create();

    if (useUnixDomainSockets() || bufferSize < minimumPageSize) {
        auto data = decoder.decodeSpan<uint8_t>(bufferSize);
        if (!data.data())
            return std::nullopt;
        return SharedBuffer::create(data);
    }

    auto handle = decoder.decode<std::optional<SharedMemory::Handle>>();
    if (UNLIKELY(!decoder.isValid()))
        return std::nullopt;

    if (!*handle)
        return std::nullopt;

    auto sharedMemoryBuffer = SharedMemory::map(WTFMove(**handle), SharedMemory::Protection::ReadOnly);
    if (!sharedMemoryBuffer)
        return std::nullopt;

    if (sharedMemoryBuffer->size() < bufferSize)
        return std::nullopt;

    return SharedBuffer::create(static_cast<unsigned char*>(sharedMemoryBuffer->data()), bufferSize);
}

} // namespace IPC
