/*
 * Copyright (C) 2010-2022 Apple Inc. All rights reserved.
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

#include "ArgumentCoders.h"
#include "Decoder.h"
#include "Encoder.h"
#include <WebCore/AutoplayEvent.h>
#include <WebCore/ColorSpace.h>
#include <WebCore/DisplayListItems.h>
#include <WebCore/FloatRoundedRect.h>
#include <WebCore/FloatSize.h>
#include <WebCore/FrameLoaderTypes.h>
#include <WebCore/IndexedDB.h>
#include <WebCore/InputMode.h>
#include <WebCore/LayoutPoint.h>
#include <WebCore/LayoutSize.h>
#include <WebCore/LengthBox.h>
#include <WebCore/MediaSelectionOption.h>
#include <WebCore/NetworkLoadMetrics.h>
#include <WebCore/NotificationDirection.h>
#include <WebCore/RealtimeMediaSource.h>
#include <WebCore/RenderingMode.h>
#include <WebCore/ScrollSnapOffsetsInfo.h>
#include <WebCore/ScrollTypes.h>
#include <WebCore/SerializedPlatformDataCueValue.h>
#include <WebCore/ServiceWorkerTypes.h>
#include <WebCore/StoredCredentialsPolicy.h>
#include <WebCore/WorkerType.h>
#include <wtf/ArgumentCoder.h>
#include <wtf/EnumTraits.h>

#if ENABLE(APPLE_PAY)
#include <WebCore/PaymentHeaders.h>
#endif

#if USE(CURL)
#include <WebCore/CurlProxySettings.h>
#endif

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
#include <WebCore/MediaPlaybackTargetContext.h>
#endif

#if ENABLE(ENCRYPTED_MEDIA)
#include <WebCore/CDMInstance.h>
#include <WebCore/CDMInstanceSession.h>
#endif

#if PLATFORM(IOS_FAMILY)
#include <WebCore/InspectorOverlay.h>
#endif

#if PLATFORM(GTK)
#include "ArgumentCodersGtk.h"
#endif

#if ENABLE(GPU_PROCESS) && ENABLE(WEBGL)
#include <WebCore/GraphicsContextGL.h>
#include <WebCore/GraphicsContextGLEnums.h>
#endif

#if ENABLE(WEBXR)
#include <WebCore/PlatformXR.h>
#endif

#if ENABLE(CONTENT_FILTERING)
#include <WebCore/MockContentFilterSettings.h>
#endif

#if PLATFORM(COCOA)
#include "ArgumentCodersCF.h"
#endif

OBJC_CLASS VKCImageAnalysis;

#if USE(AVFOUNDATION)
typedef struct __CVBuffer* CVPixelBufferRef;
#endif

namespace WebCore {

class AppKitControlSystemImage;
class BlobPart;
class CSSFilter;
class ControlPart;
class Credential;
class Cursor;
class Filter;
class FilterEffect;
class FilterFunction;
class FilterOperation;
class FilterOperations;
class FixedPositionViewportConstraints;
class Font;
class FontPlatformData;
class FragmentedSharedBuffer;
class LightSource;
class Path;
class PaymentInstallmentConfiguration;
class PixelBuffer;
class ResourceError;
class SVGFilter;
class ScriptBuffer;
class SerializedScriptValue;
class SharedBuffer;
class StickyPositionViewportConstraints;
class SystemImage;

struct CompositionUnderline;
struct DataDetectorElementInfo;
struct DiagnosticLoggingDictionary;
struct KeypressCommand;
struct Length;
struct SerializedAttachmentData;
struct SoupNetworkProxySettings;
struct TextRecognitionDataDetector;
struct ViewportArguments;

template <class>
struct PathCommand;

namespace DOMCacheEngine {
struct Record;
}

} // namespace WebCore

namespace IPC {

template<> struct ArgumentCoder<WebCore::DOMCacheEngine::Record> {
    static void encode(Encoder&, const WebCore::DOMCacheEngine::Record&);
    static std::optional<WebCore::DOMCacheEngine::Record> decode(Decoder&);
};

template<> struct ArgumentCoder<WebCore::RectEdges<bool>> {
    static void encode(Encoder&, const WebCore::RectEdges<bool>&);
    static std::optional<WebCore::RectEdges<bool>> decode(Decoder&);
};

#if ENABLE(META_VIEWPORT)
template<> struct ArgumentCoder<WebCore::ViewportArguments> {
    static void encode(Encoder&, const WebCore::ViewportArguments&);
    static std::optional<WebCore::ViewportArguments> decode(Decoder&);
};
#endif

template<> struct ArgumentCoder<WebCore::Length> {
    static void encode(Encoder&, const WebCore::Length&);
    static WARN_UNUSED_RETURN bool decode(Decoder&, WebCore::Length&);
};

template<> struct ArgumentCoder<WebCore::Credential> {
    static void encode(Encoder&, const WebCore::Credential&);
    static WARN_UNUSED_RETURN bool decode(Decoder&, WebCore::Credential&);
    static void encodePlatformData(Encoder&, const WebCore::Credential&);
    static WARN_UNUSED_RETURN bool decodePlatformData(Decoder&, WebCore::Credential&);
};

template<> struct ArgumentCoder<WebCore::Cursor> {
    static void encode(Encoder&, const WebCore::Cursor&);
    static WARN_UNUSED_RETURN bool decode(Decoder&, WebCore::Cursor&);
};

template<> struct ArgumentCoder<WebCore::DiagnosticLoggingDictionary> {
    static void encode(Encoder&, const WebCore::DiagnosticLoggingDictionary&);
    static WARN_UNUSED_RETURN bool decode(Decoder&, WebCore::DiagnosticLoggingDictionary&);
};

template<> struct ArgumentCoder<RefPtr<WebCore::Image>> {
    static void encode(Encoder&, const RefPtr<WebCore::Image>&);
    static WARN_UNUSED_RETURN bool decode(Decoder&, RefPtr<WebCore::Image>&);
};

template<> struct ArgumentCoder<RefPtr<WebCore::SerializedScriptValue>> {
    static void encode(Encoder&, const RefPtr<WebCore::SerializedScriptValue>&);
    static std::optional<RefPtr<WebCore::SerializedScriptValue>> decode(Decoder&);
};

template<> struct ArgumentCoder<WebCore::Font> {
    static void encode(Encoder&, const WebCore::Font&);
    static std::optional<Ref<WebCore::Font>> decode(Decoder&);
    static void encodePlatformData(Encoder&, const WebCore::Font&);
    static std::optional<WebCore::FontPlatformData> decodePlatformData(Decoder&);
};

template<> struct ArgumentCoder<WebCore::Font::Attributes> {
    static void encode(Encoder&, const WebCore::Font::Attributes&);
    static std::optional<WebCore::Font::Attributes> decode(Decoder&);
};

template<> struct ArgumentCoder<WebCore::FontPlatformData::Attributes> {
    static void encode(Encoder&, const WebCore::FontPlatformData::Attributes&);
    static std::optional<WebCore::FontPlatformData::Attributes> decode(Decoder&);
    static void encodePlatformData(Encoder&, const WebCore::FontPlatformData::Attributes&);
    static WARN_UNUSED_RETURN bool decodePlatformData(Decoder&, WebCore::FontPlatformData::Attributes&);
};

template<> struct ArgumentCoder<WebCore::FontCustomPlatformData> {
    static void encode(Encoder&, const WebCore::FontCustomPlatformData&);
    static std::optional<Ref<WebCore::FontCustomPlatformData>> decode(Decoder&);
};

template<> struct ArgumentCoder<WebCore::ResourceError> {
    static void encode(Encoder&, const WebCore::ResourceError&);
    static WARN_UNUSED_RETURN bool decode(Decoder&, WebCore::ResourceError&);
    static void encodePlatformData(Encoder&, const WebCore::ResourceError&);
    static WARN_UNUSED_RETURN bool decodePlatformData(Decoder&, WebCore::ResourceError&);
};

#if PLATFORM(COCOA)

template<> struct ArgumentCoder<WebCore::KeypressCommand> {
    static void encode(Encoder&, const WebCore::KeypressCommand&);
    static std::optional<WebCore::KeypressCommand> decode(Decoder&);
};

#endif // PLATFORM(COCOA)

#if PLATFORM(IOS_FAMILY)
template<> struct ArgumentCoder<WebCore::InspectorOverlay::Highlight> {
    static void encode(Encoder&, const WebCore::InspectorOverlay::Highlight&);
    static WARN_UNUSED_RETURN bool decode(Decoder&, WebCore::InspectorOverlay::Highlight&);
};

#endif

#if USE(APPKIT)

template<> struct ArgumentCoder<WebCore::AppKitControlSystemImage> {
    template<typename Encoder>
    static void encode(Encoder&, const WebCore::AppKitControlSystemImage&);
    static std::optional<Ref<WebCore::AppKitControlSystemImage>> decode(Decoder&);
};

#endif

#if USE(SOUP)
template<> struct ArgumentCoder<WebCore::SoupNetworkProxySettings> {
    static void encode(Encoder&, const WebCore::SoupNetworkProxySettings&);
    static WARN_UNUSED_RETURN bool decode(Decoder&, WebCore::SoupNetworkProxySettings&);
};
#endif

#if USE(CURL)
template<> struct ArgumentCoder<WebCore::CurlProxySettings> {
    static void encode(Encoder&, const WebCore::CurlProxySettings&);
    static std::optional<WebCore::CurlProxySettings> decode(Decoder&);
};
#endif


template<> struct ArgumentCoder<WebCore::FixedPositionViewportConstraints> {
    static void encode(Encoder&, const WebCore::FixedPositionViewportConstraints&);
    static WARN_UNUSED_RETURN bool decode(Decoder&, WebCore::FixedPositionViewportConstraints&);
};

template<> struct ArgumentCoder<WebCore::StickyPositionViewportConstraints> {
    static void encode(Encoder&, const WebCore::StickyPositionViewportConstraints&);
    static WARN_UNUSED_RETURN bool decode(Decoder&, WebCore::StickyPositionViewportConstraints&);
};

#if !USE(COORDINATED_GRAPHICS)
template<> struct ArgumentCoder<WebCore::FilterOperations> {
    static void encode(Encoder&, const WebCore::FilterOperations&);
    static WARN_UNUSED_RETURN bool decode(Decoder&, WebCore::FilterOperations&);
};

template<> struct ArgumentCoder<WebCore::FilterOperation> {
    static void encode(Encoder&, const WebCore::FilterOperation&);
};
WARN_UNUSED_RETURN bool decodeFilterOperation(Decoder&, RefPtr<WebCore::FilterOperation>&);

template<> struct ArgumentCoder<RefPtr<WebCore::FilterOperation>> {
    static void encode(Encoder&, const RefPtr<WebCore::FilterOperation>&);
    static WARN_UNUSED_RETURN bool decode(Decoder&, RefPtr<WebCore::FilterOperation>&);
};
#endif

template<> struct ArgumentCoder<WebCore::BlobPart> {
    static void encode(Encoder&, const WebCore::BlobPart&);
    static std::optional<WebCore::BlobPart> decode(Decoder&);
};

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
template<> struct ArgumentCoder<WebCore::MediaPlaybackTargetContext> {
    static void encode(Encoder&, const WebCore::MediaPlaybackTargetContext&);
    static WARN_UNUSED_RETURN bool decode(Decoder&, WebCore::MediaPlaybackTargetContext&);
    static void encodePlatformData(Encoder&, const WebCore::MediaPlaybackTargetContext&);
    static WARN_UNUSED_RETURN bool decodePlatformData(Decoder&, WebCore::MediaPlaybackTargetContext::Type, WebCore::MediaPlaybackTargetContext&);
};
#endif

#if ENABLE(APPLE_PAY)

template<> struct ArgumentCoder<WebCore::Payment> {
    static void encode(Encoder&, const WebCore::Payment&);
    static std::optional<WebCore::Payment> decode(Decoder&);
};

template<> struct ArgumentCoder<WebCore::PaymentContact> {
    static void encode(Encoder&, const WebCore::PaymentContact&);
    static std::optional<WebCore::PaymentContact> decode(Decoder&);
};

template<> struct ArgumentCoder<WebCore::PaymentMerchantSession> {
    static void encode(Encoder&, const WebCore::PaymentMerchantSession&);
    static std::optional<WebCore::PaymentMerchantSession> decode(Decoder&);
};

template<> struct ArgumentCoder<WebCore::PaymentMethod> {
    static void encode(Encoder&, const WebCore::PaymentMethod&);
    static std::optional<WebCore::PaymentMethod> decode(Decoder&);
};

template<> struct ArgumentCoder<WebCore::ApplePaySessionPaymentRequest> {
    static void encode(Encoder&, const WebCore::ApplePaySessionPaymentRequest&);
    static WARN_UNUSED_RETURN bool decode(Decoder&, WebCore::ApplePaySessionPaymentRequest&);
};
template<> struct ArgumentCoder<WebCore::ApplePaySessionPaymentRequest::ContactFields> {
    static void encode(Encoder&, const WebCore::ApplePaySessionPaymentRequest::ContactFields&);
    static WARN_UNUSED_RETURN bool decode(Decoder&, WebCore::ApplePaySessionPaymentRequest::ContactFields&);
};

template<> struct ArgumentCoder<WebCore::ApplePaySessionPaymentRequest::MerchantCapabilities> {
    static void encode(Encoder&, const WebCore::ApplePaySessionPaymentRequest::MerchantCapabilities&);
    static WARN_UNUSED_RETURN bool decode(Decoder&, WebCore::ApplePaySessionPaymentRequest::MerchantCapabilities&);
};

template<> struct ArgumentCoder<RefPtr<WebCore::ApplePayError>> {
    static void encode(Encoder&, const RefPtr<WebCore::ApplePayError>&);
    static std::optional<RefPtr<WebCore::ApplePayError>> decode(Decoder&);
};

template<> struct ArgumentCoder<WebCore::PaymentSessionError> {
    static void encode(Encoder&, const WebCore::PaymentSessionError&);
    static std::optional<WebCore::PaymentSessionError> decode(Decoder&);
};

#endif

#if ENABLE(SERVICE_WORKER)

template<> struct ArgumentCoder<WebCore::ServiceWorkerOrClientData> {
    static void encode(Encoder&, const WebCore::ServiceWorkerOrClientData&);
    static WARN_UNUSED_RETURN bool decode(Decoder&, WebCore::ServiceWorkerOrClientData&);
};

template<> struct ArgumentCoder<WebCore::ServiceWorkerOrClientIdentifier> {
    static void encode(Encoder&, const WebCore::ServiceWorkerOrClientIdentifier&);
    static WARN_UNUSED_RETURN bool decode(Decoder&, WebCore::ServiceWorkerOrClientIdentifier&);
};

#endif

#if ENABLE(ATTACHMENT_ELEMENT)

template<> struct ArgumentCoder<WebCore::SerializedAttachmentData> {
    static void encode(Encoder&, const WebCore::SerializedAttachmentData&);
    static std::optional<WebCore::SerializedAttachmentData> decode(Decoder&);
};

#endif // ENABLE(ATTACHMENT_ELEMENT)

#if ENABLE(VIDEO)
template<> struct ArgumentCoder<WebCore::SerializedPlatformDataCueValue> {
    static void encode(Encoder&, const WebCore::SerializedPlatformDataCueValue&);
    static std::optional<WebCore::SerializedPlatformDataCueValue> decode(Decoder&);
    static void encodePlatformData(Encoder&, const WebCore::SerializedPlatformDataCueValue&);
    static std::optional<WebCore::SerializedPlatformDataCueValue> decodePlatformData(Decoder&, WebCore::SerializedPlatformDataCueValue::PlatformType);
};
#endif

template<> struct ArgumentCoder<WebCore::FragmentedSharedBuffer> {
    static void encode(Encoder&, const WebCore::FragmentedSharedBuffer&);
    static std::optional<Ref<WebCore::FragmentedSharedBuffer>> decode(Decoder&);
};

template<> struct ArgumentCoder<WebCore::SharedBuffer> {
    static void encode(Encoder&, const WebCore::SharedBuffer&);
    static std::optional<Ref<WebCore::SharedBuffer>> decode(Decoder&);
};

template<> struct ArgumentCoder<WebCore::ScriptBuffer> {
    static void encode(Encoder&, const WebCore::ScriptBuffer&);
    static std::optional<WebCore::ScriptBuffer> decode(Decoder&);
};

template<> struct ArgumentCoder<WebCore::SystemImage> {
    template<typename Encoder>
    static void encode(Encoder&, const WebCore::SystemImage&);
    static std::optional<Ref<WebCore::SystemImage>> decode(Decoder&);
};

template<> struct ArgumentCoder<WebCore::ControlPart> {
    template<typename Encoder>
    static void encode(Encoder&, const WebCore::ControlPart&);
    static std::optional<Ref<WebCore::ControlPart>> decode(Decoder&);
};

template<> struct ArgumentCoder<WebCore::LightSource> {
    template<typename Encoder>
    static void encode(Encoder&, const WebCore::LightSource&);
    static std::optional<Ref<WebCore::LightSource>> decode(Decoder&);
};

template<> struct ArgumentCoder<WebCore::FilterFunction> {
    template<typename Encoder>
    static void encode(Encoder&, const WebCore::FilterFunction&);
    static std::optional<Ref<WebCore::FilterFunction>> decode(Decoder&);
};

template<> struct ArgumentCoder<WebCore::FilterEffect> {
    template<typename Encoder>
    static void encode(Encoder&, const WebCore::FilterEffect&);
    static std::optional<Ref<WebCore::FilterEffect>> decode(Decoder&);
};

template<> struct ArgumentCoder<WebCore::CSSFilter> {
    template<typename Encoder>
    static void encode(Encoder&, const WebCore::CSSFilter&);
    static std::optional<Ref<WebCore::CSSFilter>> decode(Decoder&);
};

template<> struct ArgumentCoder<WebCore::SVGFilter> {
    template<typename Encoder>
    static void encode(Encoder&, const WebCore::SVGFilter&);
    static std::optional<Ref<WebCore::SVGFilter>> decode(Decoder&);
};

template<> struct ArgumentCoder<WebCore::Filter> {
    template<typename Encoder>
    static void encode(Encoder&, const WebCore::Filter&);
    static std::optional<Ref<WebCore::Filter>> decode(Decoder&);
};

template<> struct ArgumentCoder<WebCore::Path> {
    template<typename Encoder>
    static void encode(Encoder&, const WebCore::Path&);
    static std::optional<WebCore::Path> decode(Decoder&);
};

#if ENABLE(DATA_DETECTION)

template<> struct ArgumentCoder<WebCore::DataDetectorElementInfo> {
    static void encode(Encoder&, const WebCore::DataDetectorElementInfo&);
    static std::optional<WebCore::DataDetectorElementInfo> decode(Decoder&);
};

#endif

#if ENABLE(ENCRYPTED_MEDIA)
template<> struct ArgumentCoder<WebCore::CDMInstanceSession::Message> {
    static void encode(Encoder&, const WebCore::CDMInstanceSession::Message&);
    static std::optional<WebCore::CDMInstanceSession::Message> decode(Decoder&);
};
#endif

#if ENABLE(IMAGE_ANALYSIS) && ENABLE(DATA_DETECTION)

template<> struct ArgumentCoder<WebCore::TextRecognitionDataDetector> {
    static void encode(Encoder&, const WebCore::TextRecognitionDataDetector&);
    static WARN_UNUSED_RETURN std::optional<WebCore::TextRecognitionDataDetector> decode(Decoder&);
    static void encodePlatformData(Encoder&, const WebCore::TextRecognitionDataDetector&);
    static WARN_UNUSED_RETURN bool decodePlatformData(Decoder&, WebCore::TextRecognitionDataDetector&);
};

#endif // ENABLE(IMAGE_ANALYSIS) && ENABLE(DATA_DETECTION)

#if ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)

template<> struct ArgumentCoder<RetainPtr<VKCImageAnalysis>> {
    static void encode(Encoder&, const RetainPtr<VKCImageAnalysis>&);
    static WARN_UNUSED_RETURN std::optional<RetainPtr<VKCImageAnalysis>> decode(Decoder&);
};

#endif // ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)

#if USE(AVFOUNDATION)

template<> struct ArgumentCoder<RetainPtr<CVPixelBufferRef>> {
    static void encode(Encoder&, const RetainPtr<CVPixelBufferRef>&);
    static std::optional<RetainPtr<CVPixelBufferRef>> decode(Decoder&);
};

#endif

template<> struct ArgumentCoder<WebCore::PixelBuffer> {
    template<class Encoder> static void encode(Encoder&, const WebCore::PixelBuffer&);
    static std::optional<Ref<WebCore::PixelBuffer>> decode(Decoder&);
};

#if PLATFORM(COCOA) && ENABLE(GPU_PROCESS) && ENABLE(WEBGL)

template<> struct ArgumentCoder<WebCore::GraphicsContextGL::EGLImageSourceIOSurfaceHandle> {
    static void encode(Encoder&, WebCore::GraphicsContextGL::EGLImageSourceIOSurfaceHandle&&);
    static std::optional<WebCore::GraphicsContextGL::EGLImageSourceIOSurfaceHandle> decode(Decoder&);
};

template<> struct ArgumentCoder<WebCore::GraphicsContextGL::EGLImageSourceMTLSharedTextureHandle> {
    static void encode(Encoder&, WebCore::GraphicsContextGL::EGLImageSourceMTLSharedTextureHandle&&);
    static std::optional<WebCore::GraphicsContextGL::EGLImageSourceMTLSharedTextureHandle> decode(Decoder&);
};

#endif

} // namespace IPC

namespace WTF {

#if ENABLE(MEDIA_STREAM)
template<> struct EnumTraits<WebCore::RealtimeMediaSource::Type> {
    using values = EnumValues<
        WebCore::RealtimeMediaSource::Type,
        WebCore::RealtimeMediaSource::Type::Audio,
        WebCore::RealtimeMediaSource::Type::Video
    >;
};
#endif

#if USE(CURL)
template <> struct EnumTraits<WebCore::CurlProxySettings::Mode> {
    using values = EnumValues<
        WebCore::CurlProxySettings::Mode,
        WebCore::CurlProxySettings::Mode::Default,
        WebCore::CurlProxySettings::Mode::NoProxy,
        WebCore::CurlProxySettings::Mode::Custom
    >;
};
#endif

#undef Always
template<> struct EnumTraits<WTFLogLevel> {
    using values = EnumValues<
    WTFLogLevel,
    WTFLogLevel::Always,
    WTFLogLevel::Error,
    WTFLogLevel::Warning,
    WTFLogLevel::Info,
    WTFLogLevel::Debug
    >;
};

#if ENABLE(ENCRYPTED_MEDIA)
template <> struct EnumTraits<WebCore::CDMInstanceSession::SessionLoadFailure> {
    using values = EnumValues <
    WebCore::CDMInstanceSession::SessionLoadFailure,
    WebCore::CDMInstanceSession::SessionLoadFailure::None,
    WebCore::CDMInstanceSession::SessionLoadFailure::NoSessionData,
    WebCore::CDMInstanceSession::SessionLoadFailure::MismatchedSessionType,
    WebCore::CDMInstanceSession::SessionLoadFailure::QuotaExceeded,
    WebCore::CDMInstanceSession::SessionLoadFailure::Other
    >;
};

template <> struct EnumTraits<WebCore::CDMInstance::HDCPStatus> {
    using values = EnumValues <
    WebCore::CDMInstance::HDCPStatus,
    WebCore::CDMInstance::HDCPStatus::Unknown,
    WebCore::CDMInstance::HDCPStatus::Valid,
    WebCore::CDMInstance::HDCPStatus::OutputRestricted,
    WebCore::CDMInstance::HDCPStatus::OutputDownscaled
    >;
};
#endif

#if ENABLE(GPU_PROCESS) && ENABLE(WEBGL)
template <> struct EnumTraits<WebCore::GraphicsContextGLSimulatedEventForTesting> {
    using values = EnumValues<
    WebCore::GraphicsContextGLSimulatedEventForTesting,
    WebCore::GraphicsContextGLSimulatedEventForTesting::ContextChange,
    WebCore::GraphicsContextGLSimulatedEventForTesting::GPUStatusFailure,
    WebCore::GraphicsContextGLSimulatedEventForTesting::Timeout
    >;
};
#endif

} // namespace WTF
