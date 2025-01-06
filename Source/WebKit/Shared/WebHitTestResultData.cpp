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
#include "WebFrame.h"
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
#include <WebCore/Text.h>
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

static RefPtr<WebFrame> webFrameFromHitTestResult(const HitTestResult& hitTestResult)
{
    RefPtr coreFrame = hitTestResult.frame();
    if (!coreFrame)
        return nullptr;

    return WebFrame::fromCoreFrame(*coreFrame);
}

static String linkLocalDataMIMETypeFromHitTestResult(const HitTestResult& hitTestResult)
{
    if (!hitTestResult.hasLocalDataForLinkURL())
        return nullString();

    RefPtr webFrame = webFrameFromHitTestResult(hitTestResult);
    if (!webFrame)
        return nullString();

    return webFrame->mimeTypeForResourceWithURL(hitTestResult.absoluteLinkURL());
}

static String imageSuggestedFilenameFromHitTestResult(const HitTestResult& hitTestResult)
{
    if (!hitTestResult.hasEntireImage())
        return nullString();

    RefPtr webFrame = webFrameFromHitTestResult(hitTestResult);
    if (!webFrame)
        return nullString();

    return webFrame->suggestedFilenameForResourceWithURL(hitTestResult.absoluteImageURL());
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
    , isTextNode(is<Text>(hitTestResult.innerNode()))
    , isOverTextInsideFormControlElement(hitTestResult.isOverTextInsideFormControlElement())
    , isDownloadableMedia(hitTestResult.isDownloadableMedia())
    , mediaIsInFullscreen(hitTestResult.mediaIsInFullscreen())
    , isActivePDFAnnotation(false)
    , elementType(ElementType::None)
    , frameInfo(frameInfoDataFromHitTestResult(hitTestResult))
    , toolTipText(toolTipText)
    , hasLocalDataForLinkURL(hitTestResult.hasLocalDataForLinkURL())
    , hasEntireImage(hitTestResult.hasEntireImage())
{
    if (auto* scrollbar = hitTestResult.scrollbar())
        isScrollbar = scrollbar->orientation() == ScrollbarOrientation::Horizontal ? IsScrollbar::Horizontal : IsScrollbar::Vertical;

    elementType = elementTypeFromHitTestResult(hitTestResult);
    linkLocalDataMIMEType = linkLocalDataMIMETypeFromHitTestResult(hitTestResult);
    imageSuggestedFilename = imageSuggestedFilenameFromHitTestResult(hitTestResult);
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
    , isTextNode(is<Text>(hitTestResult.innerNode()))
    , isOverTextInsideFormControlElement(hitTestResult.isOverTextInsideFormControlElement())
    , isDownloadableMedia(hitTestResult.isDownloadableMedia())
    , mediaIsInFullscreen(hitTestResult.mediaIsInFullscreen())
    , isActivePDFAnnotation(false)
    , elementType(ElementType::None)
    , frameInfo(frameInfoDataFromHitTestResult(hitTestResult))
    , hasLocalDataForLinkURL(hitTestResult.hasLocalDataForLinkURL())
    , hasEntireImage(hitTestResult.hasEntireImage())
{
    if (auto* scrollbar = hitTestResult.scrollbar())
        isScrollbar = scrollbar->orientation() == ScrollbarOrientation::Horizontal ? IsScrollbar::Horizontal : IsScrollbar::Vertical;

    elementType = elementTypeFromHitTestResult(hitTestResult);
    linkLocalDataMIMEType = linkLocalDataMIMETypeFromHitTestResult(hitTestResult);
    imageSuggestedFilename = imageSuggestedFilenameFromHitTestResult(hitTestResult);

    if (!includeImage)
        return;

    if (Image* image = hitTestResult.image()) {
        RefPtr<FragmentedSharedBuffer> buffer = image->data();
        if (buffer)
            imageSharedMemory = WebCore::SharedMemory::copyBuffer(*buffer);
    }

    if (RefPtr target = hitTestResult.innerNonSharedNode()) {
        if (auto renderer = dynamicDowncast<RenderImage>(target->renderer())) {
            imageBitmap = createShareableBitmap(*renderer);
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

WebHitTestResultData::WebHitTestResultData(const String& absoluteImageURL, const String& absolutePDFURL, const String& absoluteLinkURL, const String& absoluteMediaURL, const String& linkLabel, const String& linkTitle, const String& linkSuggestedFilename, const String& imageSuggestedFilename, bool isContentEditable, const WebCore::IntRect& elementBoundingBox, const WebKit::WebHitTestResultData::IsScrollbar& isScrollbar, bool isSelected, bool isTextNode, bool isOverTextInsideFormControlElement, bool isDownloadableMedia, bool mediaIsInFullscreen, bool isActivePDFAnnotation, const WebHitTestResultData::ElementType& elementType, std::optional<FrameInfoData>&& frameInfo, std::optional<WebCore::RemoteUserInputEventData> remoteUserInputEventData, const String& lookupText, const String& toolTipText, const String& imageText, std::optional<WebCore::SharedMemory::Handle>&& imageHandle, const RefPtr<WebCore::ShareableBitmap>& imageBitmap, const String& sourceImageMIMEType, const String& linkLocalDataMIMEType, bool hasLocalDataForLinkURL, bool hasEntireImage,
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
        , imageSuggestedFilename(imageSuggestedFilename)
        , isContentEditable(isContentEditable)
        , elementBoundingBox(elementBoundingBox)
        , isScrollbar(isScrollbar)
        , isSelected(isSelected)
        , isTextNode(isTextNode)
        , isOverTextInsideFormControlElement(isOverTextInsideFormControlElement)
        , isDownloadableMedia(isDownloadableMedia)
        , mediaIsInFullscreen(mediaIsInFullscreen)
        , isActivePDFAnnotation(isActivePDFAnnotation)
        , elementType(elementType)
        , frameInfo(frameInfo)
        , remoteUserInputEventData(remoteUserInputEventData)
        , lookupText(lookupText)
        , toolTipText(toolTipText)
        , imageText(imageText)
        , imageBitmap(imageBitmap)
        , sourceImageMIMEType(sourceImageMIMEType)
        , linkLocalDataMIMEType(linkLocalDataMIMEType)
        , hasLocalDataForLinkURL(hasLocalDataForLinkURL)
        , hasEntireImage(hasEntireImage)
#if PLATFORM(MAC)
        , platformData(platformData)
#endif
        , dictionaryPopupInfo(dictionaryPopupInfo)
        , linkTextIndicator(linkTextIndicator)
{
    if (imageHandle)
        imageSharedMemory = WebCore::SharedMemory::map(WTFMove(*imageHandle), WebCore::SharedMemory::Protection::ReadOnly);
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

std::optional<WebCore::SharedMemory::Handle> WebHitTestResultData::getImageSharedMemoryHandle() const
{
    std::optional<WebCore::SharedMemory::Handle> imageHandle = std::nullopt;
    if (imageSharedMemory && !imageSharedMemory->span().empty()) {
        if (auto handle = imageSharedMemory->createHandle(WebCore::SharedMemory::Protection::ReadOnly))
            imageHandle = WTFMove(*handle);
    }
    return imageHandle;
}

std::optional<FrameInfoData> WebHitTestResultData::frameInfoDataFromHitTestResult(const WebCore::HitTestResult& hitTestResult)
{
    RefPtr webFrame = webFrameFromHitTestResult(hitTestResult);
    if (!webFrame)
        return std::nullopt;

    return webFrame->info();
}

} // WebKit
