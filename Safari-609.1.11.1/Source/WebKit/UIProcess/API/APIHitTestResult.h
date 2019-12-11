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
#include "SharedMemory.h"
#include "WebHitTestResultData.h"
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

namespace API {

class WebFrame;

class HitTestResult : public API::ObjectImpl<API::Object::Type::HitTestResult> {
public:
    static Ref<HitTestResult> create(const WebKit::WebHitTestResultData&);

    WTF::String absoluteImageURL() const { return m_data.absoluteImageURL; }
    WTF::String absolutePDFURL() const { return m_data.absolutePDFURL; }
    WTF::String absoluteLinkURL() const { return m_data.absoluteLinkURL; }
    WTF::String absoluteMediaURL() const { return m_data.absoluteMediaURL; }

    WTF::String linkLabel() const { return m_data.linkLabel; }
    WTF::String linkTitle() const { return m_data.linkTitle; }
    WTF::String lookupText() const { return m_data.lookupText; }

    bool isContentEditable() const { return m_data.isContentEditable; }

    WebCore::IntRect elementBoundingBox() const { return m_data.elementBoundingBox; }

    bool isScrollbar() const { return m_data.isScrollbar; }

    bool isSelected() const { return m_data.isSelected; }

    bool isTextNode() const { return m_data.isTextNode; }

    bool isOverTextInsideFormControlElement() const { return m_data.isOverTextInsideFormControlElement; }

    bool isDownloadableMedia() const { return m_data.isDownloadableMedia; }

private:
    explicit HitTestResult(const WebKit::WebHitTestResultData& hitTestResultData)
        : m_data(hitTestResultData)
    {
    }

    WebKit::WebHitTestResultData m_data;
};

} // namespace API
