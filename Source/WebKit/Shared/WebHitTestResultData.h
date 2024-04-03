/*
 * Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#pragma once

#include "APIObject.h"
#include "FrameInfoData.h"
#include <WebCore/DictionaryPopupInfo.h>
#include <WebCore/FloatPoint.h>
#include <WebCore/IntRect.h>
#include <WebCore/PageOverlay.h>
#include <WebCore/ShareableBitmap.h>
#include <WebCore/SharedMemory.h>
#include <wtf/EnumTraits.h>
#include <wtf/Forward.h>
#include <wtf/RefPtr.h>
#include <wtf/text/WTFString.h>

#if HAVE(SECURE_ACTION_CONTEXT)
OBJC_CLASS DDSecureActionContext;
using WKDDActionContext = DDSecureActionContext;
#else
OBJC_CLASS DDActionContext;
using WKDDActionContext = DDActionContext;
#endif

namespace IPC {
class Decoder;
class Encoder;
}

namespace WebCore {
class HitTestResult;
class LocalFrame;
class NavigationAction;
}

namespace WebKit {

#if PLATFORM(MAC)
struct WebHitTestResultPlatformData {
    struct DetectedDataActionContext {
        RetainPtr<WKDDActionContext> context;
        struct MarkableTraits {
            static bool isEmptyValue(const DetectedDataActionContext& context) { return !context.context; }
            static DetectedDataActionContext emptyValue() { return { nullptr }; }
        };
    };
    Markable<DetectedDataActionContext> detectedDataActionContext;
    WebCore::FloatRect detectedDataBoundingBox;
    RefPtr<WebCore::TextIndicator> detectedDataTextIndicator;
    WebCore::PageOverlay::PageOverlayID detectedDataOriginatingPageOverlay;
};
#endif

struct WebHitTestResultData {
    String absoluteImageURL;
    String absolutePDFURL;
    String absoluteLinkURL;
    String absoluteMediaURL;
    String linkLabel;
    String linkTitle;
    String linkSuggestedFilename;
    String imageSuggestedFilename;
    bool isContentEditable;
    WebCore::IntRect elementBoundingBox;
    enum class IsScrollbar : uint8_t { No, Vertical, Horizontal };
    IsScrollbar isScrollbar;
    bool isSelected;
    bool isTextNode;
    bool isOverTextInsideFormControlElement;
    bool isDownloadableMedia;
    bool mediaIsInFullscreen;
    enum class ElementType : uint8_t { None, Audio, Video };
    ElementType elementType;
    std::optional<FrameInfoData> frameInfo;

    String lookupText;
    String toolTipText;
    String imageText;
    RefPtr<WebCore::SharedMemory> imageSharedMemory;
    RefPtr<WebCore::ShareableBitmap> imageBitmap;
    String sourceImageMIMEType;
    String linkLocalDataMIMEType;
    bool hasLocalDataForLinkURL;
    bool hasEntireImage;

#if PLATFORM(MAC)
    WebHitTestResultPlatformData platformData;
#endif
    
    WebCore::DictionaryPopupInfo dictionaryPopupInfo;

    RefPtr<WebCore::TextIndicator> linkTextIndicator;

    WebHitTestResultData();
    WebHitTestResultData(WebHitTestResultData&&) = default;
    WebHitTestResultData(const WebHitTestResultData&) = default;
    WebHitTestResultData& operator=(WebHitTestResultData&&) = default;
    WebHitTestResultData& operator=(const WebHitTestResultData&) = default;
    WebHitTestResultData(const WebCore::HitTestResult&, const String& toolTipText);
    WebHitTestResultData(const WebCore::HitTestResult&, bool includeImage);
    WebHitTestResultData(const String& absoluteImageURL, const String& absolutePDFURL, const String& absoluteLinkURL, const String& absoluteMediaURL, const String& linkLabel, const String& linkTitle, const String& linkSuggestedFilename, const String& imageSuggestedFilename, bool isContentEditable, const WebCore::IntRect& elementBoundingBox, const WebKit::WebHitTestResultData::IsScrollbar&, bool isSelected, bool isTextNode, bool isOverTextInsideFormControlElement, bool isDownloadableMedia, bool mediaIsInFullscreen, const WebKit::WebHitTestResultData::ElementType&, std::optional<FrameInfoData>&&, const String& lookupText, const String& toolTipText, const String& imageText, std::optional<WebCore::SharedMemory::Handle>&& imageHandle, const RefPtr<WebCore::ShareableBitmap>& imageBitmap, const String& sourceImageMIMEType, const String& linkLocalDataMIMEType, bool hasLocalDataForLinkURL, bool hasEntireImage,
#if PLATFORM(MAC)
        const WebHitTestResultPlatformData&,
#endif
        const WebCore::DictionaryPopupInfo&, const RefPtr<WebCore::TextIndicator>&);
    ~WebHitTestResultData();

    WebCore::IntRect elementBoundingBoxInWindowCoordinates(const WebCore::HitTestResult&);

    static std::optional<FrameInfoData> frameInfoDataFromHitTestResult(const WebCore::HitTestResult&);

    std::optional<WebCore::SharedMemory::Handle> getImageSharedMemoryHandle() const;

};

} // namespace WebKit
