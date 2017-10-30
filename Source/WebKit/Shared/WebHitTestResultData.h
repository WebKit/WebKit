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

#ifndef WebHitTestResultData_h
#define WebHitTestResultData_h

#include "APIObject.h"
#include "SharedMemory.h"
#include <WebCore/DictionaryPopupInfo.h>
#include <WebCore/FloatPoint.h>
#include <WebCore/IntRect.h>
#include <WebCore/PageOverlay.h>
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
    bool isScrollbar;
    bool isSelected;
    bool isTextNode;
    bool isOverTextInsideFormControlElement;
    bool isDownloadableMedia;

    String lookupText;
    RefPtr<WebKit::SharedMemory> imageSharedMemory;
    uint64_t imageSize;

#if PLATFORM(MAC)
    RetainPtr<DDActionContext> detectedDataActionContext;
#endif
    WebCore::FloatRect detectedDataBoundingBox;
    RefPtr<WebCore::TextIndicator> detectedDataTextIndicator;
    WebCore::PageOverlay::PageOverlayID detectedDataOriginatingPageOverlay;

    WebCore::DictionaryPopupInfo dictionaryPopupInfo;

    RefPtr<WebCore::TextIndicator> linkTextIndicator;

    WebHitTestResultData();
    explicit WebHitTestResultData(const WebCore::HitTestResult&);
    WebHitTestResultData(const WebCore::HitTestResult&, bool includeImage);
    ~WebHitTestResultData();

    void encode(IPC::Encoder&) const;
    void platformEncode(IPC::Encoder&) const;
    static bool decode(IPC::Decoder&, WebHitTestResultData&);
    static bool platformDecode(IPC::Decoder&, WebHitTestResultData&);

    WebCore::IntRect elementBoundingBoxInWindowCoordinates(const WebCore::HitTestResult&);
};

} // namespace WebKit

#endif // WebHitTestResultData_h
