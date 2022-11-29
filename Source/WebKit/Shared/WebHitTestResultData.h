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
#include "ShareableBitmap.h"
#include "SharedMemory.h"
#include <WebCore/DictionaryPopupInfo.h>
#include <WebCore/FloatPoint.h>
#include <WebCore/IntRect.h>
#include <WebCore/PageOverlay.h>
#include <wtf/EnumTraits.h>
#include <wtf/Forward.h>
#include <wtf/RefPtr.h>
#include <wtf/text/WTFString.h>

OBJC_CLASS DDActionContext;

namespace IPC {
class Decoder;
class Encoder;
}

namespace WebCore {
class HitTestResult;
}

namespace WebKit {

#if PLATFORM(MAC)
struct WebHitTestResultPlatformData {
    RetainPtr<DDActionContext> detectedDataActionContext;
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
    bool isContentEditable;
    WebCore::IntRect elementBoundingBox;
    enum class IsScrollbar : uint8_t { No, Vertical, Horizontal };
    IsScrollbar isScrollbar;
    bool isSelected;
    bool isTextNode;
    bool isOverTextInsideFormControlElement;
    bool isDownloadableMedia;

    String lookupText;
    String toolTipText;
    String imageText;
    RefPtr<SharedMemory> imageSharedMemory;
    RefPtr<ShareableBitmap> imageBitmap;
    String sourceImageMIMEType;

#if PLATFORM(MAC)
    WebHitTestResultPlatformData platformData;
#endif
    
    WebCore::DictionaryPopupInfo dictionaryPopupInfo;

    RefPtr<WebCore::TextIndicator> linkTextIndicator;

    WebHitTestResultData();
    WebHitTestResultData(const WebCore::HitTestResult&, const String& toolTipText);
    WebHitTestResultData(const WebCore::HitTestResult&, bool includeImage);
    WebHitTestResultData(const String& absoluteImageURL, const String& absolutePDFURL, const String& absoluteLinkURL, const String& absoluteMediaURL, const String& linkLabel, const String& linkTitle, const String& linkSuggestedFilename, bool isContentEditable, const WebCore::IntRect& elementBoundingBox, const WebKit::WebHitTestResultData::IsScrollbar&, bool isSelected, bool isTextNode, bool isOverTextInsideFormControlElement, bool isDownloadableMedia, const String& lookupText, const String& toolTipText, const String& imageText, const std::optional<WebKit::SharedMemory::Handle>& imageHandle, const RefPtr<WebKit::ShareableBitmap>& imageBitmap, const String& sourceImageMIMEType,
#if PLATFORM(MAC)
        const WebHitTestResultPlatformData&,
#endif
        const WebCore::DictionaryPopupInfo&, const RefPtr<WebCore::TextIndicator>&);
    ~WebHitTestResultData();

    WebCore::IntRect elementBoundingBoxInWindowCoordinates(const WebCore::HitTestResult&);

    std::optional<WebKit::SharedMemory::Handle> getImageSharedMemoryHandle() const;
};

} // namespace WebKit
