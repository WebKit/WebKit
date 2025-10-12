/*
 * Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2013-2025 Apple Inc. All rights reserved.
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
#include <WebCore/DocumentView.h>
#include <WebCore/ElementInlines.h>
#include <WebCore/EventHandler.h>
#include <WebCore/FrameDestructionObserverInlines.h>
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

static std::optional<ResourceResponse> linkLocalResourceFromHitTestResult(const HitTestResult& hitTestResult)
{
    if (!hitTestResult.hasLocalDataForLinkURL())
        return std::nullopt;

    RefPtr webFrame = webFrameFromHitTestResult(hitTestResult);
    if (!webFrame)
        return std::nullopt;

    return webFrame->resourceResponseForURL(hitTestResult.absoluteLinkURL());
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

WebHitTestResultData::WebHitTestResultData() = default;

WebHitTestResultData::WebHitTestResultData(const HitTestResult& hitTestResult, const String& tooltipText, bool includeImage)
    : absoluteImageURL(hitTestResult.absoluteImageURL().string())
    , absolutePDFURL(hitTestResult.absolutePDFURL().string())
    , absoluteLinkURL(hitTestResult.absoluteLinkURL().string())
    , absoluteMediaURL(hitTestResult.absoluteMediaURL().string())
    , linkLabel(hitTestResult.textContent())
    , linkTitle(hitTestResult.titleDisplayString())
    , linkSuggestedFilename(hitTestResult.linkSuggestedFilename())
    , imageSuggestedFilename(imageSuggestedFilenameFromHitTestResult(hitTestResult))
    , isContentEditable(hitTestResult.isContentEditable())
    , elementBoundingBox(elementBoundingBoxInWindowCoordinates(hitTestResult))
    , isScrollbar(IsScrollbar::No)
    , isSelected(hitTestResult.isSelected())
    , isTextNode(is<Text>(hitTestResult.innerNode()))
    , isOverTextInsideFormControlElement(hitTestResult.isOverTextInsideFormControlElement())
    , isDownloadableMedia(hitTestResult.isDownloadableMedia())
    , mediaIsInFullscreen(hitTestResult.mediaIsInFullscreen())
    , isActivePDFAnnotation(false)
    , elementType(elementTypeFromHitTestResult(hitTestResult))
    , frameInfo(frameInfoDataFromHitTestResult(hitTestResult))
    , targetFrame(hitTestResult.targetFrame() ? std::optional(hitTestResult.targetFrame()->frameID()) : std::nullopt)
    , tooltipText(tooltipText)
    , hasEntireImage(hitTestResult.hasEntireImage())
    , allowsFollowingLink(hitTestResult.allowsFollowingLink())
    , allowsFollowingImageURL(hitTestResult.allowsFollowingImageURL())
    , linkLocalResourceResponse(linkLocalResourceFromHitTestResult(hitTestResult))
{
    if (auto* scrollbar = hitTestResult.scrollbar())
        isScrollbar = scrollbar->orientation() == ScrollbarOrientation::Horizontal ? IsScrollbar::Horizontal : IsScrollbar::Vertical;

    if (!includeImage)
        return;

    if (RefPtr image = hitTestResult.image()) {
        if (RefPtr buffer = image->data())
            imageSharedMemory = WebCore::SharedMemory::copyBuffer(*buffer);
    }

    if (RefPtr target = hitTestResult.innerNonSharedNode()) {
        if (CheckedPtr renderer = dynamicDowncast<RenderImage>(target->renderer())) {
            imageBitmap = createShareableBitmap(*renderer);
            if (auto* cachedImage = renderer->cachedImage()) {
                if (RefPtr image = cachedImage->image())
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

WebHitTestResultData::WebHitTestResultData(const HitTestResult& hitTestResult, const String& tooltipText)
    : WebHitTestResultData(hitTestResult, tooltipText, false) { }

WebHitTestResultData::WebHitTestResultData(const HitTestResult& hitTestResult, bool includeImage)
    : WebHitTestResultData(hitTestResult, String(), includeImage) { }

WebHitTestResultData::WebHitTestResultData(const String& absoluteImageURL, const String& absolutePDFURL, const String& absoluteLinkURL, const String& absoluteMediaURL, const String& linkLabel, const String& linkTitle, const String& linkSuggestedFilename, const String& imageSuggestedFilename, bool isContentEditable, const WebCore::IntRect& elementBoundingBox, const WebKit::WebHitTestResultData::IsScrollbar& isScrollbar, bool isSelected, bool isTextNode, bool isOverTextInsideFormControlElement, bool isDownloadableMedia, bool mediaIsInFullscreen, bool isActivePDFAnnotation, const WebHitTestResultData::ElementType& elementType, std::optional<FrameInfoData>&& frameInfo, std::optional<WebCore::FrameIdentifier> targetFrame, std::optional<WebCore::RemoteUserInputEventData> remoteUserInputEventData, const String& lookupText, const String& tooltipText, const String& imageText, std::optional<WebCore::SharedMemory::Handle>&& imageHandle, const RefPtr<WebCore::ShareableBitmap>& imageBitmap, const String& sourceImageMIMEType, bool hasEntireImage, bool allowsFollowingLink, bool allowsFollowingImageURL, std::optional<WebCore::ResourceResponse>&& linkLocalResourceResponse,
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
        , frameInfo(WTFMove(frameInfo))
        , targetFrame(targetFrame)
        , remoteUserInputEventData(remoteUserInputEventData)
        , lookupText(lookupText)
        , tooltipText(tooltipText)
        , imageText(imageText)
        , imageBitmap(imageBitmap)
        , sourceImageMIMEType(sourceImageMIMEType)
        , hasEntireImage(hasEntireImage)
        , allowsFollowingLink(allowsFollowingLink)
        , allowsFollowingImageURL(allowsFollowingImageURL)
        , linkLocalResourceResponse(linkLocalResourceResponse)
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

    CheckedPtr renderer = node->renderer();
    if (!renderer)
        return IntRect();

    return view->contentsToWindow(renderer->absoluteBoundingBoxRect());
}

std::optional<WebCore::SharedMemory::Handle> WebHitTestResultData::getImageSharedMemoryHandle() const
{
    std::optional<WebCore::SharedMemory::Handle> imageHandle = std::nullopt;
    if (RefPtr memory = imageSharedMemory; memory && !memory->span().empty()) {
        if (auto handle = memory->createHandle(WebCore::SharedMemory::Protection::ReadOnly))
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
