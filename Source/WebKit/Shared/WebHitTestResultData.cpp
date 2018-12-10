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

#include "config.h"
#include "WebHitTestResultData.h"

#include "WebCoreArgumentCoders.h"
#include <WebCore/Document.h>
#include <WebCore/Frame.h>
#include <WebCore/FrameView.h>
#include <WebCore/HitTestResult.h>
#include <WebCore/Node.h>
#include <WebCore/RenderObject.h>
#include <WebCore/SharedBuffer.h>
#include <wtf/URL.h>
#include <wtf/text/WTFString.h>

namespace WebKit {
using namespace WebCore;

WebHitTestResultData::WebHitTestResultData()
{
}

WebHitTestResultData::WebHitTestResultData(const WebCore::HitTestResult& hitTestResult)
    : absoluteImageURL(hitTestResult.absoluteImageURL().string())
    , absolutePDFURL(hitTestResult.absolutePDFURL().string())
    , absoluteLinkURL(hitTestResult.absoluteLinkURL().string())
    , absoluteMediaURL(hitTestResult.absoluteMediaURL().string())
    , linkLabel(hitTestResult.textContent())
    , linkTitle(hitTestResult.titleDisplayString())
    , linkSuggestedFilename(hitTestResult.linkSuggestedFilename())
    , isContentEditable(hitTestResult.isContentEditable())
    , elementBoundingBox(elementBoundingBoxInWindowCoordinates(hitTestResult))
    , isScrollbar(hitTestResult.scrollbar())
    , isSelected(hitTestResult.isSelected())
    , isTextNode(hitTestResult.innerNode() && hitTestResult.innerNode()->isTextNode())
    , isOverTextInsideFormControlElement(hitTestResult.isOverTextInsideFormControlElement())
    , isDownloadableMedia(hitTestResult.isDownloadableMedia())
    , imageSize(0)
{
}

WebHitTestResultData::WebHitTestResultData(const WebCore::HitTestResult& hitTestResult, bool includeImage)
    : absoluteImageURL(hitTestResult.absoluteImageURL().string())
    , absolutePDFURL(hitTestResult.absolutePDFURL().string())
    , absoluteLinkURL(hitTestResult.absoluteLinkURL().string())
    , absoluteMediaURL(hitTestResult.absoluteMediaURL().string())
    , linkLabel(hitTestResult.textContent())
    , linkTitle(hitTestResult.titleDisplayString())
    , linkSuggestedFilename(hitTestResult.linkSuggestedFilename())
    , isContentEditable(hitTestResult.isContentEditable())
    , elementBoundingBox(elementBoundingBoxInWindowCoordinates(hitTestResult))
    , isScrollbar(hitTestResult.scrollbar())
    , isSelected(hitTestResult.isSelected())
    , isTextNode(hitTestResult.innerNode() && hitTestResult.innerNode()->isTextNode())
    , isOverTextInsideFormControlElement(hitTestResult.isOverTextInsideFormControlElement())
    , isDownloadableMedia(hitTestResult.isDownloadableMedia())
    , imageSize(0)
{
    if (!includeImage)
        return;

    if (Image* image = hitTestResult.image()) {
        RefPtr<SharedBuffer> buffer = image->data();
        if (buffer) {
            imageSharedMemory = WebKit::SharedMemory::allocate(buffer->size());
            memcpy(imageSharedMemory->data(), buffer->data(), buffer->size());
            imageSize = buffer->size();
        }
    }
}

WebHitTestResultData::~WebHitTestResultData()
{
}

void WebHitTestResultData::encode(IPC::Encoder& encoder) const
{
    encoder << absoluteImageURL;
    encoder << absolutePDFURL;
    encoder << absoluteLinkURL;
    encoder << absoluteMediaURL;
    encoder << linkLabel;
    encoder << linkTitle;
    encoder << linkSuggestedFilename;
    encoder << isContentEditable;
    encoder << elementBoundingBox;
    encoder << isScrollbar;
    encoder << isSelected;
    encoder << isTextNode;
    encoder << isOverTextInsideFormControlElement;
    encoder << isDownloadableMedia;
    encoder << lookupText;
    encoder << dictionaryPopupInfo;

    WebKit::SharedMemory::Handle imageHandle;
    if (imageSharedMemory && imageSharedMemory->data())
        imageSharedMemory->createHandle(imageHandle, WebKit::SharedMemory::Protection::ReadOnly);
    encoder << imageHandle;
    encoder << imageSize;

    bool hasLinkTextIndicator = linkTextIndicator;
    encoder << hasLinkTextIndicator;
    if (hasLinkTextIndicator)
        encoder << linkTextIndicator->data();

    platformEncode(encoder);
}

bool WebHitTestResultData::decode(IPC::Decoder& decoder, WebHitTestResultData& hitTestResultData)
{
    if (!decoder.decode(hitTestResultData.absoluteImageURL)
        || !decoder.decode(hitTestResultData.absolutePDFURL)
        || !decoder.decode(hitTestResultData.absoluteLinkURL)
        || !decoder.decode(hitTestResultData.absoluteMediaURL)
        || !decoder.decode(hitTestResultData.linkLabel)
        || !decoder.decode(hitTestResultData.linkTitle)
        || !decoder.decode(hitTestResultData.linkSuggestedFilename)
        || !decoder.decode(hitTestResultData.isContentEditable)
        || !decoder.decode(hitTestResultData.elementBoundingBox)
        || !decoder.decode(hitTestResultData.isScrollbar)
        || !decoder.decode(hitTestResultData.isSelected)
        || !decoder.decode(hitTestResultData.isTextNode)
        || !decoder.decode(hitTestResultData.isOverTextInsideFormControlElement)
        || !decoder.decode(hitTestResultData.isDownloadableMedia)
        || !decoder.decode(hitTestResultData.lookupText)
        || !decoder.decode(hitTestResultData.dictionaryPopupInfo))
        return false;

    WebKit::SharedMemory::Handle imageHandle;
    if (!decoder.decode(imageHandle))
        return false;

    if (!imageHandle.isNull())
        hitTestResultData.imageSharedMemory = WebKit::SharedMemory::map(imageHandle, WebKit::SharedMemory::Protection::ReadOnly);

    if (!decoder.decode(hitTestResultData.imageSize))
        return false;

    bool hasLinkTextIndicator;
    if (!decoder.decode(hasLinkTextIndicator))
        return false;

    if (hasLinkTextIndicator) {
        std::optional<WebCore::TextIndicatorData> indicatorData;
        decoder >> indicatorData;
        if (!indicatorData)
            return false;

        hitTestResultData.linkTextIndicator = WebCore::TextIndicator::create(*indicatorData);
    }

    return platformDecode(decoder, hitTestResultData);
}

#if !PLATFORM(MAC)
void WebHitTestResultData::platformEncode(IPC::Encoder& encoder) const
{
}

bool WebHitTestResultData::platformDecode(IPC::Decoder& decoder, WebHitTestResultData& hitTestResultData)
{
    return true;
}
#endif // !PLATFORM(MAC)

IntRect WebHitTestResultData::elementBoundingBoxInWindowCoordinates(const WebCore::HitTestResult& hitTestResult)
{
    Node* node = hitTestResult.innerNonSharedNode();
    if (!node)
        return IntRect();

    Frame* frame = node->document().frame();
    if (!frame)
        return IntRect();

    FrameView* view = frame->view();
    if (!view)
        return IntRect();

    RenderObject* renderer = node->renderer();
    if (!renderer)
        return IntRect();

    return view->contentsToWindow(renderer->absoluteBoundingBoxRect());
}

} // WebKit
