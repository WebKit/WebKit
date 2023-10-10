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

#include "ShareableBitmapUtilities.h"
#include "WebCoreArgumentCoders.h"
#include <WebCore/Document.h>
#include <WebCore/ElementInlines.h>
#include <WebCore/EventHandler.h>
#include <WebCore/HitTestResult.h>
#include <WebCore/LocalFrame.h>
#include <WebCore/LocalFrameView.h>
#include <WebCore/NavigationAction.h>
#include <WebCore/Node.h>
#include <WebCore/RenderImage.h>
#include <WebCore/SharedBuffer.h>
#include <wtf/URL.h>
#include <wtf/text/WTFString.h>

namespace WebKit {
using namespace WebCore;

static WebHitTestResultData::ElementType elementTypeFromHitTestResult(const HitTestResult& hitTestResult)
{
    if (!hitTestResult.hasMediaElement())
        return WebHitTestResultData::ElementType::None;

    return hitTestResult.mediaIsVideo() ? WebHitTestResultData::ElementType::Video : WebHitTestResultData::ElementType::Audio;
}

WebHitTestResultData::WebHitTestResultData()
{
}

WebHitTestResultData::WebHitTestResultData(const HitTestResult& hitTestResult, const String& toolTipText)
    : absoluteImageURL(hitTestResult.absoluteImageURL().string())
    , absolutePDFURL(hitTestResult.absolutePDFURL().string())
    , absoluteLinkURL(hitTestResult.absoluteLinkURL().string())
    , absoluteMediaURL(hitTestResult.absoluteMediaURL().string())
    , linkLabel(hitTestResult.textContent())
    , linkTitle(hitTestResult.titleDisplayString())
    , linkSuggestedFilename(hitTestResult.linkSuggestedFilename())
    , isContentEditable(hitTestResult.isContentEditable())
    , elementBoundingBox(elementBoundingBoxInWindowCoordinates(hitTestResult))
    , isScrollbar(IsScrollbar::No)
    , isSelected(hitTestResult.isSelected())
    , isTextNode(hitTestResult.innerNode() && hitTestResult.innerNode()->isTextNode())
    , isOverTextInsideFormControlElement(hitTestResult.isOverTextInsideFormControlElement())
    , isDownloadableMedia(hitTestResult.isDownloadableMedia())
    , elementType(ElementType::None)
    , toolTipText(toolTipText)
{
    if (auto* scrollbar = hitTestResult.scrollbar())
        isScrollbar = scrollbar->orientation() == ScrollbarOrientation::Horizontal ? IsScrollbar::Horizontal : IsScrollbar::Vertical;

    elementType = elementTypeFromHitTestResult(hitTestResult);
}

WebHitTestResultData::WebHitTestResultData(const HitTestResult& hitTestResult, bool includeImage)
    : absoluteImageURL(hitTestResult.absoluteImageURL().string())
    , absolutePDFURL(hitTestResult.absolutePDFURL().string())
    , absoluteLinkURL(hitTestResult.absoluteLinkURL().string())
    , absoluteMediaURL(hitTestResult.absoluteMediaURL().string())
    , linkLabel(hitTestResult.textContent())
    , linkTitle(hitTestResult.titleDisplayString())
    , linkSuggestedFilename(hitTestResult.linkSuggestedFilename())
    , isContentEditable(hitTestResult.isContentEditable())
    , elementBoundingBox(elementBoundingBoxInWindowCoordinates(hitTestResult))
    , isScrollbar(IsScrollbar::No)
    , isSelected(hitTestResult.isSelected())
    , isTextNode(hitTestResult.innerNode() && hitTestResult.innerNode()->isTextNode())
    , isOverTextInsideFormControlElement(hitTestResult.isOverTextInsideFormControlElement())
    , isDownloadableMedia(hitTestResult.isDownloadableMedia())
    , elementType(ElementType::None)
{
    if (auto* scrollbar = hitTestResult.scrollbar())
        isScrollbar = scrollbar->orientation() == ScrollbarOrientation::Horizontal ? IsScrollbar::Horizontal : IsScrollbar::Vertical;

    elementType = elementTypeFromHitTestResult(hitTestResult);

    if (!includeImage)
        return;

    if (Image* image = hitTestResult.image()) {
        RefPtr<FragmentedSharedBuffer> buffer = image->data();
        if (buffer)
            imageSharedMemory = WebKit::SharedMemory::copyBuffer(*buffer);
    }

    if (auto target = RefPtr { hitTestResult.innerNonSharedNode() }) {
        if (auto renderer = dynamicDowncast<RenderImage>(target->renderer())) {
            imageBitmap = createShareableBitmap(*downcast<RenderImage>(target->renderer()));
            if (auto* cachedImage = renderer->cachedImage()) {
                if (auto* image = cachedImage->image())
                    sourceImageMIMEType = image->mimeType();
            }

            imageText = [&]() -> String {
                if (RefPtr element = dynamicDowncast<Element>(target.get())) {
                    auto& title = element->attributeWithoutSynchronization(HTMLNames::titleAttr);
                    if (!title.isEmpty())
                        return title;
                }

                return renderer->altText();
            }();
        }
    }
}

WebHitTestResultData::WebHitTestResultData(const String& absoluteImageURL, const String& absolutePDFURL, const String& absoluteLinkURL, const String& absoluteMediaURL, const String& linkLabel, const String& linkTitle, const String& linkSuggestedFilename, bool isContentEditable, const WebCore::IntRect& elementBoundingBox, const WebKit::WebHitTestResultData::IsScrollbar& isScrollbar, bool isSelected, bool isTextNode, bool isOverTextInsideFormControlElement, bool isDownloadableMedia, const WebHitTestResultData::ElementType& elementType, const String& lookupText, const String& toolTipText, const String& imageText, std::optional<WebKit::SharedMemory::Handle>&& imageHandle, const RefPtr<WebKit::ShareableBitmap>& imageBitmap, const String& sourceImageMIMEType,
#if PLATFORM(MAC)
    const WebHitTestResultPlatformData& platformData,
#endif
    const WebCore::DictionaryPopupInfo& dictionaryPopupInfo, const RefPtr<WebCore::TextIndicator>& linkTextIndicator)
        : absoluteImageURL(absoluteImageURL)
        , absolutePDFURL(absolutePDFURL)
        , absoluteLinkURL(absoluteLinkURL)
        , absoluteMediaURL(absoluteMediaURL)
        , linkLabel(linkLabel)
        , linkTitle(linkTitle)
        , linkSuggestedFilename(linkSuggestedFilename)
        , isContentEditable(isContentEditable)
        , elementBoundingBox(elementBoundingBox)
        , isScrollbar(isScrollbar)
        , isSelected(isSelected)
        , isTextNode(isTextNode)
        , isOverTextInsideFormControlElement(isOverTextInsideFormControlElement)
        , isDownloadableMedia(isDownloadableMedia)
        , elementType(elementType)
        , lookupText(lookupText)
        , toolTipText(toolTipText)
        , imageText(imageText)
        , imageBitmap(imageBitmap)
        , sourceImageMIMEType(sourceImageMIMEType)
#if PLATFORM(MAC)
        , platformData(platformData)
#endif
        , dictionaryPopupInfo(dictionaryPopupInfo)
        , linkTextIndicator(linkTextIndicator)
{
    if (imageHandle)
        imageSharedMemory = WebKit::SharedMemory::map(WTFMove(*imageHandle), WebKit::SharedMemory::Protection::ReadOnly);
}

WebHitTestResultData::~WebHitTestResultData()
{
}

IntRect WebHitTestResultData::elementBoundingBoxInWindowCoordinates(const WebCore::HitTestResult& hitTestResult)
{
    RefPtr node = hitTestResult.innerNonSharedNode();
    if (!node)
        return IntRect();

    RefPtr frame = node->document().frame();
    if (!frame)
        return IntRect();

    RefPtr view = frame->view();
    if (!view)
        return IntRect();

    auto* renderer = node->renderer();
    if (!renderer)
        return IntRect();

    return view->contentsToWindow(renderer->absoluteBoundingBoxRect());
}

std::optional<WebKit::SharedMemory::Handle> WebHitTestResultData::getImageSharedMemoryHandle() const
{
    std::optional<WebKit::SharedMemory::Handle> imageHandle = std::nullopt;
    if (imageSharedMemory && imageSharedMemory->data()) {
        if (auto handle = imageSharedMemory->createHandle(WebKit::SharedMemory::Protection::ReadOnly))
            imageHandle = WTFMove(*handle);
    }
    return imageHandle;
}

#if PLATFORM(MAC) || HAVE(UIKIT_WITH_MOUSE_SUPPORT)
std::optional<WebKit::WebHitTestResultData> WebHitTestResultData::fromNavigationActionAndLocalFrame(const NavigationAction& navigationAction, WebCore::LocalFrame* coreFrame)
{
    if (!coreFrame)
        return std::nullopt;

    auto mouseEventData = navigationAction.mouseEventData();
    if (!mouseEventData)
        return std::nullopt;

    constexpr OptionSet<HitTestRequest::Type> hitType { HitTestRequest::Type::ReadOnly, HitTestRequest::Type::Active, HitTestRequest::Type::DisallowUserAgentShadowContent, HitTestRequest::Type::AllowChildFrameContent };
    HitTestResult hitTestResult = coreFrame->eventHandler().hitTestResultAtPoint(mouseEventData->absoluteLocation, hitType);

    return WebKit::WebHitTestResultData(hitTestResult, false);
}
#endif

} // WebKit
