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

#include "ShareableBitmap.h"
#include "ShareableResource.h"
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
#include <WebCore/ControlPart.h>
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
#include <WebCore/ResourceError.h>
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
#include <WebCore/SerializedAttachmentData.h>
#include <WebCore/SerializedPlatformDataCueValue.h>
#include <WebCore/SerializedScriptValue.h>
#include <WebCore/ServiceWorkerClientData.h>
#include <WebCore/ServiceWorkerData.h>
#include <WebCore/ShareData.h>
#include <WebCore/SharedBuffer.h>
#include <WebCore/SkewTransformOperation.h>
#include <WebCore/SliderThumbPart.h>
#include <WebCore/SliderTrackPart.h>
#include <WebCore/SourceAlpha.h>
#include <WebCore/SourceGraphic.h>
#include <WebCore/SpotLightSource.h>
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

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
#include <WebCore/MediaPlaybackTargetContext.h>
#endif

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

void ArgumentCoder<DOMCacheEngine::Record>::encode(Encoder& encoder, const DOMCacheEngine::Record& record)
{
    encoder << record.identifier;

    encoder << record.requestHeadersGuard;
    encoder << record.request;
    encoder << record.options;
    encoder << record.referrer;

    encoder << record.responseHeadersGuard;
    encoder << record.response;
    encoder << record.updateResponseCounter;
    encoder << record.responseBodySize;

    WTF::switchOn(record.responseBody, [&](const Ref<SharedBuffer>& buffer) {
        encoder << true;
        encoder << buffer;
    }, [&](const Ref<FormData>& formData) {
        encoder << false;
        encoder << true;
        encoder << formData;
    }, [&](const std::nullptr_t&) {
        encoder << false;
        encoder << false;
    });
}

std::optional<DOMCacheEngine::Record> ArgumentCoder<DOMCacheEngine::Record>::decode(Decoder& decoder)
{
    uint64_t identifier;
    if (!decoder.decode(identifier))
        return std::nullopt;

    FetchHeaders::Guard requestHeadersGuard;
    if (!decoder.decode(requestHeadersGuard))
        return std::nullopt;

    WebCore::ResourceRequest request;
    if (!decoder.decode(request))
        return std::nullopt;

    std::optional<WebCore::FetchOptions> options;
    decoder >> options;
    if (!options)
        return std::nullopt;

    String referrer;
    if (!decoder.decode(referrer))
        return std::nullopt;

    FetchHeaders::Guard responseHeadersGuard;
    if (!decoder.decode(responseHeadersGuard))
        return std::nullopt;

    WebCore::ResourceResponse response;
    if (!decoder.decode(response))
        return std::nullopt;

    uint64_t updateResponseCounter;
    if (!decoder.decode(updateResponseCounter))
        return std::nullopt;

    uint64_t responseBodySize;
    if (!decoder.decode(responseBodySize))
        return std::nullopt;

    WebCore::DOMCacheEngine::ResponseBody responseBody;
    bool hasSharedBufferBody;
    if (!decoder.decode(hasSharedBufferBody))
        return std::nullopt;

    if (hasSharedBufferBody) {
        auto buffer = decoder.decode<Ref<SharedBuffer>>();
        if (!buffer)
            return std::nullopt;
        responseBody = WTFMove(*buffer);
    } else {
        bool hasFormDataBody;
        if (!decoder.decode(hasFormDataBody))
            return std::nullopt;
        if (hasFormDataBody) {
            std::optional<Ref<WebCore::FormData>> formData;
            decoder >> formData;
            if (!formData)
                return std::nullopt;
            responseBody = *formData;
        }
    }

    return {{ WTFMove(identifier), WTFMove(updateResponseCounter), WTFMove(requestHeadersGuard), WTFMove(request), WTFMove(options.value()), WTFMove(referrer), WTFMove(responseHeadersGuard), WTFMove(response), WTFMove(responseBody), responseBodySize }};
}

void ArgumentCoder<RectEdges<bool>>::encode(Encoder& encoder, const RectEdges<bool>& boxEdges)
{
    SimpleArgumentCoder<RectEdges<bool>>::encode(encoder, boxEdges);
}
    
std::optional<RectEdges<bool>> ArgumentCoder<RectEdges<bool>>::decode(Decoder& decoder)
{
    return SimpleArgumentCoder<RectEdges<bool>>::decode(decoder);
}

#if ENABLE(META_VIEWPORT)
void ArgumentCoder<ViewportArguments>::encode(Encoder& encoder, const ViewportArguments& viewportArguments)
{
    SimpleArgumentCoder<ViewportArguments>::encode(encoder, viewportArguments);
}

std::optional<ViewportArguments> ArgumentCoder<ViewportArguments>::decode(Decoder& decoder)
{
    return SimpleArgumentCoder<ViewportArguments>::decode(decoder);
}

#endif // ENABLE(META_VIEWPORT)

void ArgumentCoder<Length>::encode(Encoder& encoder, const Length& length)
{
    encoder << length.type() << length.hasQuirk();

    switch (length.type()) {
    case LengthType::Auto:
    case LengthType::Normal:
    case LengthType::Content:
    case LengthType::Undefined:
        break;
    case LengthType::Fixed:
    case LengthType::Relative:
    case LengthType::Intrinsic:
    case LengthType::MinIntrinsic:
    case LengthType::MinContent:
    case LengthType::MaxContent:
    case LengthType::FillAvailable:
    case LengthType::FitContent:
    case LengthType::Percent:
        encoder << length.isFloat();
        if (length.isFloat())
            encoder << length.value();
        else
            encoder << length.intValue();
        break;
    case LengthType::Calculated:
        ASSERT_NOT_REACHED();
        break;
    }
}

bool ArgumentCoder<Length>::decode(Decoder& decoder, Length& length)
{
    LengthType type;
    if (!decoder.decode(type))
        return false;

    bool hasQuirk;
    if (!decoder.decode(hasQuirk))
        return false;

    switch (type) {
    case LengthType::Normal:
    case LengthType::Auto:
    case LengthType::Content:
    case LengthType::Undefined:
        length = Length(type);
        return true;
    case LengthType::Fixed:
    case LengthType::Relative:
    case LengthType::Intrinsic:
    case LengthType::MinIntrinsic:
    case LengthType::MinContent:
    case LengthType::MaxContent:
    case LengthType::FillAvailable:
    case LengthType::FitContent:
    case LengthType::Percent: {
        bool isFloat;
        if (!decoder.decode(isFloat))
            return false;

        if (isFloat) {
            float value;
            if (!decoder.decode(value))
                return false;

            length = Length(value, type, hasQuirk);
        } else {
            int value;
            if (!decoder.decode(value))
                return false;

            length = Length(value, type, hasQuirk);
        }
        return true;
    }
    case LengthType::Calculated:
        ASSERT_NOT_REACHED();
        return false;
    }

    return false;
}

void ArgumentCoder<Credential>::encode(Encoder& encoder, const Credential& credential)
{
    if (credential.encodingRequiresPlatformData()) {
        encoder << true;
        encodePlatformData(encoder, credential);
        return;
    }

    encoder << false;
    encoder << credential.user() << credential.password();
    encoder << credential.persistence();
}

