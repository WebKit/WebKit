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
#include <WebCore/DiagnosticLoggingClient.h>
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
#include "ArrayReference.h"
#include <WebCore/GraphicsContextGL.h>
#include <WebCore/GraphicsTypesGL.h>
#endif

#if ENABLE(WEBXR)
#include <WebCore/PlatformXR.h>
#endif

#if ENABLE(CONTENT_FILTERING_IN_NETWORKING_PROCESS)
#include <WebCore/MockContentFilterSettings.h>
#endif

#if PLATFORM(COCOA)
#include "ArgumentCodersCF.h"
#endif

#if USE(UNIX_DOMAIN_SOCKETS)
#include <wtf/unix/UnixFileDescriptor.h>
#endif

OBJC_CLASS VKCImageAnalysis;

#if USE(AVFOUNDATION)
typedef struct __CVBuffer* CVPixelBufferRef;
#endif

namespace WebCore {

class AbsolutePositionConstraints;
class AuthenticationChallenge;
class BlobPart;
class CertificateInfo;
class Color;
class SharedBuffer;
class CSPViolationReportBody;
class Credential;
class Cursor;
class DatabaseDetails;
class DragData;
class DecomposedGlyphs;
class File;
class FilterOperation;
class FilterOperations;
class FixedPositionViewportConstraints;
class Font;
class FontPlatformData;
class HTTPHeaderMap;
class KeyframeValueList;
class Notification;
class NotificationResources;
class PasteboardCustomData;
class PaymentInstallmentConfiguration;
class PixelBuffer;
class ProtectionSpace;
class Region;
class Report;
class ReportBody;
class ResourceError;
class ResourceRequest;
class ResourceResponse;
class ScriptBuffer;
class SecurityOrigin;
class FragmentedSharedBuffer;
class StickyPositionViewportConstraints;
class SystemImage;
class TextCheckingRequestData;
class UserStyleSheet;

struct AttributedString;
struct CompositionUnderline;
struct DataDetectorElementInfo;
struct DictationAlternative;
struct DictionaryPopupInfo;
struct EventTrackingRegions;
struct ExceptionDetails;
struct FontAttributes;
struct FileChooserSettings;
struct TextRecognitionDataDetector;
struct Length;
struct GrammarDetail;
struct MimeClassInfo;
struct PasteboardImage;
struct PromisedAttachmentInfo;
struct RecentSearch;
struct ScrollableAreaParameters;
struct TextCheckingResult;
struct TextIndicatorData;
struct TouchActionData;
struct VelocityData;
struct ViewportAttributes;
struct WindowFeatures;

using IDBKeyPath = std::variant<String, Vector<String>>;

#if PLATFORM(COCOA)
struct KeypressCommand;
#endif

#if PLATFORM(IOS_FAMILY)
class FloatQuad;
class SelectionGeometry;
struct PasteboardImage;
struct PasteboardWebContent;
#endif

#if ENABLE(META_VIEWPORT)
struct ViewportArguments;
#endif

#if USE(SOUP)
struct SoupNetworkProxySettings;
#endif

#if USE(LIBWPE)
struct PasteboardWebContent;
#endif

#if ENABLE(CONTENT_FILTERING)
class ContentFilterUnblockHandler;
#endif

#if ENABLE(MEDIA_STREAM)
struct MediaConstraints;
#endif

#if ENABLE(ATTACHMENT_ELEMENT)
struct SerializedAttachmentData;
#endif

namespace DOMCacheEngine {
struct Record;
}

} // namespace WebCore

