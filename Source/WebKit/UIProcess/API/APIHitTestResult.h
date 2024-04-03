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
#include "WebHitTestResultData.h"
#include "WebPageProxy.h"
#include <WebCore/DictionaryPopupInfo.h>
#include <WebCore/FloatPoint.h>
#include <WebCore/IntRect.h>
#include <WebCore/PageOverlay.h>
#include <WebCore/SharedMemory.h>
#include <wtf/Forward.h>
#include <wtf/RefPtr.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/WTFString.h>

namespace IPC {
class Decoder;
class Encoder;
}

namespace WebCore {
class HitTestResult;
}

namespace API {

class WebFrame;

class HitTestResult : public API::ObjectImpl<API::Object::Type::HitTestResult> {
public:
    static Ref<HitTestResult> create(const WebKit::WebHitTestResultData&, WebKit::WebPageProxy*);

    WTF::String absoluteImageURL() const { return m_data.absoluteImageURL; }
    WTF::String absolutePDFURL() const { return m_data.absolutePDFURL; }
    WTF::String absoluteLinkURL() const { return m_data.absoluteLinkURL; }
    WTF::String absoluteMediaURL() const { return m_data.absoluteMediaURL; }

    WTF::String linkLabel() const { return m_data.linkLabel; }
    WTF::String linkTitle() const { return m_data.linkTitle; }
    WTF::String linkLocalDataMIMEType() const { return m_data.linkLocalDataMIMEType; }
    WTF::String linkSuggestedFilename() const { return m_data.linkSuggestedFilename; }
    WTF::String imageSuggestedFilename() const { return m_data.imageSuggestedFilename; }
    WTF::String lookupText() const { return m_data.lookupText; }
    WTF::String sourceImageMIMEType() const { return m_data.sourceImageMIMEType; }

    bool isContentEditable() const { return m_data.isContentEditable; }

    WebCore::IntRect elementBoundingBox() const { return m_data.elementBoundingBox; }

    bool isScrollbar() const { return m_data.isScrollbar != WebKit::WebHitTestResultData::IsScrollbar::No; }

    bool isSelected() const { return m_data.isSelected; }

    bool isTextNode() const { return m_data.isTextNode; }

    bool isOverTextInsideFormControlElement() const { return m_data.isOverTextInsideFormControlElement; }

    bool isDownloadableMedia() const { return m_data.isDownloadableMedia; }

    bool mediaIsInFullscreen() const { return m_data.mediaIsInFullscreen; }

    WebKit::WebHitTestResultData::ElementType elementType() const { return m_data.elementType; }

    WebKit::WebPageProxy* page() { return m_page.get(); }

    const std::optional<WebKit::FrameInfoData>& frameInfo() const { return m_data.frameInfo; }

    bool hasLocalDataForLinkURL() const { return m_data.hasLocalDataForLinkURL; }

private:
    explicit HitTestResult(const WebKit::WebHitTestResultData& hitTestResultData, WebKit::WebPageProxy* page)
        : m_data(hitTestResultData)
        , m_page(page)
    {
    }

    WebKit::WebHitTestResultData m_data;
    WeakPtr<WebKit::WebPageProxy> m_page;
};

} // namespace API
