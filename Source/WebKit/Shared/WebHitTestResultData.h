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
    enum class IsScrollbar { No, Vertical, Horizontal };
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
    RetainPtr<DDActionContext> detectedDataActionContext;
#endif
    WebCore::FloatRect detectedDataBoundingBox;
    RefPtr<WebCore::TextIndicator> detectedDataTextIndicator;
    WebCore::PageOverlay::PageOverlayID detectedDataOriginatingPageOverlay;

    WebCore::DictionaryPopupInfo dictionaryPopupInfo;

    RefPtr<WebCore::TextIndicator> linkTextIndicator;

    WebHitTestResultData();
    WebHitTestResultData(const WebCore::HitTestResult&, const String& toolTipText);
    WebHitTestResultData(const WebCore::HitTestResult&, bool includeImage);
    ~WebHitTestResultData();

    void encode(IPC::Encoder&) const;
    void platformEncode(IPC::Encoder&) const;
    static WARN_UNUSED_RETURN bool decode(IPC::Decoder&, WebHitTestResultData&);
    static WARN_UNUSED_RETURN bool platformDecode(IPC::Decoder&, WebHitTestResultData&);

    WebCore::IntRect elementBoundingBoxInWindowCoordinates(const WebCore::HitTestResult&);
};

} // namespace WebKit

namespace WTF {

template<> struct EnumTraits<WebKit::WebHitTestResultData::IsScrollbar> {
    using values = EnumValues<
    WebKit::WebHitTestResultData::IsScrollbar,
    WebKit::WebHitTestResultData::IsScrollbar::No,
    WebKit::WebHitTestResultData::IsScrollbar::Vertical,
    WebKit::WebHitTestResultData::IsScrollbar::Horizontal
    >;
};

} // namespace WTF