bool ArgumentCoder<Credential>::decode(Decoder& decoder, Credential& credential)
{
    bool hasPlatformData;
    if (!decoder.decode(hasPlatformData))
        return false;

    if (hasPlatformData)
        return decodePlatformData(decoder, credential);

    String user;
    if (!decoder.decode(user))
        return false;

    String password;
    if (!decoder.decode(password))
        return false;

    CredentialPersistence persistence;
    if (!decoder.decode(persistence))
        return false;
    
    credential = Credential(user, password, persistence);
    return true;
}

void ArgumentCoder<RefPtr<Image>>::encode(Encoder& encoder, const RefPtr<Image>& image)
{
    bool hasImage = !!image;
    encoder << hasImage;

    if (!hasImage)
        return;

    RefPtr<ShareableBitmap> bitmap = ShareableBitmap::create({ IntSize(image->size()) });
    auto graphicsContext = bitmap->createGraphicsContext();
    encoder << !!graphicsContext;
    if (!graphicsContext)
        return;

    graphicsContext->drawImage(*image, IntPoint());
    
    encoder << bitmap;
}

bool ArgumentCoder<RefPtr<Image>>::decode(Decoder& decoder, RefPtr<Image>& image)
{
    bool hasImage;
    if (!decoder.decode(hasImage))
        return false;

    if (!hasImage)
        return true;

    std::optional<bool> didCreateGraphicsContext;
    decoder >> didCreateGraphicsContext;
    if (!didCreateGraphicsContext || !*didCreateGraphicsContext)
        return false;

    std::optional<RefPtr<WebKit::ShareableBitmap>> bitmap;
    decoder >> bitmap;
    if (!bitmap)
        return false;
    
    image = bitmap.value()->createImage();
    if (!image)
        return false;
    return true;
}

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

void ArgumentCoder<WebCore::Font::Attributes>::encode(Encoder& encoder, const WebCore::Font::Attributes& attributes)
{
    encoder << attributes.origin;
    encoder << attributes.isInterstitial;
    encoder << attributes.visibility;
    encoder << attributes.isTextOrientationFallback;
    encoder << attributes.ensureRenderingResourceIdentifier();
}

std::optional<Font::Attributes> ArgumentCoder<Font::Attributes>::decode(Decoder& decoder)
{
    std::optional<Font::Origin> origin;
    decoder >> origin;
    if (!origin)
        return std::nullopt;

    std::optional<Font::Interstitial> isInterstitial;
    decoder >> isInterstitial;
    if (!isInterstitial)
        return std::nullopt;

    std::optional<Font::Visibility> visibility;
    decoder >> visibility;
    if (!visibility)
        return std::nullopt;

    std::optional<Font::OrientationFallback> isTextOrientationFallback;
    decoder >> isTextOrientationFallback;
    if (!isTextOrientationFallback)
        return std::nullopt;

    std::optional<RenderingResourceIdentifier> renderingResourceIdentifier;
    decoder >> renderingResourceIdentifier;
    if (!renderingResourceIdentifier)
        return std::nullopt;

    return std::optional<Font::Attributes>({ renderingResourceIdentifier, origin.value(), isInterstitial.value(), visibility.value(), isTextOrientationFallback.value() });
}