namespace IPC {

#define DEFINE_SIMPLE_ARGUMENT_CODER_FOR_HEADER(Type) \
    template<> struct ArgumentCoder<Type> { \
        template<typename Encoder> static void encode(Encoder&, const Type&); \
        static WARN_UNUSED_RETURN bool decode(Decoder&, Type&); \
        static std::optional<Type> decode(Decoder&); \
    };

DEFINE_SIMPLE_ARGUMENT_CODER_FOR_HEADER(WebCore::FloatBoxExtent)

DEFINE_SIMPLE_ARGUMENT_CODER_FOR_HEADER(WebCore::DisplayList::SetInlineFillColor)
DEFINE_SIMPLE_ARGUMENT_CODER_FOR_HEADER(WebCore::DisplayList::SetInlineStrokeColor)

#undef DEFINE_SIMPLE_ARGUMENT_CODER_FOR_HEADER

template<> struct ArgumentCoder<WebCore::AttributedString> {
    static void encode(Encoder&, const WebCore::AttributedString&);
    static std::optional<WebCore::AttributedString> decode(Decoder&);
};

template<> struct ArgumentCoder<WebCore::DOMCacheEngine::Record> {
    static void encode(Encoder&, const WebCore::DOMCacheEngine::Record&);
    static std::optional<WebCore::DOMCacheEngine::Record> decode(Decoder&);
};

template<> struct ArgumentCoder<WebCore::TouchActionData> {
    static void encode(Encoder&, const WebCore::TouchActionData&);
    static std::optional<WebCore::TouchActionData> decode(Decoder&);
};

template<> struct ArgumentCoder<WebCore::EventTrackingRegions> {
    static void encode(Encoder&, const WebCore::EventTrackingRegions&);
    static WARN_UNUSED_RETURN bool decode(Decoder&, WebCore::EventTrackingRegions&);
};

template<> struct ArgumentCoder<WebCore::CertificateInfo> {
    template<typename Encoder>
    static void encode(Encoder&, const WebCore::CertificateInfo&);
    template<typename Decoder>
    static std::optional<WebCore::CertificateInfo> decode(Decoder&);
};

template<> struct ArgumentCoder<WebCore::RectEdges<bool>> {
    static void encode(Encoder&, const WebCore::RectEdges<bool>&);
    static WARN_UNUSED_RETURN bool decode(Decoder&, WebCore::RectEdges<bool>&);
};

#if ENABLE(META_VIEWPORT)
template<> struct ArgumentCoder<WebCore::ViewportArguments> {
    static void encode(Encoder&, const WebCore::ViewportArguments&);
    static WARN_UNUSED_RETURN bool decode(Decoder&, WebCore::ViewportArguments&);
    static std::optional<WebCore::ViewportArguments> decode(Decoder&);
};

#endif

template<> struct ArgumentCoder<WebCore::Length> {
    static void encode(Encoder&, const WebCore::Length&);
    static WARN_UNUSED_RETURN bool decode(Decoder&, WebCore::Length&);
};

template<> struct ArgumentCoder<WebCore::VelocityData> {
    static void encode(Encoder&, const WebCore::VelocityData&);
    static WARN_UNUSED_RETURN bool decode(Decoder&, WebCore::VelocityData&);
};

template<> struct ArgumentCoder<WebCore::MimeClassInfo> {
    static void encode(Encoder&, const WebCore::MimeClassInfo&);
    static std::optional<WebCore::MimeClassInfo> decode(Decoder&);
};

template<> struct ArgumentCoder<WebCore::AuthenticationChallenge> {
    static void encode(Encoder&, const WebCore::AuthenticationChallenge&);
    static WARN_UNUSED_RETURN bool decode(Decoder&, WebCore::AuthenticationChallenge&);
};

template<> struct ArgumentCoder<WebCore::ProtectionSpace> {
    static void encode(Encoder&, const WebCore::ProtectionSpace&);
    static WARN_UNUSED_RETURN bool decode(Decoder&, WebCore::ProtectionSpace&);
    static void encodePlatformData(Encoder&, const WebCore::ProtectionSpace&);
    static WARN_UNUSED_RETURN bool decodePlatformData(Decoder&, WebCore::ProtectionSpace&);
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


template<> struct ArgumentCoder<WebCore::Font> {
    static void encode(Encoder&, const WebCore::Font&);
    static std::optional<Ref<WebCore::Font>> decode(Decoder&);
    static void encodePlatformData(Encoder&, const WebCore::Font&);
    static std::optional<WebCore::FontPlatformData> decodePlatformData(Decoder&);
};

template<> struct ArgumentCoder<WebCore::DecomposedGlyphs> {
    static void encode(Encoder&, const WebCore::DecomposedGlyphs&);
    static std::optional<Ref<WebCore::DecomposedGlyphs>> decode(Decoder&);
};

template<> struct ArgumentCoder<WebCore::ResourceRequest> {
    static void encode(Encoder&, const WebCore::ResourceRequest&);
    static WARN_UNUSED_RETURN bool decode(Decoder&, WebCore::ResourceRequest&);
    static void encodePlatformData(Encoder&, const WebCore::ResourceRequest&);
    static WARN_UNUSED_RETURN bool decodePlatformData(Decoder&, WebCore::ResourceRequest&);
};

template<> struct ArgumentCoder<WebCore::ResourceError> {
    static void encode(Encoder&, const WebCore::ResourceError&);
    static WARN_UNUSED_RETURN bool decode(Decoder&, WebCore::ResourceError&);
    static void encodePlatformData(Encoder&, const WebCore::ResourceError&);
    static WARN_UNUSED_RETURN bool decodePlatformData(Decoder&, WebCore::ResourceError&);
};

template<> struct ArgumentCoder<WebCore::WindowFeatures> {
    static void encode(Encoder&, const WebCore::WindowFeatures&);
    static WARN_UNUSED_RETURN bool decode(Decoder&, WebCore::WindowFeatures&);
};

#if ENABLE(DRAG_SUPPORT)
template<> struct ArgumentCoder<WebCore::DragData> {
    static void encode(Encoder&, const WebCore::DragData&);
    static WARN_UNUSED_RETURN bool decode(Decoder&, WebCore::DragData&);
};
#endif

#if PLATFORM(COCOA)

template<> struct ArgumentCoder<WebCore::KeypressCommand> {
    static void encode(Encoder&, const WebCore::KeypressCommand&);
    static std::optional<WebCore::KeypressCommand> decode(Decoder&);
};

#endif // PLATFORM(COCOA)

#if PLATFORM(IOS_FAMILY)
template<> struct ArgumentCoder<WebCore::SelectionGeometry> {
    static void encode(Encoder&, const WebCore::SelectionGeometry&);
    static std::optional<WebCore::SelectionGeometry> decode(Decoder&);
};

template<> struct ArgumentCoder<WebCore::InspectorOverlay::Highlight> {
    static void encode(Encoder&, const WebCore::InspectorOverlay::Highlight&);
    static WARN_UNUSED_RETURN bool decode(Decoder&, WebCore::InspectorOverlay::Highlight&);
};

template<> struct ArgumentCoder<WebCore::PasteboardWebContent> {
    static void encode(Encoder&, const WebCore::PasteboardWebContent&);
    static WARN_UNUSED_RETURN bool decode(Decoder&, WebCore::PasteboardWebContent&);
};

template<> struct ArgumentCoder<WebCore::PasteboardImage> {
    static void encode(Encoder&, const WebCore::PasteboardImage&);
    static WARN_UNUSED_RETURN bool decode(Decoder&, WebCore::PasteboardImage&);
};
#endif

template<> struct ArgumentCoder<WebCore::PasteboardCustomData> {
    static void encode(Encoder&, const WebCore::PasteboardCustomData&);
    static WARN_UNUSED_RETURN bool decode(Decoder&, WebCore::PasteboardCustomData&);
};

#if USE(SOUP)
template<> struct ArgumentCoder<WebCore::SoupNetworkProxySettings> {
    static void encode(Encoder&, const WebCore::SoupNetworkProxySettings&);
    static WARN_UNUSED_RETURN bool decode(Decoder&, WebCore::SoupNetworkProxySettings&);
};
#endif

#if USE(LIBWPE)
template<> struct ArgumentCoder<WebCore::PasteboardWebContent> {
    static void encode(Encoder&, const WebCore::PasteboardWebContent&);
    static WARN_UNUSED_RETURN bool decode(Decoder&, WebCore::PasteboardWebContent&);
};
#endif

#if USE(CURL)
template<> struct ArgumentCoder<WebCore::CurlProxySettings> {
    static void encode(Encoder&, const WebCore::CurlProxySettings&);
    static std::optional<WebCore::CurlProxySettings> decode(Decoder&);
};
#endif

template<> struct ArgumentCoder<WebCore::CompositionUnderline> {
    static void encode(Encoder&, const WebCore::CompositionUnderline&);
    static std::optional<WebCore::CompositionUnderline> decode(Decoder&);
};

template<> struct ArgumentCoder<WebCore::DatabaseDetails> {
    static void encode(Encoder&, const WebCore::DatabaseDetails&);
    static WARN_UNUSED_RETURN bool decode(Decoder&, WebCore::DatabaseDetails&);
};

template<> struct ArgumentCoder<WebCore::DictationAlternative> {
    static void encode(Encoder&, const WebCore::DictationAlternative&);
    static std::optional<WebCore::DictationAlternative> decode(Decoder&);
};

template<> struct ArgumentCoder<WebCore::FileChooserSettings> {
    static void encode(Encoder&, const WebCore::FileChooserSettings&);
    static WARN_UNUSED_RETURN bool decode(Decoder&, WebCore::FileChooserSettings&);
};

template<> struct ArgumentCoder<WebCore::GrammarDetail> {
    static void encode(Encoder&, const WebCore::GrammarDetail&);
    static std::optional<WebCore::GrammarDetail> decode(Decoder&);
};

template<> struct ArgumentCoder<WebCore::TextCheckingRequestData> {
    static void encode(Encoder&, const WebCore::TextCheckingRequestData&);
    static WARN_UNUSED_RETURN bool decode(Decoder&, WebCore::TextCheckingRequestData&);
};

template<> struct ArgumentCoder<WebCore::TextCheckingResult> {
    static void encode(Encoder&, const WebCore::TextCheckingResult&);
    static std::optional<WebCore::TextCheckingResult> decode(Decoder&);
};

template<> struct ArgumentCoder<WebCore::UserStyleSheet> {
    static void encode(Encoder&, const WebCore::UserStyleSheet&);
    static WARN_UNUSED_RETURN bool decode(Decoder&, WebCore::UserStyleSheet&);
};

template<> struct ArgumentCoder<WebCore::ScrollableAreaParameters> {
    static void encode(Encoder&, const WebCore::ScrollableAreaParameters&);
    static WARN_UNUSED_RETURN bool decode(Decoder&, WebCore::ScrollableAreaParameters&);
};

template<> struct ArgumentCoder<WebCore::FixedPositionViewportConstraints> {
    static void encode(Encoder&, const WebCore::FixedPositionViewportConstraints&);
    static WARN_UNUSED_RETURN bool decode(Decoder&, WebCore::FixedPositionViewportConstraints&);
};

template<> struct ArgumentCoder<WebCore::StickyPositionViewportConstraints> {
    static void encode(Encoder&, const WebCore::StickyPositionViewportConstraints&);
    static WARN_UNUSED_RETURN bool decode(Decoder&, WebCore::StickyPositionViewportConstraints&);
};

template<> struct ArgumentCoder<WebCore::AbsolutePositionConstraints> {
    static void encode(Encoder&, const WebCore::AbsolutePositionConstraints&);
    static WARN_UNUSED_RETURN bool decode(Decoder&, WebCore::AbsolutePositionConstraints&);
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

#if ENABLE(CONTENT_FILTERING)
template<> struct ArgumentCoder<WebCore::ContentFilterUnblockHandler> {
    static void encode(Encoder&, const WebCore::ContentFilterUnblockHandler&);
    static WARN_UNUSED_RETURN bool decode(Decoder&, WebCore::ContentFilterUnblockHandler&);
};
#endif

template<> struct ArgumentCoder<WebCore::TextIndicatorData> {
    static void encode(Encoder&, const WebCore::TextIndicatorData&);
    static std::optional<WebCore::TextIndicatorData> decode(Decoder&);
};

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
template<> struct ArgumentCoder<WebCore::MediaPlaybackTargetContext> {
    static void encode(Encoder&, const WebCore::MediaPlaybackTargetContext&);
    static WARN_UNUSED_RETURN bool decode(Decoder&, WebCore::MediaPlaybackTargetContext&);
    static void encodePlatformData(Encoder&, const WebCore::MediaPlaybackTargetContext&);
    static WARN_UNUSED_RETURN bool decodePlatformData(Decoder&, WebCore::MediaPlaybackTargetContext::Type, WebCore::MediaPlaybackTargetContext&);
};
#endif

template<> struct ArgumentCoder<WebCore::RecentSearch> {
    static void encode(Encoder&, const WebCore::RecentSearch&);
    static std::optional<WebCore::RecentSearch> decode(Decoder&);
};

template<> struct ArgumentCoder<WebCore::ExceptionDetails> {
    static void encode(Encoder&, const WebCore::ExceptionDetails&);
    static WARN_UNUSED_RETURN bool decode(Decoder&, WebCore::ExceptionDetails&);
};

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

#if ENABLE(MEDIA_STREAM)
template<> struct ArgumentCoder<WebCore::MediaConstraints> {
    static void encode(Encoder&, const WebCore::MediaConstraints&);
    static WARN_UNUSED_RETURN bool decode(Decoder&, WebCore::MediaConstraints&);
};
#endif

template<> struct ArgumentCoder<WebCore::IDBKeyPath> {
    static void encode(Encoder&, const WebCore::IDBKeyPath&);
    static WARN_UNUSED_RETURN bool decode(Decoder&, WebCore::IDBKeyPath&);
};

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

template<> struct ArgumentCoder<WebCore::PromisedAttachmentInfo> {
    static void encode(Encoder&, const WebCore::PromisedAttachmentInfo&);
    static WARN_UNUSED_RETURN bool decode(Decoder&, WebCore::PromisedAttachmentInfo&);
};

template<> struct ArgumentCoder<RefPtr<WebCore::SecurityOrigin>> {
    static void encode(Encoder&, const RefPtr<WebCore::SecurityOrigin>&);
    static std::optional<RefPtr<WebCore::SecurityOrigin>> decode(Decoder&);
};

template<> struct ArgumentCoder<Ref<WebCore::SecurityOrigin>> {
    static void encode(Encoder&, const Ref<WebCore::SecurityOrigin>&);
    static std::optional<Ref<WebCore::SecurityOrigin>> decode(Decoder&);
};

template<> struct ArgumentCoder<WebCore::FontAttributes> {
    static void encode(Encoder&, const WebCore::FontAttributes&);
    static std::optional<WebCore::FontAttributes> decode(Decoder&);
};

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

template<> struct ArgumentCoder<WebCore::NotificationResources> {
    static void encode(Encoder&, const WebCore::NotificationResources&);
    static std::optional<RefPtr<WebCore::NotificationResources>> decode(Decoder&);
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

#if HAVE(PASSKIT_INSTALLMENTS)
template<> struct ArgumentCoder<WebCore::PaymentInstallmentConfiguration> {
    static void encode(Encoder&, const WebCore::PaymentInstallmentConfiguration&);
    static std::optional<WebCore::PaymentInstallmentConfiguration> decode(Decoder&);
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

#if USE(UNIX_DOMAIN_SOCKETS)

template<> struct ArgumentCoder<UnixFileDescriptor> {
    static void encode(Encoder&, const UnixFileDescriptor&);
    static void encode(Encoder&, UnixFileDescriptor&&);
    static std::optional<UnixFileDescriptor> decode(Decoder&);
};

#endif

template<> struct ArgumentCoder<WebCore::PixelBuffer> {
    template<class Encoder> static void encode(Encoder&, const WebCore::PixelBuffer&);
    static std::optional<Ref<WebCore::PixelBuffer>> decode(Decoder&);
};

template<> struct ArgumentCoder<RefPtr<WebCore::ReportBody>> {
    static void encode(Encoder&, const RefPtr<WebCore::ReportBody>&);
    static std::optional<RefPtr<WebCore::ReportBody>> decode(Decoder&);
};

} // namespace IPC

namespace WTF {

template<> struct EnumTraits<WebCore::RenderingMode> {
    using values = EnumValues<
        WebCore::RenderingMode,
        WebCore::RenderingMode::Unaccelerated,
        WebCore::RenderingMode::Accelerated
    >;
};

template<> struct EnumTraits<WebCore::RenderingPurpose> {
    using values = EnumValues<
        WebCore::RenderingPurpose,
        WebCore::RenderingPurpose::Unspecified,
        WebCore::RenderingPurpose::Canvas,
        WebCore::RenderingPurpose::DOM,
        WebCore::RenderingPurpose::LayerBacking,
        WebCore::RenderingPurpose::Snapshot,
        WebCore::RenderingPurpose::ShareableSnapshot,
        WebCore::RenderingPurpose::MediaPainting
    >;
};

#if ENABLE(CONTENT_FILTERING_IN_NETWORKING_PROCESS)
template<> struct EnumTraits<WebCore::MockContentFilterSettings::DecisionPoint> {
    using values = EnumValues<
        WebCore::MockContentFilterSettings::DecisionPoint,
        WebCore::MockContentFilterSettings::DecisionPoint::AfterWillSendRequest,
        WebCore::MockContentFilterSettings::DecisionPoint::AfterRedirect,
        WebCore::MockContentFilterSettings::DecisionPoint::AfterResponse,
        WebCore::MockContentFilterSettings::DecisionPoint::AfterAddData,
        WebCore::MockContentFilterSettings::DecisionPoint::AfterFinishedAddingData,
        WebCore::MockContentFilterSettings::DecisionPoint::Never
    >;
};

template<> struct EnumTraits<WebCore::MockContentFilterSettings::Decision> {
    using values = EnumValues<
        WebCore::MockContentFilterSettings::Decision,
        WebCore::MockContentFilterSettings::Decision::Allow,
        WebCore::MockContentFilterSettings::Decision::Block
    >;
};
#endif

template<> struct EnumTraits<WebCore::AutoplayEvent> {
    using values = EnumValues<
        WebCore::AutoplayEvent,
        WebCore::AutoplayEvent::DidPreventMediaFromPlaying,
        WebCore::AutoplayEvent::DidPlayMediaWithUserGesture,
        WebCore::AutoplayEvent::DidAutoplayMediaPastThresholdWithoutUserInterference,
        WebCore::AutoplayEvent::UserDidInterfereWithPlayback
    >;
};

template<> struct EnumTraits<WebCore::InputMode> {
    using values = EnumValues<
        WebCore::InputMode,
        WebCore::InputMode::Unspecified,
        WebCore::InputMode::None,
        WebCore::InputMode::Text,
        WebCore::InputMode::Telephone,
        WebCore::InputMode::Url,
        WebCore::InputMode::Email,
        WebCore::InputMode::Numeric,
        WebCore::InputMode::Decimal,
        WebCore::InputMode::Search
    >;
};

template<> struct EnumTraits<WebCore::NotificationDirection> {
    using values = EnumValues<
        WebCore::NotificationDirection,
        WebCore::NotificationDirection::Auto,
        WebCore::NotificationDirection::Ltr,
        WebCore::NotificationDirection::Rtl
    >;
};

template<> struct EnumTraits<WebCore::IndexedDB::GetAllType> {
    using values = EnumValues<
        WebCore::IndexedDB::GetAllType,
        WebCore::IndexedDB::GetAllType::Keys,
        WebCore::IndexedDB::GetAllType::Values
    >;
};

#if ENABLE(MEDIA_STREAM)
template<> struct EnumTraits<WebCore::RealtimeMediaSource::Type> {
    using values = EnumValues<
        WebCore::RealtimeMediaSource::Type,
        WebCore::RealtimeMediaSource::Type::Audio,
        WebCore::RealtimeMediaSource::Type::Video
    >;
};
#endif

template <> struct EnumTraits<WebCore::WorkerType> {
    using values = EnumValues<
        WebCore::WorkerType,
        WebCore::WorkerType::Classic,
        WebCore::WorkerType::Module
    >;
};

template<> struct EnumTraits<WebCore::StoredCredentialsPolicy> {
    using values = EnumValues<
        WebCore::StoredCredentialsPolicy,
        WebCore::StoredCredentialsPolicy::DoNotUse,
        WebCore::StoredCredentialsPolicy::Use,
        WebCore::StoredCredentialsPolicy::EphemeralStateless
    >;
};

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

template<> struct EnumTraits<WTFLogChannelState> {
    using values = EnumValues<
    WTFLogChannelState,
    WTFLogChannelState::Off,
    WTFLogChannelState::On,
    WTFLogChannelState::OnWithAccumulation
    >;
};

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

template <> struct EnumTraits<WebCore::GraphicsContextGL::SimulatedEventForTesting> {
    using values = EnumValues<
    WebCore::GraphicsContextGL::SimulatedEventForTesting,
    WebCore::GraphicsContextGL::SimulatedEventForTesting::ContextChange,
    WebCore::GraphicsContextGL::SimulatedEventForTesting::GPUStatusFailure,
    WebCore::GraphicsContextGL::SimulatedEventForTesting::Timeout
    >;
};
#endif

template<> struct EnumTraits<WebCore::ScrollSnapStrictness> {
    using values = EnumValues<
        WebCore::ScrollSnapStrictness,
        WebCore::ScrollSnapStrictness::None,
        WebCore::ScrollSnapStrictness::Proximity,
        WebCore::ScrollSnapStrictness::Mandatory
    >;
};

template<> struct EnumTraits<WebCore::LengthType> {
    using values = EnumValues<
        WebCore::LengthType,
        WebCore::LengthType::Auto,
        WebCore::LengthType::Relative,
        WebCore::LengthType::Percent,
        WebCore::LengthType::Fixed,
        WebCore::LengthType::Intrinsic,
        WebCore::LengthType::MinIntrinsic,
        WebCore::LengthType::MinContent,
        WebCore::LengthType::MaxContent,
        WebCore::LengthType::FillAvailable,
        WebCore::LengthType::FitContent,
        WebCore::LengthType::Calculated,
        WebCore::LengthType::Undefined
    >;
};

template<> struct EnumTraits<WebCore::OverscrollBehavior> {
    using values = EnumValues<
        WebCore::OverscrollBehavior,
        WebCore::OverscrollBehavior::Auto,
        WebCore::OverscrollBehavior::Contain,
        WebCore::OverscrollBehavior::None
    >;
};

} // namespace WTF
