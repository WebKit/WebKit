/*
 * Copyright (C) 2010-2018 Apple Inc. All rights reserved.
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
#include <WebCore/CacheStorageConnection.h>
#include <WebCore/ColorSpace.h>
#include <WebCore/DiagnosticLoggingClient.h>
#include <WebCore/FrameLoaderTypes.h>
#include <WebCore/IndexedDB.h>
#include <WebCore/MediaSelectionOption.h>
#include <WebCore/NetworkLoadMetrics.h>
#include <WebCore/NotificationDirection.h>
#include <WebCore/RealtimeMediaSource.h>
#include <WebCore/ScrollSnapOffsetsInfo.h>
#include <WebCore/ServiceWorkerTypes.h>
#include <WebCore/StoredCredentialsPolicy.h>
#include <WebCore/WorkerType.h>

#if ENABLE(APPLE_PAY)
#include <WebCore/PaymentHeaders.h>
#endif

#if PLATFORM(COCOA)
namespace WTF {
class MachSendRight;
}
#endif

namespace WebCore {
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
class FilterOperation;
class FilterOperations;
class FloatPoint;
class FloatPoint3D;
class FloatRect;
class FloatRoundedRect;
class FloatSize;
class FixedPositionViewportConstraints;
class HTTPHeaderMap;
class IntPoint;
class IntRect;
class IntSize;
class KeyframeValueList;
class LayoutSize;
class LayoutPoint;
class LinearTimingFunction;
class Notification;
class Path;
class ProtectionSpace;
class Region;
class ResourceError;
class ResourceRequest;
class ResourceResponse;
class SecurityOrigin;
class SpringTimingFunction;
class StepsTimingFunction;
class FramesTimingFunction;
class StickyPositionViewportConstraints;
class TextCheckingRequestData;
class TransformationMatrix;
class UserStyleSheet;
class URL;

struct CacheQueryOptions;
struct CompositionUnderline;
struct DictationAlternative;
struct DictionaryPopupInfo;
struct EventTrackingRegions;
struct ExceptionDetails;
struct FontAttributes;
struct FileChooserSettings;
struct ShareData;
struct ShareDataWithParsedURL;
struct Length;
struct GrammarDetail;
struct MimeClassInfo;
struct PasteboardImage;
struct PasteboardCustomData;
struct PasteboardURL;
struct PluginInfo;
struct PromisedAttachmentInfo;
struct RecentSearch;
struct ResourceLoadStatistics;
struct ScrollableAreaParameters;
struct TextCheckingResult;
struct TextIndicatorData;
struct ViewportAttributes;
struct WindowFeatures;
    
template <typename> class RectEdges;
using FloatBoxExtent = RectEdges<float>;

#if PLATFORM(COCOA)
struct KeypressCommand;
#endif

#if PLATFORM(IOS)
class FloatQuad;
class SelectionRect;
struct Highlight;
struct PasteboardImage;
struct PasteboardWebContent;
struct ViewportArguments;
#endif

#if ENABLE(DATALIST_ELEMENT)
struct DataListSuggestionInformation;
#endif

#if USE(SOUP)
struct SoupNetworkProxySettings;
#endif

#if PLATFORM(WPE)
struct PasteboardWebContent;
#endif

#if ENABLE(CONTENT_FILTERING)
class ContentFilterUnblockHandler;
#endif

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
class MediaPlaybackTargetContext;
#endif

#if ENABLE(MEDIA_SESSION)
class MediaSessionMetadata;
#endif

#if ENABLE(MEDIA_STREAM)
struct MediaConstraints;
#endif

#if ENABLE(INDEXED_DATABASE)
using IDBKeyPath = Variant<String, Vector<String>>;
#endif
}

namespace IPC {

template<> struct ArgumentCoder<WebCore::AffineTransform> {
    static void encode(Encoder&, const WebCore::AffineTransform&);
    static bool decode(Decoder&, WebCore::AffineTransform&);
};

template<> struct ArgumentCoder<WebCore::CacheQueryOptions> {
    static void encode(Encoder&, const WebCore::CacheQueryOptions&);
    static bool decode(Decoder&, WebCore::CacheQueryOptions&);
};

template<> struct ArgumentCoder<WebCore::DOMCacheEngine::CacheInfo> {
    static void encode(Encoder&, const WebCore::DOMCacheEngine::CacheInfo&);
    static std::optional<WebCore::DOMCacheEngine::CacheInfo> decode(Decoder&);
};

template<> struct ArgumentCoder<WebCore::DOMCacheEngine::Record> {
    static void encode(Encoder&, const WebCore::DOMCacheEngine::Record&);
    static std::optional<WebCore::DOMCacheEngine::Record> decode(Decoder&);
};

template<> struct ArgumentCoder<WebCore::EventTrackingRegions> {
    static void encode(Encoder&, const WebCore::EventTrackingRegions&);
    static bool decode(Decoder&, WebCore::EventTrackingRegions&);
};

template<> struct ArgumentCoder<WebCore::TransformationMatrix> {
    static void encode(Encoder&, const WebCore::TransformationMatrix&);
    static bool decode(Decoder&, WebCore::TransformationMatrix&);
};

template<> struct ArgumentCoder<WebCore::LinearTimingFunction> {
    static void encode(Encoder&, const WebCore::LinearTimingFunction&);
    static bool decode(Decoder&, WebCore::LinearTimingFunction&);
};

template<> struct ArgumentCoder<WebCore::CubicBezierTimingFunction> {
    static void encode(Encoder&, const WebCore::CubicBezierTimingFunction&);
    static bool decode(Decoder&, WebCore::CubicBezierTimingFunction&);
};

template<> struct ArgumentCoder<WebCore::StepsTimingFunction> {
    static void encode(Encoder&, const WebCore::StepsTimingFunction&);
    static bool decode(Decoder&, WebCore::StepsTimingFunction&);
};

template<> struct ArgumentCoder<WebCore::FramesTimingFunction> {
    static void encode(Encoder&, const WebCore::FramesTimingFunction&);
    static bool decode(Decoder&, WebCore::FramesTimingFunction&);
};

template<> struct ArgumentCoder<WebCore::SpringTimingFunction> {
    static void encode(Encoder&, const WebCore::SpringTimingFunction&);
    static bool decode(Decoder&, WebCore::SpringTimingFunction&);
};

template<> struct ArgumentCoder<WebCore::CertificateInfo> {
    static void encode(Encoder&, const WebCore::CertificateInfo&);
    static bool decode(Decoder&, WebCore::CertificateInfo&);
};

template<> struct ArgumentCoder<WebCore::FloatPoint> {
    static void encode(Encoder&, const WebCore::FloatPoint&);
    static bool decode(Decoder&, WebCore::FloatPoint&);
    static std::optional<WebCore::FloatPoint> decode(Decoder&);
};

template<> struct ArgumentCoder<WebCore::FloatPoint3D> {
    static void encode(Encoder&, const WebCore::FloatPoint3D&);
    static bool decode(Decoder&, WebCore::FloatPoint3D&);
};

template<> struct ArgumentCoder<WebCore::FloatRect> {
    static void encode(Encoder&, const WebCore::FloatRect&);
    static bool decode(Decoder&, WebCore::FloatRect&);
    static std::optional<WebCore::FloatRect> decode(Decoder&);
};
    
template<> struct ArgumentCoder<WebCore::FloatBoxExtent> {
    static void encode(Encoder&, const WebCore::FloatBoxExtent&);
    static bool decode(Decoder&, WebCore::FloatBoxExtent&);
};

template<> struct ArgumentCoder<WebCore::FloatSize> {
    static void encode(Encoder&, const WebCore::FloatSize&);
    static bool decode(Decoder&, WebCore::FloatSize&);
};

template<> struct ArgumentCoder<WebCore::FloatRoundedRect> {
    static void encode(Encoder&, const WebCore::FloatRoundedRect&);
    static bool decode(Decoder&, WebCore::FloatRoundedRect&);
};

#if PLATFORM(IOS)
template<> struct ArgumentCoder<WebCore::FloatQuad> {
    static void encode(Encoder&, const WebCore::FloatQuad&);
    static std::optional<WebCore::FloatQuad> decode(Decoder&);
};

template<> struct ArgumentCoder<WebCore::ViewportArguments> {
    static void encode(Encoder&, const WebCore::ViewportArguments&);
    static bool decode(Decoder&, WebCore::ViewportArguments&);
    static std::optional<WebCore::ViewportArguments> decode(Decoder&);
};
#endif // PLATFORM(IOS)

template<> struct ArgumentCoder<WebCore::IntPoint> {
    static void encode(Encoder&, const WebCore::IntPoint&);
    static bool decode(Decoder&, WebCore::IntPoint&);
    static std::optional<WebCore::IntPoint> decode(Decoder&);
};

template<> struct ArgumentCoder<WebCore::IntRect> {
    static void encode(Encoder&, const WebCore::IntRect&);
    static bool decode(Decoder&, WebCore::IntRect&);
    static std::optional<WebCore::IntRect> decode(Decoder&);
};

template<> struct ArgumentCoder<WebCore::IntSize> {
    static void encode(Encoder&, const WebCore::IntSize&);
    static bool decode(Decoder&, WebCore::IntSize&);
    static std::optional<WebCore::IntSize> decode(Decoder&);
};

template<> struct ArgumentCoder<WebCore::LayoutSize> {
    static void encode(Encoder&, const WebCore::LayoutSize&);
    static bool decode(Decoder&, WebCore::LayoutSize&);
};

template<> struct ArgumentCoder<WebCore::LayoutPoint> {
    static void encode(Encoder&, const WebCore::LayoutPoint&);
    static bool decode(Decoder&, WebCore::LayoutPoint&);
};

template<> struct ArgumentCoder<WebCore::Path> {
    static void encode(Encoder&, const WebCore::Path&);
    static bool decode(Decoder&, WebCore::Path&);
    static std::optional<WebCore::Path> decode(Decoder&);
};

template<> struct ArgumentCoder<WebCore::Region> {
    static void encode(Encoder&, const WebCore::Region&);
    static bool decode(Decoder&, WebCore::Region&);
};

template<> struct ArgumentCoder<WebCore::Length> {
    static void encode(Encoder&, const WebCore::Length&);
    static bool decode(Decoder&, WebCore::Length&);
};

template<> struct ArgumentCoder<WebCore::ViewportAttributes> {
    static void encode(Encoder&, const WebCore::ViewportAttributes&);
    static bool decode(Decoder&, WebCore::ViewportAttributes&);
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
    static bool decode(Decoder&, WebCore::AuthenticationChallenge&);
};

template<> struct ArgumentCoder<WebCore::ProtectionSpace> {
    static void encode(Encoder&, const WebCore::ProtectionSpace&);
    static bool decode(Decoder&, WebCore::ProtectionSpace&);
    static void encodePlatformData(Encoder&, const WebCore::ProtectionSpace&);
    static bool decodePlatformData(Decoder&, WebCore::ProtectionSpace&);
};

template<> struct ArgumentCoder<WebCore::Credential> {
    static void encode(Encoder&, const WebCore::Credential&);
    static bool decode(Decoder&, WebCore::Credential&);
    static void encodePlatformData(Encoder&, const WebCore::Credential&);
    static bool decodePlatformData(Decoder&, WebCore::Credential&);
};

template<> struct ArgumentCoder<WebCore::Cursor> {
    static void encode(Encoder&, const WebCore::Cursor&);
    static bool decode(Decoder&, WebCore::Cursor&);
};

template<> struct ArgumentCoder<WebCore::ResourceRequest> {
    static void encode(Encoder&, const WebCore::ResourceRequest&);
    static bool decode(Decoder&, WebCore::ResourceRequest&);
    static void encodePlatformData(Encoder&, const WebCore::ResourceRequest&);
    static bool decodePlatformData(Decoder&, WebCore::ResourceRequest&);
};

template<> struct ArgumentCoder<WebCore::ResourceError> {
    static void encode(Encoder&, const WebCore::ResourceError&);
    static bool decode(Decoder&, WebCore::ResourceError&);
    static void encodePlatformData(Encoder&, const WebCore::ResourceError&);
    static bool decodePlatformData(Decoder&, WebCore::ResourceError&);
};

template<> struct ArgumentCoder<WebCore::WindowFeatures> {
    static void encode(Encoder&, const WebCore::WindowFeatures&);
    static bool decode(Decoder&, WebCore::WindowFeatures&);
};

template<> struct ArgumentCoder<WebCore::Color> {
    static void encode(Encoder&, const WebCore::Color&);
    static bool decode(Decoder&, WebCore::Color&);
    static std::optional<WebCore::Color> decode(Decoder&);
};

#if ENABLE(DRAG_SUPPORT)
template<> struct ArgumentCoder<WebCore::DragData> {
    static void encode(Encoder&, const WebCore::DragData&);
    static bool decode(Decoder&, WebCore::DragData&);
};
#endif

#if PLATFORM(COCOA)
template<> struct ArgumentCoder<WTF::MachSendRight> {
    static void encode(Encoder&, const WTF::MachSendRight&);
    static void encode(Encoder&, WTF::MachSendRight&&);
    static bool decode(Decoder&, WTF::MachSendRight&);
};

template<> struct ArgumentCoder<WebCore::KeypressCommand> {
    static void encode(Encoder&, const WebCore::KeypressCommand&);
    static std::optional<WebCore::KeypressCommand> decode(Decoder&);
};
#endif

#if PLATFORM(IOS)
template<> struct ArgumentCoder<WebCore::SelectionRect> {
    static void encode(Encoder&, const WebCore::SelectionRect&);
    static std::optional<WebCore::SelectionRect> decode(Decoder&);
};

template<> struct ArgumentCoder<WebCore::Highlight> {
    static void encode(Encoder&, const WebCore::Highlight&);
    static bool decode(Decoder&, WebCore::Highlight&);
};

template<> struct ArgumentCoder<WebCore::PasteboardWebContent> {
    static void encode(Encoder&, const WebCore::PasteboardWebContent&);
    static bool decode(Decoder&, WebCore::PasteboardWebContent&);
};

template<> struct ArgumentCoder<WebCore::PasteboardImage> {
    static void encode(Encoder&, const WebCore::PasteboardImage&);
    static bool decode(Decoder&, WebCore::PasteboardImage&);
};
#endif

template<> struct ArgumentCoder<WebCore::PasteboardCustomData> {
    static void encode(Encoder&, const WebCore::PasteboardCustomData&);
    static bool decode(Decoder&, WebCore::PasteboardCustomData&);
};

template<> struct ArgumentCoder<WebCore::PasteboardURL> {
    static void encode(Encoder&, const WebCore::PasteboardURL&);
    static bool decode(Decoder&, WebCore::PasteboardURL&);
};

#if USE(SOUP)
template<> struct ArgumentCoder<WebCore::SoupNetworkProxySettings> {
    static void encode(Encoder&, const WebCore::SoupNetworkProxySettings&);
    static bool decode(Decoder&, WebCore::SoupNetworkProxySettings&);
};
#endif

#if PLATFORM(WPE)
template<> struct ArgumentCoder<WebCore::PasteboardWebContent> {
    static void encode(Encoder&, const WebCore::PasteboardWebContent&);
    static bool decode(Decoder&, WebCore::PasteboardWebContent&);
};
#endif

template<> struct ArgumentCoder<WebCore::CompositionUnderline> {
    static void encode(Encoder&, const WebCore::CompositionUnderline&);
    static std::optional<WebCore::CompositionUnderline> decode(Decoder&);
};

template<> struct ArgumentCoder<WebCore::DatabaseDetails> {
    static void encode(Encoder&, const WebCore::DatabaseDetails&);
    static bool decode(Decoder&, WebCore::DatabaseDetails&);
};

#if ENABLE(DATALIST_ELEMENT)
template<> struct ArgumentCoder<WebCore::DataListSuggestionInformation> {
    static void encode(Encoder&, const WebCore::DataListSuggestionInformation&);
    static bool decode(Decoder&, WebCore::DataListSuggestionInformation&);
};
#endif

template<> struct ArgumentCoder<WebCore::DictationAlternative> {
    static void encode(Encoder&, const WebCore::DictationAlternative&);
    static std::optional<WebCore::DictationAlternative> decode(Decoder&);
};

template<> struct ArgumentCoder<WebCore::FileChooserSettings> {
    static void encode(Encoder&, const WebCore::FileChooserSettings&);
    static bool decode(Decoder&, WebCore::FileChooserSettings&);
};
    
template<> struct ArgumentCoder<WebCore::ShareData> {
    static void encode(Encoder&, const WebCore::ShareData&);
    static bool decode(Decoder&, WebCore::ShareData&);
};
    
template<> struct ArgumentCoder<WebCore::ShareDataWithParsedURL> {
    static void encode(Encoder&, const WebCore::ShareDataWithParsedURL&);
    static bool decode(Decoder&, WebCore::ShareDataWithParsedURL&);
};

template<> struct ArgumentCoder<WebCore::GrammarDetail> {
    static void encode(Encoder&, const WebCore::GrammarDetail&);
    static std::optional<WebCore::GrammarDetail> decode(Decoder&);
};

template<> struct ArgumentCoder<WebCore::TextCheckingRequestData> {
    static void encode(Encoder&, const WebCore::TextCheckingRequestData&);
    static bool decode(Decoder&, WebCore::TextCheckingRequestData&);
};

template<> struct ArgumentCoder<WebCore::TextCheckingResult> {
    static void encode(Encoder&, const WebCore::TextCheckingResult&);
    static std::optional<WebCore::TextCheckingResult> decode(Decoder&);
};

template<> struct ArgumentCoder<WebCore::UserStyleSheet> {
    static void encode(Encoder&, const WebCore::UserStyleSheet&);
    static bool decode(Decoder&, WebCore::UserStyleSheet&);
};

template<> struct ArgumentCoder<WebCore::ScrollableAreaParameters> {
    static void encode(Encoder&, const WebCore::ScrollableAreaParameters&);
    static bool decode(Decoder&, WebCore::ScrollableAreaParameters&);
};

template<> struct ArgumentCoder<WebCore::FixedPositionViewportConstraints> {
    static void encode(Encoder&, const WebCore::FixedPositionViewportConstraints&);
    static bool decode(Decoder&, WebCore::FixedPositionViewportConstraints&);
};

template<> struct ArgumentCoder<WebCore::StickyPositionViewportConstraints> {
    static void encode(Encoder&, const WebCore::StickyPositionViewportConstraints&);
    static bool decode(Decoder&, WebCore::StickyPositionViewportConstraints&);
};

#if !USE(COORDINATED_GRAPHICS)
template<> struct ArgumentCoder<WebCore::FilterOperations> {
    static void encode(Encoder&, const WebCore::FilterOperations&);
    static bool decode(Decoder&, WebCore::FilterOperations&);
};
    
template<> struct ArgumentCoder<WebCore::FilterOperation> {
    static void encode(Encoder&, const WebCore::FilterOperation&);
};
bool decodeFilterOperation(Decoder&, RefPtr<WebCore::FilterOperation>&);
#endif

template<> struct ArgumentCoder<WebCore::BlobPart> {
    static void encode(Encoder&, const WebCore::BlobPart&);
    static std::optional<WebCore::BlobPart> decode(Decoder&);
};

#if ENABLE(CONTENT_FILTERING)
template<> struct ArgumentCoder<WebCore::ContentFilterUnblockHandler> {
    static void encode(Encoder&, const WebCore::ContentFilterUnblockHandler&);
    static bool decode(Decoder&, WebCore::ContentFilterUnblockHandler&);
};
#endif

#if ENABLE(MEDIA_SESSION)
template<> struct ArgumentCoder<WebCore::MediaSessionMetadata> {
    static void encode(Encoder&, const WebCore::MediaSessionMetadata&);
    static bool decode(Decoder&, WebCore::MediaSessionMetadata&);
};
#endif

template<> struct ArgumentCoder<WebCore::TextIndicatorData> {
    static void encode(Encoder&, const WebCore::TextIndicatorData&);
    static std::optional<WebCore::TextIndicatorData> decode(Decoder&);
};

template<> struct ArgumentCoder<WebCore::DictionaryPopupInfo> {
    static void encode(Encoder&, const WebCore::DictionaryPopupInfo&);
    static bool decode(Decoder&, WebCore::DictionaryPopupInfo&);
};

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
template<> struct ArgumentCoder<WebCore::MediaPlaybackTargetContext> {
    static void encode(Encoder&, const WebCore::MediaPlaybackTargetContext&);
    static bool decode(Decoder&, WebCore::MediaPlaybackTargetContext&);
    static void encodePlatformData(Encoder&, const WebCore::MediaPlaybackTargetContext&);
    static bool decodePlatformData(Decoder&, WebCore::MediaPlaybackTargetContext&);
};
#endif

template<> struct ArgumentCoder<WebCore::RecentSearch> {
    static void encode(Encoder&, const WebCore::RecentSearch&);
    static std::optional<WebCore::RecentSearch> decode(Decoder&);
};

template<> struct ArgumentCoder<WebCore::ExceptionDetails> {
    static void encode(Encoder&, const WebCore::ExceptionDetails&);
    static bool decode(Decoder&, WebCore::ExceptionDetails&);
};

template<> struct ArgumentCoder<WebCore::ResourceLoadStatistics> {
    static void encode(Encoder&, const WebCore::ResourceLoadStatistics&);
    static std::optional<WebCore::ResourceLoadStatistics> decode(Decoder&);
};

#if ENABLE(APPLE_PAY)

template<> struct ArgumentCoder<WebCore::Payment> {
    static void encode(Encoder&, const WebCore::Payment&);
    static bool decode(Decoder&, WebCore::Payment&);
};

template<> struct ArgumentCoder<WebCore::PaymentAuthorizationResult> {
    static void encode(Encoder&, const WebCore::PaymentAuthorizationResult&);
    static std::optional<WebCore::PaymentAuthorizationResult> decode(Decoder&);
};

template<> struct ArgumentCoder<WebCore::PaymentContact> {
    static void encode(Encoder&, const WebCore::PaymentContact&);
    static bool decode(Decoder&, WebCore::PaymentContact&);
};

template<> struct ArgumentCoder<WebCore::PaymentError> {
    static void encode(Encoder&, const WebCore::PaymentError&);
    static std::optional<WebCore::PaymentError> decode(Decoder&);
};

template<> struct ArgumentCoder<WebCore::PaymentMerchantSession> {
    static void encode(Encoder&, const WebCore::PaymentMerchantSession&);
    static bool decode(Decoder&, WebCore::PaymentMerchantSession&);
};

template<> struct ArgumentCoder<WebCore::PaymentMethod> {
    static void encode(Encoder&, const WebCore::PaymentMethod&);
    static bool decode(Decoder&, WebCore::PaymentMethod&);
};

template<> struct ArgumentCoder<WebCore::PaymentMethodUpdate> {
    static void encode(Encoder&, const WebCore::PaymentMethodUpdate&);
    static std::optional<WebCore::PaymentMethodUpdate> decode(Decoder&);
};

template<> struct ArgumentCoder<WebCore::ApplePaySessionPaymentRequest> {
    static void encode(Encoder&, const WebCore::ApplePaySessionPaymentRequest&);
    static bool decode(Decoder&, WebCore::ApplePaySessionPaymentRequest&);
};

template<> struct ArgumentCoder<WebCore::ApplePaySessionPaymentRequest::ContactFields> {
    static void encode(Encoder&, const WebCore::ApplePaySessionPaymentRequest::ContactFields&);
    static bool decode(Decoder&, WebCore::ApplePaySessionPaymentRequest::ContactFields&);
};

template<> struct ArgumentCoder<WebCore::ApplePaySessionPaymentRequest::LineItem> {
    static void encode(Encoder&, const WebCore::ApplePaySessionPaymentRequest::LineItem&);
    static std::optional<WebCore::ApplePaySessionPaymentRequest::LineItem> decode(Decoder&);
};

template<> struct ArgumentCoder<WebCore::ApplePaySessionPaymentRequest::MerchantCapabilities> {
    static void encode(Encoder&, const WebCore::ApplePaySessionPaymentRequest::MerchantCapabilities&);
    static bool decode(Decoder&, WebCore::ApplePaySessionPaymentRequest::MerchantCapabilities&);
};

template<> struct ArgumentCoder<WebCore::ApplePaySessionPaymentRequest::ShippingMethod> {
    static void encode(Encoder&, const WebCore::ApplePaySessionPaymentRequest::ShippingMethod&);
    static std::optional<WebCore::ApplePaySessionPaymentRequest::ShippingMethod> decode(Decoder&);
};

template<> struct ArgumentCoder<WebCore::ApplePaySessionPaymentRequest::TotalAndLineItems> {
    static void encode(Encoder&, const WebCore::ApplePaySessionPaymentRequest::TotalAndLineItems&);
    static std::optional<WebCore::ApplePaySessionPaymentRequest::TotalAndLineItems> decode(Decoder&);
};

template<> struct ArgumentCoder<WebCore::ShippingContactUpdate> {
    static void encode(Encoder&, const WebCore::ShippingContactUpdate&);
    static std::optional<WebCore::ShippingContactUpdate> decode(Decoder&);
};

template<> struct ArgumentCoder<WebCore::ShippingMethodUpdate> {
    static void encode(Encoder&, const WebCore::ShippingMethodUpdate&);
    static std::optional<WebCore::ShippingMethodUpdate> decode(Decoder&);
};

#endif

#if ENABLE(MEDIA_STREAM)
template<> struct ArgumentCoder<WebCore::MediaConstraints> {
    static void encode(Encoder&, const WebCore::MediaConstraints&);
    static bool decode(Decoder&, WebCore::MediaConstraints&);
};
#endif

#if ENABLE(INDEXED_DATABASE)

template<> struct ArgumentCoder<WebCore::IDBKeyPath> {
    static void encode(Encoder&, const WebCore::IDBKeyPath&);
    static bool decode(Decoder&, WebCore::IDBKeyPath&);
};

#endif

#if ENABLE(SERVICE_WORKER)

template<> struct ArgumentCoder<WebCore::ServiceWorkerOrClientData> {
    static void encode(Encoder&, const WebCore::ServiceWorkerOrClientData&);
    static bool decode(Decoder&, WebCore::ServiceWorkerOrClientData&);
};

template<> struct ArgumentCoder<WebCore::ServiceWorkerOrClientIdentifier> {
    static void encode(Encoder&, const WebCore::ServiceWorkerOrClientIdentifier&);
    static bool decode(Decoder&, WebCore::ServiceWorkerOrClientIdentifier&);
};

#endif

#if ENABLE(CSS_SCROLL_SNAP)

template<> struct ArgumentCoder<WebCore::ScrollOffsetRange<float>> {
    static void encode(Encoder&, const WebCore::ScrollOffsetRange<float>&);
    static std::optional<WebCore::ScrollOffsetRange<float>> decode(Decoder&);
};

#endif

template<> struct ArgumentCoder<WebCore::MediaSelectionOption> {
    static void encode(Encoder&, const WebCore::MediaSelectionOption&);
    static std::optional<WebCore::MediaSelectionOption> decode(Decoder&);
};

template<> struct ArgumentCoder<WebCore::PromisedAttachmentInfo> {
    static void encode(Encoder&, const WebCore::PromisedAttachmentInfo&);
    static bool decode(Decoder&, WebCore::PromisedAttachmentInfo&);
};

template<> struct ArgumentCoder<Vector<RefPtr<WebCore::SecurityOrigin>>> {
    static void encode(Encoder&, const Vector<RefPtr<WebCore::SecurityOrigin>>&);
    static bool decode(Decoder&, Vector<RefPtr<WebCore::SecurityOrigin>>&);
};

template<> struct ArgumentCoder<WebCore::FontAttributes> {
    static void encode(Encoder&, const WebCore::FontAttributes&);
    static std::optional<WebCore::FontAttributes> decode(Decoder&);
};

} // namespace IPC

namespace WTF {

template<> struct EnumTraits<WebCore::ColorSpace> {
    using values = EnumValues<
    WebCore::ColorSpace,
    WebCore::ColorSpace::ColorSpaceSRGB,
    WebCore::ColorSpace::ColorSpaceLinearRGB,
    WebCore::ColorSpace::ColorSpaceDisplayP3
    >;
};

template<> struct EnumTraits<WebCore::HasInsecureContent> {
    using values = EnumValues<
        WebCore::HasInsecureContent,
        WebCore::HasInsecureContent::No,
        WebCore::HasInsecureContent::Yes
    >;
};

template<> struct EnumTraits<WebCore::AutoplayEvent> {
    using values = EnumValues<
        WebCore::AutoplayEvent,
        WebCore::AutoplayEvent::DidPreventMediaFromPlaying,
        WebCore::AutoplayEvent::DidPlayMediaPreventedFromPlaying,
        WebCore::AutoplayEvent::DidAutoplayMediaPastThresholdWithoutUserInterference,
        WebCore::AutoplayEvent::UserDidInterfereWithPlayback
    >;
};

template<> struct EnumTraits<WebCore::ShouldSample> {
    using values = EnumValues<
        WebCore::ShouldSample,
        WebCore::ShouldSample::No,
        WebCore::ShouldSample::Yes
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

#if ENABLE(INDEXED_DATABASE)
template<> struct EnumTraits<WebCore::IndexedDB::GetAllType> {
    using values = EnumValues<
        WebCore::IndexedDB::GetAllType,
        WebCore::IndexedDB::GetAllType::Keys,
        WebCore::IndexedDB::GetAllType::Values
    >;
};
#endif

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

template <> struct EnumTraits<WebCore::StoredCredentialsPolicy> {
    using values = EnumValues<
        WebCore::StoredCredentialsPolicy,
        WebCore::StoredCredentialsPolicy::DoNotUse,
        WebCore::StoredCredentialsPolicy::Use
    >;
};

template <> struct EnumTraits<WebCore::WorkerType> {
    using values = EnumValues<
        WebCore::WorkerType,
        WebCore::WorkerType::Classic,
        WebCore::WorkerType::Module
    >;
};

} // namespace WTF