void ArgumentCoder<WebCore::FontCustomPlatformData>::encode(Encoder& encoder, const WebCore::FontCustomPlatformData& customPlatformData)
{
    WebKit::SharedMemory::Handle handle;
    {
        auto sharedMemoryBuffer = WebKit::SharedMemory::copyBuffer(customPlatformData.creationData.fontFaceData);
        if (auto memoryHandle = sharedMemoryBuffer->createHandle(WebKit::SharedMemory::Protection::ReadOnly))
            handle = WTFMove(*memoryHandle);
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

    std::optional<WebKit::SharedMemory::Handle> handle;
    decoder >> handle;
    if (!handle)
        return std::nullopt;

    auto sharedMemoryBuffer = WebKit::SharedMemory::map(WTFMove(*handle), WebKit::SharedMemory::Protection::ReadOnly);
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

void ArgumentCoder<Cursor>::encode(Encoder& encoder, const Cursor& cursor)
{
    encoder << cursor.type();
        
    if (cursor.type() != Cursor::Custom)
        return;

    if (cursor.image()->isNull()) {
        encoder << false; // There is no valid image being encoded.
        return;
    }

    encoder << true;
    encoder << cursor.image();
    encoder << cursor.hotSpot();
#if ENABLE(MOUSE_CURSOR_SCALE)
    encoder << cursor.imageScaleFactor();
#endif
}

bool ArgumentCoder<Cursor>::decode(Decoder& decoder, Cursor& cursor)
{
    Cursor::Type type;
    if (!decoder.decode(type))
        return false;

    if (type > Cursor::Custom)
        return false;

    if (type != Cursor::Custom) {
        const Cursor& cursorReference = Cursor::fromType(type);
        // Calling platformCursor here will eagerly create the platform cursor for the cursor singletons inside WebCore.
        // This will avoid having to re-create the platform cursors over and over.
        (void)cursorReference.platformCursor();

        cursor = cursorReference;
        return true;
    }

    bool isValidImagePresent;
    if (!decoder.decode(isValidImagePresent))
        return false;

    if (!isValidImagePresent) {
        cursor = Cursor(&Image::nullImage(), IntPoint());
        return true;
    }

    RefPtr<Image> image;
    if (!decoder.decode(image))
        return false;

    IntPoint hotSpot;
    if (!decoder.decode(hotSpot))
        return false;

    if (!image->rect().contains(hotSpot))
        return false;

#if ENABLE(MOUSE_CURSOR_SCALE)
    float scale;
    if (!decoder.decode(scale))
        return false;

    cursor = Cursor(image.get(), hotSpot, scale);
#else
    cursor = Cursor(image.get(), hotSpot);
#endif
    return true;
}

void ArgumentCoder<ResourceError>::encode(Encoder& encoder, const ResourceError& resourceError)
{
    encoder << resourceError.type();
    if (resourceError.type() == ResourceError::Type::Null)
        return;
    encodePlatformData(encoder, resourceError);
    encoder << resourceError.isSanitized();
}

bool ArgumentCoder<ResourceError>::decode(Decoder& decoder, ResourceError& resourceError)
{
    ResourceError::Type type;
    if (!decoder.decode(type))
        return false;

    if (type == ResourceError::Type::Null) {
        resourceError = { };
        return true;
    }

    if (!decodePlatformData(decoder, resourceError))
        return false;

    bool isSanitized;
    if (!decoder.decode(isSanitized))
        return false;

    resourceError.setType(type);
    if (isSanitized)
        resourceError.setAsSanitized();

    return true;
}

#if !USE(COORDINATED_GRAPHICS)
void ArgumentCoder<FilterOperation>::encode(Encoder& encoder, const FilterOperation& filter)
{
    encoder << filter.type();

    switch (filter.type()) {
    case FilterOperation::Type::None:
    case FilterOperation::Type::Reference:
        ASSERT_NOT_REACHED();
        return;
    case FilterOperation::Type::Grayscale:
    case FilterOperation::Type::Sepia:
    case FilterOperation::Type::Saturate:
    case FilterOperation::Type::HueRotate:
        encoder << downcast<BasicColorMatrixFilterOperation>(filter).amount();
        return;
    case FilterOperation::Type::Invert:
    case FilterOperation::Type::Opacity:
    case FilterOperation::Type::Brightness:
    case FilterOperation::Type::Contrast:
        encoder << downcast<BasicComponentTransferFilterOperation>(filter).amount();
        return;
    case FilterOperation::Type::AppleInvertLightness:
        ASSERT_NOT_REACHED(); // AppleInvertLightness is only used in -apple-color-filter.
        return;
    case FilterOperation::Type::Blur:
        encoder << downcast<BlurFilterOperation>(filter).stdDeviation();
        return;
    case FilterOperation::Type::DropShadow: {
        const auto& dropShadowFilter = downcast<DropShadowFilterOperation>(filter);
        encoder << dropShadowFilter.location();
        encoder << dropShadowFilter.stdDeviation();
        encoder << dropShadowFilter.color();
        return;
    }
    case FilterOperation::Type::Default:
        encoder << downcast<DefaultFilterOperation>(filter).representedType();
        return;
    case FilterOperation::Type::Passthrough:
        return;
    }

    ASSERT_NOT_REACHED();
}

bool decodeFilterOperation(Decoder& decoder, RefPtr<FilterOperation>& filter)
{
    FilterOperation::Type type;
    if (!decoder.decode(type))
        return false;

    switch (type) {
    case FilterOperation::Type::None:
    case FilterOperation::Type::Reference:
        ASSERT_NOT_REACHED();
        return false;
    case FilterOperation::Type::Grayscale:
    case FilterOperation::Type::Sepia:
    case FilterOperation::Type::Saturate:
    case FilterOperation::Type::HueRotate: {
        double amount;
        if (!decoder.decode(amount))
            return false;
        filter = BasicColorMatrixFilterOperation::create(amount, type);
        return true;
    }
    case FilterOperation::Type::Invert:
    case FilterOperation::Type::Opacity:
    case FilterOperation::Type::Brightness:
    case FilterOperation::Type::Contrast: {
        double amount;
        if (!decoder.decode(amount))
            return false;
        filter = BasicComponentTransferFilterOperation::create(amount, type);
        return true;
    }
    case FilterOperation::Type::AppleInvertLightness:
        ASSERT_NOT_REACHED(); // AppleInvertLightness is only used in -apple-color-filter.
        return false;
    case FilterOperation::Type::Blur: {
        Length stdDeviation;
        if (!decoder.decode(stdDeviation))
            return false;
        filter = BlurFilterOperation::create(stdDeviation);
        return true;
    }
    case FilterOperation::Type::DropShadow: {
        IntPoint location;
        int stdDeviation;
        Color color;
        if (!decoder.decode(location))
            return false;
        if (!decoder.decode(stdDeviation))
            return false;
        if (!decoder.decode(color))
            return false;
        filter = DropShadowFilterOperation::create(location, stdDeviation, color);
        return true;
    }
    case FilterOperation::Type::Default: {
        FilterOperation::Type representedType;
        if (!decoder.decode(representedType))
            return false;
        filter = DefaultFilterOperation::create(representedType);
        return true;
    }
    case FilterOperation::Type::Passthrough:
        filter = PassthroughFilterOperation::create();
        return true;
    }
            
    ASSERT_NOT_REACHED();
    return false;
}


void ArgumentCoder<FilterOperations>::encode(Encoder& encoder, const FilterOperations& filters)
{
    encoder << static_cast<uint64_t>(filters.size());

    for (const auto& filter : filters.operations())
        encoder << *filter;
}

bool ArgumentCoder<FilterOperations>::decode(Decoder& decoder, FilterOperations& filters)
{
    uint64_t filterCount;
    if (!decoder.decode(filterCount))
        return false;

    for (uint64_t i = 0; i < filterCount; ++i) {
        RefPtr<FilterOperation> filter;
        if (!decodeFilterOperation(decoder, filter))
            return false;
        filters.operations().append(WTFMove(filter));
    }

    return true;
}

void ArgumentCoder<RefPtr<WebCore::FilterOperation>>::encode(Encoder& encoder, const RefPtr<WebCore::FilterOperation>& operation)
{
    encoder << !!operation;
    if (operation)
        encoder << *operation;
}

WARN_UNUSED_RETURN bool ArgumentCoder<RefPtr<WebCore::FilterOperation>>::decode(Decoder& decoder, RefPtr<WebCore::FilterOperation>& value)
{
    std::optional<bool> isNull;
    decoder >> isNull;
    if (!isNull)
        return false;
    
    if (!decodeFilterOperation(decoder, value)) {
        value = nullptr;
        return false;
    }
    return true;
}

#endif // !USE(COORDINATED_GRAPHICS)

void ArgumentCoder<BlobPart>::encode(Encoder& encoder, const BlobPart& blobPart)
{
    encoder << blobPart.type();
    switch (blobPart.type()) {
    case BlobPart::Type::Data:
        encoder << blobPart.data();
        return;
    case BlobPart::Type::Blob:
        encoder << blobPart.url();
        return;
    }
    ASSERT_NOT_REACHED();
}

std::optional<BlobPart> ArgumentCoder<BlobPart>::decode(Decoder& decoder)
{
    std::optional<BlobPart::Type> type;
    decoder >> type;
    if (!type)
        return std::nullopt;

    switch (*type) {
    case BlobPart::Type::Data: {
        std::optional<Vector<uint8_t>> data;
        decoder >> data;
        if (!data)
            return std::nullopt;
        return BlobPart(WTFMove(*data));
    }
    case BlobPart::Type::Blob: {
        URL url;
        if (!decoder.decode(url))
            return std::nullopt;
        return BlobPart(url);
    }
    }

    ASSERT_NOT_REACHED();
    return std::nullopt;
}

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
void ArgumentCoder<MediaPlaybackTargetContext>::encode(Encoder& encoder, const MediaPlaybackTargetContext& target)
{
    bool hasPlatformData = target.encodingRequiresPlatformData();
    encoder << hasPlatformData;

    MediaPlaybackTargetContext::Type contextType = target.type();
    encoder << contextType;

    if (target.encodingRequiresPlatformData()) {
        encodePlatformData(encoder, target);
        return;
    }

    ASSERT(contextType == MediaPlaybackTargetContext::Type::Mock);
    encoder << target.deviceName();
    encoder << target.mockState();
}

bool ArgumentCoder<MediaPlaybackTargetContext>::decode(Decoder& decoder, MediaPlaybackTargetContext& target)
{
    bool hasPlatformData;
    if (!decoder.decode(hasPlatformData))
        return false;

    MediaPlaybackTargetContext::Type contextType;
    if (!decoder.decode(contextType))
        return false;

    if (hasPlatformData)
        return decodePlatformData(decoder, contextType, target);

    ASSERT(contextType == MediaPlaybackTargetContext::Type::Mock);
    String deviceName;
    if (!decoder.decode(deviceName))
        return false;

    MediaPlaybackTargetContext::MockState mockState;
    if (!decoder.decode(mockState))
        return false;

    target = MediaPlaybackTargetContext(deviceName, mockState);

    return true;
}
#endif

void ArgumentCoder<RefPtr<WebCore::SerializedScriptValue>>::encode(Encoder& encoder, const RefPtr<WebCore::SerializedScriptValue>& instance)
{
    encoder << !!instance;
    if (instance)
        instance->encode(encoder);
}

std::optional<RefPtr<WebCore::SerializedScriptValue>> ArgumentCoder<RefPtr<WebCore::SerializedScriptValue>>::decode(Decoder& decoder)
{
    std::optional<bool> nonEmpty;
    decoder >> nonEmpty;
    if (!nonEmpty)
        return std::nullopt;

    if (!*nonEmpty)
        return nullptr;

    RefPtr<SerializedScriptValue> scriptValue = SerializedScriptValue::decode(decoder);
    if (!scriptValue)
        return std::nullopt;

    return { scriptValue };
}

#if ENABLE(SERVICE_WORKER)
void ArgumentCoder<ServiceWorkerOrClientData>::encode(Encoder& encoder, const ServiceWorkerOrClientData& data)
{
    bool isServiceWorkerData = std::holds_alternative<ServiceWorkerData>(data);
    encoder << isServiceWorkerData;
    if (isServiceWorkerData)
        encoder << std::get<ServiceWorkerData>(data);
    else
        encoder << std::get<ServiceWorkerClientData>(data);
}

bool ArgumentCoder<ServiceWorkerOrClientData>::decode(Decoder& decoder, ServiceWorkerOrClientData& data)
{
    bool isServiceWorkerData;
    if (!decoder.decode(isServiceWorkerData))
        return false;
    if (isServiceWorkerData) {
        std::optional<ServiceWorkerData> workerData;
        decoder >> workerData;
        if (!workerData)
            return false;

        data = WTFMove(*workerData);
    } else {
        std::optional<ServiceWorkerClientData> clientData;
        decoder >> clientData;
        if (!clientData)
            return false;

        data = WTFMove(*clientData);
    }
    return true;
}

void ArgumentCoder<ServiceWorkerOrClientIdentifier>::encode(Encoder& encoder, const ServiceWorkerOrClientIdentifier& identifier)
{
    bool isServiceWorkerIdentifier = std::holds_alternative<ServiceWorkerIdentifier>(identifier);
    encoder << isServiceWorkerIdentifier;
    if (isServiceWorkerIdentifier)
        encoder << std::get<ServiceWorkerIdentifier>(identifier);
    else
        encoder << std::get<ScriptExecutionContextIdentifier>(identifier);
}

bool ArgumentCoder<ServiceWorkerOrClientIdentifier>::decode(Decoder& decoder, ServiceWorkerOrClientIdentifier& identifier)
{
    bool isServiceWorkerIdentifier;
    if (!decoder.decode(isServiceWorkerIdentifier))
        return false;
    if (isServiceWorkerIdentifier) {
        std::optional<ServiceWorkerIdentifier> workerIdentifier;
        decoder >> workerIdentifier;
        if (!workerIdentifier)
            return false;

        identifier = WTFMove(*workerIdentifier);
    } else {
        std::optional<ScriptExecutionContextIdentifier> clientIdentifier;
        decoder >> clientIdentifier;
        if (!clientIdentifier)
            return false;

        identifier = WTFMove(*clientIdentifier);
    }
    return true;
}

#endif

#if ENABLE(ATTACHMENT_ELEMENT)

void ArgumentCoder<SerializedAttachmentData>::encode(IPC::Encoder& encoder, const WebCore::SerializedAttachmentData& data)
{
    encoder << data.identifier << data.mimeType << data.data;
}

std::optional<SerializedAttachmentData> ArgumentCoder<WebCore::SerializedAttachmentData>::decode(IPC::Decoder& decoder)
{
    auto identifier = decoder.decode<String>();
    auto mimeType = decoder.decode<String>();
    auto data = decoder.decode<Ref<SharedBuffer>>();

    if (UNLIKELY(!decoder.isValid()))
        return std::nullopt;

    return { { WTFMove(*identifier), WTFMove(*mimeType), WTFMove(*data) } };
}

#endif // ENABLE(ATTACHMENT_ELEMENT)

#if ENABLE(VIDEO)
void ArgumentCoder<WebCore::SerializedPlatformDataCueValue>::encode(Encoder& encoder, const SerializedPlatformDataCueValue& value)
{
    bool hasPlatformData = value.encodingRequiresPlatformData();
    encoder << hasPlatformData;

    encoder << value.platformType();
    if (hasPlatformData)
        encodePlatformData(encoder, value);
}

std::optional<SerializedPlatformDataCueValue> ArgumentCoder<WebCore::SerializedPlatformDataCueValue>::decode(IPC::Decoder& decoder)
{
    bool hasPlatformData;
    if (!decoder.decode(hasPlatformData))
        return std::nullopt;

    WebCore::SerializedPlatformDataCueValue::PlatformType type;
    if (!decoder.decode(type))
        return std::nullopt;

    if (hasPlatformData)
        return decodePlatformData(decoder, type);

    return { SerializedPlatformDataCueValue() };

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
        SharedMemory::Handle handle;
        {
            auto sharedMemoryBuffer = SharedMemory::copyBuffer(buffer);
            if (auto memoryHandle = sharedMemoryBuffer->createHandle(SharedMemory::Protection::ReadOnly))
                handle = WTFMove(*memoryHandle);
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

    SharedMemory::Handle handle;
    if (!decoder.decode(handle))
        return std::nullopt;

    auto sharedMemoryBuffer = SharedMemory::map(WTFMove(handle), SharedMemory::Protection::ReadOnly);
    if (!sharedMemoryBuffer)
        return std::nullopt;

    if (sharedMemoryBuffer->size() < bufferSize)
        return std::nullopt;

    return SharedBuffer::create(static_cast<unsigned char*>(sharedMemoryBuffer->data()), bufferSize);
}

void ArgumentCoder<WebCore::SharedBuffer>::encode(Encoder& encoder, const WebCore::SharedBuffer& buffer)
{
    encoder << static_cast<const WebCore::FragmentedSharedBuffer&>(buffer);
}

std::optional<Ref<WebCore::SharedBuffer>> ArgumentCoder<WebCore::SharedBuffer>::decode(Decoder& decoder)
{
    if (auto buffer = decoder.decode<Ref<FragmentedSharedBuffer>>())
        return (*buffer)->makeContiguous();
    return std::nullopt;
}

#if ENABLE(SHAREABLE_RESOURCE) && PLATFORM(COCOA)
static ShareableResource::Handle tryConvertToShareableResourceHandle(const ScriptBuffer& script)
{
    if (!script.containsSingleFileMappedSegment())
        return ShareableResource::Handle { };

    auto& segment = script.buffer()->begin()->segment;
    auto sharedMemory = SharedMemory::wrapMap(const_cast<uint8_t*>(segment->data()), segment->size(), SharedMemory::Protection::ReadOnly);
    if (!sharedMemory)
        return ShareableResource::Handle { };

    auto shareableResource = ShareableResource::create(sharedMemory.releaseNonNull(), 0, segment->size());
    if (!shareableResource)
        return ShareableResource::Handle { };

    ShareableResource::Handle shareableResourceHandle;
    if (auto handle = shareableResource->createHandle())
        return WTFMove(*handle);
    return ShareableResource::Handle { };
}

static std::optional<WebCore::ScriptBuffer> decodeScriptBufferAsShareableResourceHandle(Decoder& decoder)
{
    ShareableResource::Handle handle;
    if (!decoder.decode(handle) || handle.isNull())
        return std::nullopt;
    auto buffer = WTFMove(handle).tryWrapInSharedBuffer();
    if (!buffer)
        return std::nullopt;
    return WebCore::ScriptBuffer { WTFMove(buffer) };
}
#endif

void ArgumentCoder<WebCore::ScriptBuffer>::encode(Encoder& encoder, const WebCore::ScriptBuffer& script)
{
#if ENABLE(SHAREABLE_RESOURCE) && PLATFORM(COCOA)
    auto handle = tryConvertToShareableResourceHandle(script);
    bool isShareableResourceHandle = !handle.isNull();
    encoder << isShareableResourceHandle;
    if (isShareableResourceHandle) {
        encoder << WTFMove(handle);
        return;
    }
#endif
    encoder << RefPtr { script.buffer() };
}

std::optional<WebCore::ScriptBuffer> ArgumentCoder<WebCore::ScriptBuffer>::decode(Decoder& decoder)
{
#if ENABLE(SHAREABLE_RESOURCE) && PLATFORM(COCOA)
    std::optional<bool> isShareableResourceHandle;
    decoder >> isShareableResourceHandle;
    if (!isShareableResourceHandle)
        return std::nullopt;
    if (*isShareableResourceHandle)
        return decodeScriptBufferAsShareableResourceHandle(decoder);
#endif

    if (auto buffer = decoder.decode<RefPtr<FragmentedSharedBuffer>>())
        return WebCore::ScriptBuffer { WTFMove(*buffer) };
    return std::nullopt;
}

template<typename Encoder>
void ArgumentCoder<SystemImage>::encode(Encoder& encoder, const SystemImage& systemImage)
{
    encoder << systemImage.systemImageType();

    switch (systemImage.systemImageType()) {
#if ENABLE(APPLE_PAY)
    case SystemImageType::ApplePayLogo:
        encoder << downcast<ApplePayLogoSystemImage>(systemImage);
        return;
#endif
#if USE(SYSTEM_PREVIEW)
    case SystemImageType::ARKitBadge:
        encoder << downcast<ARKitBadgeSystemImage>(systemImage);
        return;
#endif
#if USE(APPKIT)
    case SystemImageType::AppKitControl:
        encoder << downcast<AppKitControlSystemImage>(systemImage);
        return;
#endif
    }

    ASSERT_NOT_REACHED();
}

template
void ArgumentCoder<SystemImage>::encode<Encoder>(Encoder&, const SystemImage&);
template
void ArgumentCoder<SystemImage>::encode<StreamConnectionEncoder>(StreamConnectionEncoder&, const SystemImage&);

std::optional<Ref<SystemImage>> ArgumentCoder<SystemImage>::decode(Decoder& decoder)
{
    std::optional<SystemImageType> systemImageType;
    decoder >> systemImageType;
    if (!systemImageType)
        return std::nullopt;

    switch (*systemImageType) {
#if ENABLE(APPLE_PAY)
    case SystemImageType::ApplePayLogo: {
        std::optional<Ref<ApplePayLogoSystemImage>> image;
        decoder >> image;
        if (!image)
            return std::nullopt;
        return WTFMove(*image);
    }
#endif
#if USE(SYSTEM_PREVIEW)
    case SystemImageType::ARKitBadge: {
        std::optional<Ref<ARKitBadgeSystemImage>> image;
        decoder >> image;
        if (!image)
            return std::nullopt;
        return WTFMove(*image);
    }
#endif
#if USE(APPKIT)
    case SystemImageType::AppKitControl: {
        std::optional<Ref<AppKitControlSystemImage>> image;
        decoder >> image;
        if (!image)
            return std::nullopt;
        return WTFMove(*image);
    }
#endif
    }

    ASSERT_NOT_REACHED();
    return std::nullopt;
}

template<typename Encoder>
void ArgumentCoder<ControlPart>::encode(Encoder& encoder, const ControlPart& part)
{
    encoder << part.type();

    switch (part.type()) {
    case WebCore::StyleAppearance::None:
    case WebCore::StyleAppearance::Auto:
        break;

    case WebCore::StyleAppearance::Checkbox:
    case WebCore::StyleAppearance::Radio:
    case WebCore::StyleAppearance::PushButton:
    case WebCore::StyleAppearance::SquareButton:
    case WebCore::StyleAppearance::Button:
    case WebCore::StyleAppearance::DefaultButton:
    case WebCore::StyleAppearance::Listbox:
    case WebCore::StyleAppearance::Menulist:
    case WebCore::StyleAppearance::MenulistButton:
        break;

    case WebCore::StyleAppearance::Meter:
        encoder << downcast<WebCore::MeterPart>(part);
        break;

    case WebCore::StyleAppearance::ProgressBar:
        encoder << downcast<WebCore::ProgressBarPart>(part);
        break;

    case WebCore::StyleAppearance::SliderHorizontal:
    case WebCore::StyleAppearance::SliderVertical:
        encoder << downcast<WebCore::SliderTrackPart>(part);
        break;

    case WebCore::StyleAppearance::SearchField:
        break;
            
#if ENABLE(APPLE_PAY)
    case WebCore::StyleAppearance::ApplePayButton:
        encoder << downcast<WebCore::ApplePayButtonPart>(part);
        break;
#endif

#if ENABLE(ATTACHMENT_ELEMENT)
    case WebCore::StyleAppearance::Attachment:
    case WebCore::StyleAppearance::BorderlessAttachment:
#endif
    case WebCore::StyleAppearance::TextArea:
    case WebCore::StyleAppearance::TextField:
    case WebCore::StyleAppearance::CapsLockIndicator:
#if ENABLE(INPUT_TYPE_COLOR)
    case WebCore::StyleAppearance::ColorWell:
#endif
#if ENABLE(SERVICE_CONTROLS)
    case WebCore::StyleAppearance::ImageControlsButton:
#endif
    case WebCore::StyleAppearance::InnerSpinButton:
#if ENABLE(DATALIST_ELEMENT)
    case WebCore::StyleAppearance::ListButton:
#endif
    case WebCore::StyleAppearance::SearchFieldDecoration:
    case WebCore::StyleAppearance::SearchFieldResultsDecoration:
    case WebCore::StyleAppearance::SearchFieldResultsButton:
    case WebCore::StyleAppearance::SearchFieldCancelButton:
    case WebCore::StyleAppearance::SliderThumbHorizontal:
    case WebCore::StyleAppearance::SliderThumbVertical:
        break;
    }
}

template
void ArgumentCoder<ControlPart>::encode<Encoder>(Encoder&, const ControlPart&);
template
void ArgumentCoder<ControlPart>::encode<StreamConnectionEncoder>(StreamConnectionEncoder&, const ControlPart&);

std::optional<Ref<ControlPart>> ArgumentCoder<ControlPart>::decode(Decoder& decoder)
{
    std::optional<WebCore::StyleAppearance> type;
    decoder >> type;
    if (!type)
        return std::nullopt;

    switch (*type) {
    case WebCore::StyleAppearance::None:
    case WebCore::StyleAppearance::Auto:
        break;

    case WebCore::StyleAppearance::Checkbox:
    case WebCore::StyleAppearance::Radio:
        return WebCore::ToggleButtonPart::create(*type);

    case WebCore::StyleAppearance::PushButton:
    case WebCore::StyleAppearance::SquareButton:
    case WebCore::StyleAppearance::Button:
    case WebCore::StyleAppearance::DefaultButton:
        return WebCore::ButtonPart::create(*type);

    case WebCore::StyleAppearance::Menulist:
        return WebCore::MenuListPart::create();

    case WebCore::StyleAppearance::MenulistButton:
        return WebCore::MenuListButtonPart::create();

    case WebCore::StyleAppearance::Meter: {
        std::optional<Ref<WebCore::MeterPart>> meterPart;
        decoder >> meterPart;
        if (meterPart)
            return WTFMove(*meterPart);
        break;
    }

    case WebCore::StyleAppearance::ProgressBar: {
        std::optional<Ref<WebCore::ProgressBarPart>> progressBarPart;
        decoder >> progressBarPart;
        if (progressBarPart)
            return WTFMove(*progressBarPart);
        break;
    }

    case WebCore::StyleAppearance::SliderHorizontal:
    case WebCore::StyleAppearance::SliderVertical: {
        std::optional<Ref<WebCore::SliderTrackPart>> sliderTrackPart;
        decoder >> sliderTrackPart;
        if (sliderTrackPart)
            return WTFMove(*sliderTrackPart);
        break;
    }

    case WebCore::StyleAppearance::SearchField:
        return WebCore::SearchFieldPart::create();

#if ENABLE(APPLE_PAY)
    case WebCore::StyleAppearance::ApplePayButton: {
        std::optional<Ref<WebCore::ApplePayButtonPart>> applePayButtonPart;
        decoder >> applePayButtonPart;
        if (applePayButtonPart)
            return WTFMove(*applePayButtonPart);
        break;
    }
#endif

#if ENABLE(ATTACHMENT_ELEMENT)
    case WebCore::StyleAppearance::Attachment:
    case WebCore::StyleAppearance::BorderlessAttachment:
#endif
        break;

    case WebCore::StyleAppearance::Listbox:
    case WebCore::StyleAppearance::TextArea:
        return WebCore::TextAreaPart::create(*type);

    case WebCore::StyleAppearance::TextField:
        return WebCore::TextFieldPart::create();

    case WebCore::StyleAppearance::CapsLockIndicator:
        break;

#if ENABLE(INPUT_TYPE_COLOR)
    case WebCore::StyleAppearance::ColorWell:
        return WebCore::ColorWellPart::create();
#endif
#if ENABLE(SERVICE_CONTROLS)
    case WebCore::StyleAppearance::ImageControlsButton:
        return WebCore::ImageControlsButtonPart::create();
#endif

    case WebCore::StyleAppearance::InnerSpinButton:
        return WebCore::InnerSpinButtonPart::create();

#if ENABLE(DATALIST_ELEMENT)
    case WebCore::StyleAppearance::ListButton:
        break;
#endif

    case WebCore::StyleAppearance::SearchFieldDecoration:
        break;

    case WebCore::StyleAppearance::SearchFieldResultsDecoration:
    case WebCore::StyleAppearance::SearchFieldResultsButton:
        return WebCore::SearchFieldResultsPart::create(*type);

    case WebCore::StyleAppearance::SearchFieldCancelButton:
        return WebCore::SearchFieldCancelButtonPart::create();

    case WebCore::StyleAppearance::SliderThumbHorizontal:
    case WebCore::StyleAppearance::SliderThumbVertical:
        return WebCore::SliderThumbPart::create(*type);
    }

    ASSERT_NOT_REACHED();
    return std::nullopt;
}

template<typename Encoder>
void ArgumentCoder<LightSource>::encode(Encoder& encoder, const LightSource& lightSource)
{
    encoder << lightSource.type();

    switch (lightSource.type()) {
    case LS_DISTANT:
        encoder << downcast<DistantLightSource>(lightSource);
        return;

    case LS_POINT:
        encoder << downcast<PointLightSource>(lightSource);
        return;

    case LS_SPOT:
        encoder << downcast<SpotLightSource>(lightSource);
        return;
    }

    ASSERT_NOT_REACHED();
}

template
void ArgumentCoder<LightSource>::encode<Encoder>(Encoder&, const LightSource&);
template
void ArgumentCoder<LightSource>::encode<StreamConnectionEncoder>(StreamConnectionEncoder&, const LightSource&);

std::optional<Ref<LightSource>> ArgumentCoder<LightSource>::decode(Decoder& decoder)
{
    std::optional<LightType> lightSourceType;
    decoder >> lightSourceType;
    if (!lightSourceType)
        return std::nullopt;

    switch (*lightSourceType) {
    case LS_DISTANT: {
        std::optional<Ref<DistantLightSource>> distantLightSource;
        decoder >> distantLightSource;
        if (distantLightSource)
            return WTFMove(*distantLightSource);
        break;
    }

    case LS_POINT: {
        std::optional<Ref<PointLightSource>> pointLightSource;
        decoder >> pointLightSource;
        if (pointLightSource)
            return WTFMove(*pointLightSource);
        break;
    }

    case LS_SPOT: {
        std::optional<Ref<SpotLightSource>> spotLightSource;
        decoder >> spotLightSource;
        if (spotLightSource)
            return WTFMove(*spotLightSource);
        break;
    }
    }

    ASSERT_NOT_REACHED();
    return std::nullopt;
}

template<typename Encoder>
void ArgumentCoder<FilterFunction>::encode(Encoder& encoder, const FilterFunction& function)
{
    encoder << function.filterType();

    if (is<SVGFilter>(function)) {
        encoder << downcast<Filter>(function);
        return;
    }

    if (is<FilterEffect>(function)) {
        encoder << downcast<FilterEffect>(function);
        return;
    }
    
    ASSERT_NOT_REACHED();
}

template
void ArgumentCoder<FilterFunction>::encode<Encoder>(Encoder&, const FilterFunction&);
template
void ArgumentCoder<FilterFunction>::encode<StreamConnectionEncoder>(StreamConnectionEncoder&, const FilterFunction&);

std::optional<Ref<FilterFunction>> ArgumentCoder<FilterFunction>::decode(Decoder& decoder)
{
    std::optional<FilterFunction::Type> type;
    decoder >> type;
    if (!type)
        return std::nullopt;

    if (*type == FilterFunction::Type::SVGFilter) {
        std::optional<Ref<Filter>> filter;
        decoder >> filter;
        if (filter)
            return WTFMove(*filter);
    }

    if (*type >= FilterFunction::Type::FEFirst && *type <= FilterFunction::Type::FELast) {
        std::optional<Ref<FilterEffect>> effect;
        decoder >> effect;
        if (effect)
            return WTFMove(*effect);
    }

    ASSERT_NOT_REACHED();
    return std::nullopt;
}

template<typename Encoder>
void ArgumentCoder<FilterEffect>::encode(Encoder& encoder, const FilterEffect& effect)
{
    encoder << effect.filterType();
    encoder << effect.operatingColorSpace();

    switch (effect.filterType()) {
    case FilterEffect::Type::FEBlend:
        encoder << downcast<FEBlend>(effect);
        break;

    case FilterEffect::Type::FEColorMatrix:
        encoder << downcast<FEColorMatrix>(effect);
        break;

    case FilterEffect::Type::FEComponentTransfer:
        encoder << downcast<FEComponentTransfer>(effect);
        break;

    case FilterEffect::Type::FEComposite:
        encoder << downcast<FEComposite>(effect);
        break;

    case FilterEffect::Type::FEConvolveMatrix:
        encoder << downcast<FEConvolveMatrix>(effect);
        break;

    case FilterEffect::Type::FEDiffuseLighting:
        encoder << downcast<FEDiffuseLighting>(effect);
        break;

    case FilterEffect::Type::FEDisplacementMap:
        encoder << downcast<FEDisplacementMap>(effect);
        break;

    case FilterEffect::Type::FEDropShadow:
        encoder << downcast<FEDropShadow>(effect);
        break;

    case FilterEffect::Type::FEFlood:
        encoder << downcast<FEFlood>(effect);
        break;

    case FilterEffect::Type::FEGaussianBlur:
        encoder << downcast<FEGaussianBlur>(effect);
        break;

    case FilterEffect::Type::FEImage:
        encoder << downcast<FEImage>(effect);
        break;

    case FilterEffect::Type::FEMerge:
        encoder << downcast<FEMerge>(effect);
        break;

    case FilterEffect::Type::FEMorphology:
        encoder << downcast<FEMorphology>(effect);
        break;

    case FilterEffect::Type::FEOffset:
        encoder << downcast<FEOffset>(effect);
        break;

    case FilterEffect::Type::FESpecularLighting:
        encoder << downcast<FESpecularLighting>(effect);
        break;

    case FilterEffect::Type::FETile:
        break;

    case FilterEffect::Type::FETurbulence:
        encoder << downcast<FETurbulence>(effect);
        break;

    case FilterEffect::Type::SourceAlpha:
    case FilterEffect::Type::SourceGraphic:
        break;

    default:
        ASSERT_NOT_REACHED();
    }
}

template
void ArgumentCoder<FilterEffect>::encode<Encoder>(Encoder&, const FilterEffect&);
template
void ArgumentCoder<FilterEffect>::encode<StreamConnectionEncoder>(StreamConnectionEncoder&, const FilterEffect&);

std::optional<Ref<FilterEffect>> ArgumentCoder<FilterEffect>::decode(Decoder& decoder)
{
    std::optional<FilterFunction::Type> type;
    decoder >> type;
    if (!type)
        return std::nullopt;

    std::optional<DestinationColorSpace> operatingColorSpace;
    decoder >> operatingColorSpace;
    if (!operatingColorSpace)
        return std::nullopt;

    std::optional<Ref<FilterEffect>> effect;

    switch (*type) {
    case FilterEffect::Type::FEBlend: {
        std::optional<Ref<FEBlend>> feBlend;
        decoder >> feBlend;
        if (feBlend)
            effect = WTFMove(*feBlend);
        break;
    }

    case FilterEffect::Type::FEColorMatrix: {
        std::optional<Ref<FEColorMatrix>> feColorMatrix;
        decoder >> feColorMatrix;
        if (feColorMatrix)
            effect = WTFMove(*feColorMatrix);
        break;
    }

    case FilterEffect::Type::FEComponentTransfer: {
        std::optional<Ref<FEComponentTransfer>> feComponentTransfer;
        decoder >> feComponentTransfer;
        if (feComponentTransfer)
            effect = WTFMove(*feComponentTransfer);
        break;
    }

    case FilterEffect::Type::FEComposite: {
        std::optional<Ref<FEComposite>> feComposite;
        decoder >> feComposite;
        if (feComposite)
            effect = WTFMove(*feComposite);
        break;
    }

    case FilterEffect::Type::FEConvolveMatrix: {
        std::optional<Ref<FEConvolveMatrix>> feConvolveMatrix;
        decoder >> feConvolveMatrix;
        if (feConvolveMatrix)
            effect = WTFMove(*feConvolveMatrix);
        break;
    }

    case FilterEffect::Type::FEDiffuseLighting: {
        std::optional<Ref<FEDiffuseLighting>> feDiffuseLighting;
        decoder >> feDiffuseLighting;
        if (feDiffuseLighting)
            effect = WTFMove(*feDiffuseLighting);
        break;
    }

    case FilterEffect::Type::FEDisplacementMap: {
        std::optional<Ref<FEDisplacementMap>> feDisplacementMap;
        decoder >> feDisplacementMap;
        if (feDisplacementMap)
            effect = WTFMove(*feDisplacementMap);
        break;
    }

    case FilterEffect::Type::FEDropShadow: {
        std::optional<Ref<FEDropShadow>> feDropShadow;
        decoder >> feDropShadow;
        if (feDropShadow)
            effect = WTFMove(*feDropShadow);
        break;
    }

    case FilterEffect::Type::FEFlood: {
        std::optional<Ref<FEFlood>> feFlood;
        decoder >> feFlood;
        if (feFlood)
            effect = WTFMove(*feFlood);
        break;
    }

    case FilterEffect::Type::FEGaussianBlur: {
        std::optional<Ref<FEGaussianBlur>> feGaussianBlur;
        decoder >> feGaussianBlur;
        if (feGaussianBlur)
            effect = WTFMove(*feGaussianBlur);
        break;
    }

    case FilterEffect::Type::FEImage: {
        std::optional<Ref<FEImage>> feImage;
        decoder >> feImage;
        if (feImage)
            effect = WTFMove(*feImage);
        break;
    }

    case FilterEffect::Type::FEMerge: {
        std::optional<Ref<FEMerge>> feMerge;
        decoder >> feMerge;
        if (feMerge)
            effect = WTFMove(*feMerge);
        break;
    }

    case FilterEffect::Type::FEMorphology: {
        std::optional<Ref<FEMorphology>> feMorphology;
        decoder >> feMorphology;
        if (feMorphology)
            effect = WTFMove(*feMorphology);
        break;
    }

    case FilterEffect::Type::FEOffset: {
        std::optional<Ref<FEOffset>> feOffset;
        decoder >> feOffset;
        if (feOffset)
            effect = WTFMove(*feOffset);
        break;
    }

    case FilterEffect::Type::FESpecularLighting: {
        std::optional<Ref<FESpecularLighting>> feSpecularLighting;
        decoder >> feSpecularLighting;
        if (feSpecularLighting)
            effect = WTFMove(*feSpecularLighting);
        break;
    }

    case FilterEffect::Type::FETile:
        effect = FETile::create();
        break;

    case FilterEffect::Type::FETurbulence: {
        std::optional<Ref<FETurbulence>> feTurbulence;
        decoder >> feTurbulence;
        if (feTurbulence)
            effect = WTFMove(*feTurbulence);
        break;
    }

    case FilterEffect::Type::SourceAlpha:
        effect = SourceAlpha::create();
        break;

    case FilterEffect::Type::SourceGraphic:
        effect = SourceGraphic::create();
        break;

    default:
        break;
    }

    if (effect) {
        (*effect)->setOperatingColorSpace(*operatingColorSpace);
        return effect;
    }

    ASSERT_NOT_REACHED();
    return std::nullopt;
}

template<typename Encoder>
void ArgumentCoder<CSSFilter>::encode(Encoder& encoder, const CSSFilter& filter)
{
    encoder << filter.functions().size();
    for (auto& function : filter.functions())
        encoder << function;
}

template
void ArgumentCoder<CSSFilter>::encode<Encoder>(Encoder&, const CSSFilter&);
template
void ArgumentCoder<CSSFilter>::encode<StreamConnectionEncoder>(StreamConnectionEncoder&, const CSSFilter&);

std::optional<Ref<CSSFilter>> ArgumentCoder<CSSFilter>::decode(Decoder& decoder)
{
    std::optional<size_t> size;
    decoder >> size;
    if (!size || !*size)
        return std::nullopt;

    Vector<Ref<FilterFunction>> functions;
    functions.reserveInitialCapacity(*size);

    for (size_t i = 0; i < *size; ++i) {
        std::optional<Ref<FilterFunction>> function;

        decoder >> function;
        if (!function)
            return std::nullopt;

        functions.uncheckedAppend(WTFMove(*function));
    }

    auto filter = CSSFilter::create(WTFMove(functions));
    if (!filter)
        return std::nullopt;

    return filter.releaseNonNull();
}

template<typename Encoder>
void ArgumentCoder<SVGFilter>::encode(Encoder& encoder, const SVGFilter& filter)
{
    encoder << filter.targetBoundingBox();
    encoder << filter.primitiveUnits();

    encoder << filter.expression();
    encoder << filter.effects();

    encoder << filter.renderingResourceIdentifierIfExists();
}

template
void ArgumentCoder<SVGFilter>::encode<Encoder>(Encoder&, const SVGFilter&);
template
void ArgumentCoder<SVGFilter>::encode<StreamConnectionEncoder>(StreamConnectionEncoder&, const SVGFilter&);

std::optional<Ref<SVGFilter>> ArgumentCoder<SVGFilter>::decode(Decoder& decoder)
{
    std::optional<FloatRect> targetBoundingBox;
    decoder >> targetBoundingBox;
    if (!targetBoundingBox)
        return std::nullopt;

    std::optional<SVGUnitTypes::SVGUnitType> primitiveUnits;
    decoder >> primitiveUnits;
    if (!primitiveUnits)
        return std::nullopt;

    std::optional<SVGFilterExpression> expression;
    decoder >> expression;
    if (!expression || expression->isEmpty())
        return std::nullopt;

    std::optional<Vector<Ref<FilterEffect>>> effects;
    decoder >> effects;
    if (!effects || effects->isEmpty())
        return std::nullopt;

    std::optional<std::optional<RenderingResourceIdentifier>> renderingResourceIdentifier;
    decoder >> renderingResourceIdentifier;
    if (!renderingResourceIdentifier)
        return std::nullopt;

    auto filter = WebCore::SVGFilter::create(*targetBoundingBox, *primitiveUnits, WTFMove(*expression), WTFMove(*effects), *renderingResourceIdentifier);
    if (!filter)
        return std::nullopt;

    return filter.releaseNonNull();
}

template<typename Encoder>
void ArgumentCoder<Filter>::encode(Encoder& encoder, const Filter& filter)
{
    encoder << filter.filterType();

    if (is<CSSFilter>(filter))
        encoder << downcast<CSSFilter>(filter);
    else
        encoder << downcast<SVGFilter>(filter);

    encoder << filter.filterRenderingModes();
    encoder << filter.filterScale();
    encoder << filter.filterRegion();
}

template
void ArgumentCoder<Filter>::encode<Encoder>(Encoder&, const Filter&);
template
void ArgumentCoder<Filter>::encode<StreamConnectionEncoder>(StreamConnectionEncoder&, const Filter&);

std::optional<Ref<Filter>> ArgumentCoder<Filter>::decode(Decoder& decoder)
{
    std::optional<FilterFunction::Type> type;
    decoder >> type;
    if (!type)
        return std::nullopt;

    std::optional<Ref<Filter>> filter;

    if (*type == FilterFunction::Type::CSSFilter) {
        std::optional<Ref<CSSFilter>> cssFilter;
        decoder >> cssFilter;
        if (!cssFilter)
            return std::nullopt;

        filter = WTFMove(*cssFilter);
    } else {
        std::optional<Ref<SVGFilter>> svgFilter;
        decoder >> svgFilter;
        if (!svgFilter)
            return std::nullopt;
        
        filter = WTFMove(*svgFilter);
    }

    std::optional<OptionSet<FilterRenderingMode>> filterRenderingModes;
    decoder >> filterRenderingModes;
    if (!filterRenderingModes)
        return std::nullopt;

    std::optional<FloatSize> filterScale;
    decoder >> filterScale;
    if (!filterScale)
        return std::nullopt;

    std::optional<FloatRect> filterRegion;
    decoder >> filterRegion;
    if (!filterRegion)
        return std::nullopt;

    (*filter)->setFilterRenderingModes(*filterRenderingModes);
    (*filter)->setFilterScale(*filterScale);
    (*filter)->setFilterRegion(*filterRegion);

    return filter;
}

template<typename Encoder>
void ArgumentCoder<Path>::encode(Encoder& encoder, const Path& path)
{
    if (auto segment = path.singleSegment())
        encoder << false << *segment;
    else if (auto* segments = path.segmentsIfExists())
        encoder << true << *segments;
    else
        encoder << true << path.segments();
}

template
void ArgumentCoder<Path>::encode<Encoder>(Encoder&, const Path&);
template
void ArgumentCoder<Path>::encode<StreamConnectionEncoder>(StreamConnectionEncoder&, const Path&);

std::optional<Path> ArgumentCoder<Path>::decode(Decoder& decoder)
{
    std::optional<bool> hasVector;
    decoder >> hasVector;
    if (!hasVector)
        return std::nullopt;

    if (!*hasVector) {
        std::optional<PathSegment> segment;
        decoder >> segment;
        if (!segment)
            return std::nullopt;

        return Path(WTFMove(*segment));
    }

    std::optional<Vector<PathSegment>> segments;
    decoder >> segments;
    if (!segments)
        return std::nullopt;

    return Path(WTFMove(*segments));
}

#if ENABLE(ENCRYPTED_MEDIA)
void ArgumentCoder<WebCore::CDMInstanceSession::Message>::encode(Encoder& encoder, const WebCore::CDMInstanceSession::Message& message)
{
    encoder << message.first;
    encoder << message.second;
}

std::optional<WebCore::CDMInstanceSession::Message>  ArgumentCoder<WebCore::CDMInstanceSession::Message>::decode(Decoder& decoder)
{
    WebCore::CDMInstanceSession::MessageType type;
    if (!decoder.decode(type))
        return std::nullopt;

    auto buffer = decoder.decode<Ref<SharedBuffer>>();
    if (UNLIKELY(!buffer))
        return std::nullopt;

    return std::make_optional<WebCore::CDMInstanceSession::Message>({ type, WTFMove(*buffer) });
}
#endif // ENABLE(ENCRYPTED_MEDIA)

#if ENABLE(IMAGE_ANALYSIS) && ENABLE(DATA_DETECTION)

void ArgumentCoder<TextRecognitionDataDetector>::encode(Encoder& encoder, const TextRecognitionDataDetector& info)
{
    encodePlatformData(encoder, info);
    encoder << info.normalizedQuads;
}

std::optional<TextRecognitionDataDetector> ArgumentCoder<TextRecognitionDataDetector>::decode(Decoder& decoder)
{
    TextRecognitionDataDetector result;
    if (!decodePlatformData(decoder, result))
        return std::nullopt;

    std::optional<Vector<FloatQuad>> normalizedQuads;
    decoder >> normalizedQuads;
    if (!normalizedQuads)
        return std::nullopt;

    result.normalizedQuads = WTFMove(*normalizedQuads);
    return WTFMove(result);
}

#endif // ENABLE(IMAGE_ANALYSIS) && ENABLE(DATA_DETECTION)

template<class Encoder>
void ArgumentCoder<PixelBuffer>::encode(Encoder& encoder, const PixelBuffer& pixelBuffer)
{
    if (LIKELY(is<const ByteArrayPixelBuffer>(pixelBuffer))) {
        downcast<const ByteArrayPixelBuffer>(pixelBuffer).encode(encoder);
        return;
    }
    ASSERT_NOT_REACHED();
}

std::optional<Ref<PixelBuffer>> ArgumentCoder<PixelBuffer>::decode(Decoder& decoder)
{
    return ByteArrayPixelBuffer::decode(decoder);
}

template
void ArgumentCoder<PixelBuffer>::encode<Encoder>(Encoder&, const PixelBuffer&);

template
void ArgumentCoder<PixelBuffer>::encode<StreamConnectionEncoder>(StreamConnectionEncoder&, const PixelBuffer&);

} // namespace IPC
