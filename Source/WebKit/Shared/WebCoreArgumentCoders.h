/*
 * Copyright (C) 2010-2021 Apple Inc. All rights reserved.
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
#include <WebCore/AutoplayEvent.h>
#include <WebCore/ColorSpace.h>
#include <WebCore/DiagnosticLoggingClient.h>
#include <WebCore/FrameLoaderTypes.h>
#include <WebCore/IndexedDB.h>
#include <WebCore/InputMode.h>
#include <WebCore/MediaSelectionOption.h>
#include <WebCore/NativeImage.h>
#include <WebCore/NetworkLoadMetrics.h>
#include <WebCore/NotificationDirection.h>
#include <WebCore/RealtimeMediaSource.h>
#include <WebCore/RenderingMode.h>
#include <WebCore/ScrollSnapOffsetsInfo.h>
#include <WebCore/SerializedPlatformDataCueValue.h>
#include <WebCore/ServiceWorkerTypes.h>
#include <WebCore/StoredCredentialsPolicy.h>
#include <WebCore/WorkerType.h>
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
#include <WebCore/GraphicsContextGLAttributes.h>
#include <WebCore/GraphicsTypesGL.h>
#endif

#if ENABLE(WEBXR)
#include <WebCore/PlatformXR.h>
#endif

#if PLATFORM(COCOA)
#include "ArgumentCodersCF.h"

namespace WTF {
class MachSendRight;
}
#endif

#if USE(AVFOUNDATION)
typedef struct __CVBuffer* CVPixelBufferRef;
#endif

namespace WebCore {

class AbsolutePositionConstraints;
class AffineTransform;
class AuthenticationChallenge;
class BlobPart;
class CertificateInfo;
class Color;
class Credential;
class CubicBezierTimingFunction;
class Cursor;
class DatabaseDetails;
class DragData;
class File;
class FilterOperation;
class FilterOperations;
class FloatPoint;
class FloatPoint3D;
class FloatRect;
class FloatRoundedRect;
class FloatSize;
class FixedPositionViewportConstraints;
class Font;
class FontPlatformData;
class HTTPHeaderMap;
class IntPoint;
class IntRect;
class IntSize;
class KeyframeValueList;
class LayoutSize;
class LayoutPoint;
class LinearTimingFunction;
class Notification;
class PasteboardCustomData;
class PaymentInstallmentConfiguration;
class ProtectionSpace;
class Region;
class ResourceError;
class ResourceRequest;
class ResourceResponse;
class ScriptBuffer;
class SecurityOrigin;
class SharedBuffer;
class SpringTimingFunction;
class StepsTimingFunction;
class StickyPositionViewportConstraints;
class TextCheckingRequestData;
class TransformationMatrix;
class UserStyleSheet;

struct AttributedString;
struct CacheQueryOptions;
struct CharacterRange;
struct CompositionUnderline;
struct DataDetectorElementInfo;
struct DictationAlternative;
struct DictionaryPopupInfo;
struct EventTrackingRegions;
struct ExceptionDetails;
struct FontAttributes;
struct FileChooserSettings;
struct TextRecognitionDataDetector;
struct RawFile;
struct ShareData;
struct ShareDataWithParsedURL;
struct Length;
struct GrammarDetail;
struct MimeClassInfo;
struct PasteboardImage;
struct PasteboardURL;
struct PluginInfo;
struct PromisedAttachmentInfo;
struct RecentSearch;
struct ResourceLoadStatistics;
struct ScrollableAreaParameters;
struct TextCheckingResult;
struct TextIndicatorData;
struct TouchActionData;
struct VelocityData;
struct ViewportAttributes;
struct WindowFeatures;
    
template<typename> class RectEdges;
using FloatBoxExtent = RectEdges<float>;
using IDBKeyPath = Variant<String, Vector<String>>;

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

#if ENABLE(GPU_PROCESS) && ENABLE(WEBGL)
struct GraphicsContextGLAttributes;
#endif

namespace DOMCacheEngine {
struct CacheInfo;
struct Record;
}

} // namespace WebCore

namespace IPC {

template<> struct ArgumentCoder<WebCore::AffineTransform> {
    static void encode(Encoder&, const WebCore::AffineTransform&);
    static WARN_UNUSED_RETURN bool decode(Decoder&, WebCore::AffineTransform&);
};

template<> struct ArgumentCoder<WebCore::AttributedString> {
    static void encode(Encoder&, const WebCore::AttributedString&);
    static std::optional<WebCore::AttributedString> decode(Decoder&);
};

template<> struct ArgumentCoder<WebCore::CacheQueryOptions> {
    static void encode(Encoder&, const WebCore::CacheQueryOptions&);
    static WARN_UNUSED_RETURN bool decode(Decoder&, WebCore::CacheQueryOptions&);
};

template<> struct ArgumentCoder<WebCore::CharacterRange> {
    static void encode(Encoder&, const WebCore::CharacterRange&);
    static std::optional<WebCore::CharacterRange> decode(Decoder&);
};

template<> struct ArgumentCoder<WebCore::DOMCacheEngine::CacheInfo> {
    static void encode(Encoder&, const WebCore::DOMCacheEngine::CacheInfo&);
    static std::optional<WebCore::DOMCacheEngine::CacheInfo> decode(Decoder&);
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

template<> struct ArgumentCoder<WebCore::TransformationMatrix> {
    static void encode(Encoder&, const WebCore::TransformationMatrix&);
    static WARN_UNUSED_RETURN bool decode(Decoder&, WebCore::TransformationMatrix&);
};

template<> struct ArgumentCoder<WebCore::LinearTimingFunction> {
    static void encode(Encoder&, const WebCore::LinearTimingFunction&);
    static WARN_UNUSED_RETURN bool decode(Decoder&, WebCore::LinearTimingFunction&);
};

template<> struct ArgumentCoder<WebCore::CubicBezierTimingFunction> {
    static void encode(Encoder&, const WebCore::CubicBezierTimingFunction&);
    static WARN_UNUSED_RETURN bool decode(Decoder&, WebCore::CubicBezierTimingFunction&);
};

template<> struct ArgumentCoder<WebCore::StepsTimingFunction> {
    static void encode(Encoder&, const WebCore::StepsTimingFunction&);
    static WARN_UNUSED_RETURN bool decode(Decoder&, WebCore::StepsTimingFunction&);
};

template<> struct ArgumentCoder<WebCore::SpringTimingFunction> {
    static void encode(Encoder&, const WebCore::SpringTimingFunction&);
    static WARN_UNUSED_RETURN bool decode(Decoder&, WebCore::SpringTimingFunction&);
};

template<> struct ArgumentCoder<WebCore::CertificateInfo> {
    static void encode(Encoder&, const WebCore::CertificateInfo&);
    static WARN_UNUSED_RETURN bool decode(Decoder&, WebCore::CertificateInfo&);
};

template<> struct ArgumentCoder<WebCore::FloatPoint> {
    static void encode(Encoder&, const WebCore::FloatPoint&);
    static WARN_UNUSED_RETURN bool decode(Decoder&, WebCore::FloatPoint&);
    static std::optional<WebCore::FloatPoint> decode(Decoder&);
};

template<> struct ArgumentCoder<WebCore::FloatPoint3D> {
    static void encode(Encoder&, const WebCore::FloatPoint3D&);
    static WARN_UNUSED_RETURN bool decode(Decoder&, WebCore::FloatPoint3D&);
};

template<> struct ArgumentCoder<WebCore::FloatRect> {
    static void encode(Encoder&, const WebCore::FloatRect&);
    static WARN_UNUSED_RETURN bool decode(Decoder&, WebCore::FloatRect&);
    static std::optional<WebCore::FloatRect> decode(Decoder&);
};
    
template<> struct ArgumentCoder<WebCore::FloatBoxExtent> {
    static void encode(Encoder&, const WebCore::FloatBoxExtent&);
    static WARN_UNUSED_RETURN bool decode(Decoder&, WebCore::FloatBoxExtent&);
};

template<> struct ArgumentCoder<WebCore::FloatSize> {
    static void encode(Encoder&, const WebCore::FloatSize&);
    static WARN_UNUSED_RETURN bool decode(Decoder&, WebCore::FloatSize&);
};

template<> struct ArgumentCoder<WebCore::FloatRoundedRect> {
    static void encode(Encoder&, const WebCore::FloatRoundedRect&);
    static WARN_UNUSED_RETURN bool decode(Decoder&, WebCore::FloatRoundedRect&);
};

#if ENABLE(META_VIEWPORT)
template<> struct ArgumentCoder<WebCore::ViewportArguments> {
    static void encode(Encoder&, const WebCore::ViewportArguments&);
    static WARN_UNUSED_RETURN bool decode(Decoder&, WebCore::ViewportArguments&);
    static std::optional<WebCore::ViewportArguments> decode(Decoder&);
};

#endif

template<> struct ArgumentCoder<WebCore::ViewportAttributes> {
    static void encode(Encoder&, const WebCore::ViewportAttributes&);
    static WARN_UNUSED_RETURN bool decode(Decoder&, WebCore::ViewportAttributes&);
};

template<> struct ArgumentCoder<WebCore::IntPoint> {
    static void encode(Encoder&, const WebCore::IntPoint&);
    static WARN_UNUSED_RETURN bool decode(Decoder&, WebCore::IntPoint&);
    static std::optional<WebCore::IntPoint> decode(Decoder&);
};

template<> struct ArgumentCoder<WebCore::IntRect> {
    static void encode(Encoder&, const WebCore::IntRect&);
    static WARN_UNUSED_RETURN bool decode(Decoder&, WebCore::IntRect&);
    static std::optional<WebCore::IntRect> decode(Decoder&);
};

template<> struct ArgumentCoder<WebCore::IntSize> {
    template<typename Encoder>
    static void encode(Encoder&, const WebCore::IntSize&);
    static WARN_UNUSED_RETURN bool decode(Decoder&, WebCore::IntSize&);
    static std::optional<WebCore::IntSize> decode(Decoder&);
};

template<> struct ArgumentCoder<WebCore::LayoutSize> {
    static void encode(Encoder&, const WebCore::LayoutSize&);
    static WARN_UNUSED_RETURN bool decode(Decoder&, WebCore::LayoutSize&);
};

template<> struct ArgumentCoder<WebCore::LayoutPoint> {
    static void encode(Encoder&, const WebCore::LayoutPoint&);
    static WARN_UNUSED_RETURN bool decode(Decoder&, WebCore::LayoutPoint&);
};

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

template<> struct ArgumentCoder<WebCore::PluginInfo> {
    static void encode(Encoder&, const WebCore::PluginInfo&);
    static std::optional<WebCore::PluginInfo> decode(Decoder&);
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

template<> struct ArgumentCoder<Ref<WebCore::Font>> {
    static void encode(Encoder&, const Ref<WebCore::Font>&);
    static std::optional<Ref<WebCore::Font>> decode(Decoder&);
    static void encodePlatformData(Encoder&, const Ref<WebCore::Font>&);
    static std::optional<WebCore::FontPlatformData> decodePlatformData(Decoder&);
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
template<> struct ArgumentCoder<WTF::MachSendRight> {
    static void encode(Encoder&, const WTF::MachSendRight&);
    static void encode(Encoder&, WTF::MachSendRight&&);
    static WARN_UNUSED_RETURN bool decode(Decoder&, WTF::MachSendRight&);
};

template<> struct ArgumentCoder<WebCore::KeypressCommand> {
    static void encode(Encoder&, const WebCore::KeypressCommand&);
    static std::optional<WebCore::KeypressCommand> decode(Decoder&);
};

template<> struct ArgumentCoder<CGPoint> {
    static void encode(Encoder&, CGPoint);
    static std::optional<CGPoint> decode(Decoder&);
};

template<> struct ArgumentCoder<CGSize> {
    static void encode(Encoder&, CGSize);
    static std::optional<CGSize> decode(Decoder&);
};

template<> struct ArgumentCoder<CGRect> {
    static void encode(Encoder&, CGRect);
    static std::optional<CGRect> decode(Decoder&);
};

template<> struct ArgumentCoder<CGAffineTransform> {
    static void encode(Encoder&, CGAffineTransform);
    static std::optional<CGAffineTransform> decode(Decoder&);
};
#endif

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

template<> struct ArgumentCoder<WebCore::PasteboardURL> {
    static void encode(Encoder&, const WebCore::PasteboardURL&);
    static WARN_UNUSED_RETURN bool decode(Decoder&, WebCore::PasteboardURL&);
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
    
template<> struct ArgumentCoder<WebCore::RawFile> {
    static void encode(Encoder&, const WebCore::RawFile&);
    static WARN_UNUSED_RETURN bool decode(Decoder&, WebCore::RawFile&);
};

template<> struct ArgumentCoder<WebCore::ShareData> {
    static void encode(Encoder&, const WebCore::ShareData&);
    static WARN_UNUSED_RETURN bool decode(Decoder&, WebCore::ShareData&);
};
    
template<> struct ArgumentCoder<WebCore::ShareDataWithParsedURL> {
    static void encode(Encoder&, const WebCore::ShareDataWithParsedURL&);
    static WARN_UNUSED_RETURN bool decode(Decoder&, WebCore::ShareDataWithParsedURL&);
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

template<> struct ArgumentCoder<WebCore::DictionaryPopupInfo> {
    static void encode(Encoder&, const WebCore::DictionaryPopupInfo&);
    static WARN_UNUSED_RETURN bool decode(Decoder&, WebCore::DictionaryPopupInfo&);
    static void encodePlatformData(Encoder&, const WebCore::DictionaryPopupInfo&);
    static WARN_UNUSED_RETURN bool decodePlatformData(Decoder&, WebCore::DictionaryPopupInfo&);
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

template<> struct ArgumentCoder<WebCore::ResourceLoadStatistics> {
    static void encode(Encoder&, const WebCore::ResourceLoadStatistics&);
    static std::optional<WebCore::ResourceLoadStatistics> decode(Decoder&);
};

#if ENABLE(APPLE_PAY)

template<> struct ArgumentCoder<WebCore::Payment> {
    static void encode(Encoder&, const WebCore::Payment&);
    static std::optional<WebCore::Payment> decode(Decoder&);
};

template<> struct ArgumentCoder<WebCore::PaymentAuthorizationResult> {
    static void encode(Encoder&, const WebCore::PaymentAuthorizationResult&);
    static std::optional<WebCore::PaymentAuthorizationResult> decode(Decoder&);
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

template<> struct ArgumentCoder<WebCore::MediaSelectionOption> {
    static void encode(Encoder&, const WebCore::MediaSelectionOption&);
    static std::optional<WebCore::MediaSelectionOption> decode(Decoder&);
};

template<> struct ArgumentCoder<WebCore::PromisedAttachmentInfo> {
    static void encode(Encoder&, const WebCore::PromisedAttachmentInfo&);
    static WARN_UNUSED_RETURN bool decode(Decoder&, WebCore::PromisedAttachmentInfo&);
};

template<> struct ArgumentCoder<RefPtr<WebCore::SecurityOrigin>> {
    static void encode(Encoder&, const RefPtr<WebCore::SecurityOrigin>&);
    static std::optional<RefPtr<WebCore::SecurityOrigin>> decode(Decoder&);
};

template<> struct ArgumentCoder<WebCore::FontAttributes> {
    static void encode(Encoder&, const WebCore::FontAttributes&);
    static std::optional<WebCore::FontAttributes> decode(Decoder&);
    static void encodePlatformData(Encoder&, const WebCore::FontAttributes&);
    static std::optional<WebCore::FontAttributes> decodePlatformData(Decoder&, WebCore::FontAttributes&);
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

template<> struct ArgumentCoder<RefPtr<WebCore::SharedBuffer>> {
    static void encode(Encoder&, const RefPtr<WebCore::SharedBuffer>&);
    static std::optional<RefPtr<WebCore::SharedBuffer>> decode(Decoder&);
};

template<> struct ArgumentCoder<Ref<WebCore::SharedBuffer>> {
    static void encode(Encoder&, const Ref<WebCore::SharedBuffer>&);
    static std::optional<Ref<WebCore::SharedBuffer>> decode(Decoder&);
};

template<> struct ArgumentCoder<WebCore::ScriptBuffer> {
    static void encode(Encoder&, const WebCore::ScriptBuffer&);
    static std::optional<WebCore::ScriptBuffer> decode(Decoder&);
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

#if ENABLE(GPU_PROCESS) && ENABLE(WEBGL)

template<> struct ArgumentCoder<WebCore::GraphicsContextGLAttributes> {
    static void encode(Encoder&, const WebCore::GraphicsContextGLAttributes&);
    static std::optional<WebCore::GraphicsContextGLAttributes> decode(Decoder&);
};

template<> struct ArgumentCoder<WebCore::GraphicsContextGL::ActiveInfo> {
    template<typename Encoder>
    static void encode(Encoder&, const WebCore::GraphicsContextGL::ActiveInfo&);
    static std::optional<WebCore::GraphicsContextGL::ActiveInfo> decode(Decoder&);
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

#if USE(AVFOUNDATION)

template<> struct ArgumentCoder<RetainPtr<CVPixelBufferRef>> {
    static void encode(Encoder&, const RetainPtr<CVPixelBufferRef>&);
    static std::optional<RetainPtr<CVPixelBufferRef>> decode(Decoder&);
};

#endif

} // namespace IPC

namespace WTF {

template<> struct EnumTraits<WebCore::RenderingMode> {
    using values = EnumValues<
        WebCore::RenderingMode,
        WebCore::RenderingMode::Unaccelerated,
        WebCore::RenderingMode::Accelerated
    >;
};

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

template<> struct EnumTraits<WebCore::NetworkLoadPriority> {
    using values = EnumValues<
        WebCore::NetworkLoadPriority,
        WebCore::NetworkLoadPriority::Low,
        WebCore::NetworkLoadPriority::Medium,
        WebCore::NetworkLoadPriority::High,
        WebCore::NetworkLoadPriority::Unknown
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
        WebCore::RealtimeMediaSource::Type::None,
        WebCore::RealtimeMediaSource::Type::Audio,
        WebCore::RealtimeMediaSource::Type::Video
    >;
};
#endif

template<> struct EnumTraits<WebCore::MediaSelectionOption::Type> {
    using values = EnumValues<
        WebCore::MediaSelectionOption::Type,
        WebCore::MediaSelectionOption::Type::Regular,
        WebCore::MediaSelectionOption::Type::LegibleOff,
        WebCore::MediaSelectionOption::Type::LegibleAuto
    >;
};

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

template <> struct EnumTraits<WebCore::GraphicsContextGLPowerPreference> {
    using values = EnumValues <
    WebCore::GraphicsContextGLPowerPreference,
    WebCore::GraphicsContextGLPowerPreference::Default,
    WebCore::GraphicsContextGLPowerPreference::LowPower,
    WebCore::GraphicsContextGLPowerPreference::HighPerformance
    >;
};

template <> struct EnumTraits<WebCore::GraphicsContextGLWebGLVersion> {
    using values = EnumValues <
    WebCore::GraphicsContextGLWebGLVersion,
    WebCore::GraphicsContextGLWebGLVersion::WebGL1
#if ENABLE(WEBGL2)
    , WebCore::GraphicsContextGLWebGLVersion::WebGL2
#endif
    >;
};

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

} // namespace WTF
