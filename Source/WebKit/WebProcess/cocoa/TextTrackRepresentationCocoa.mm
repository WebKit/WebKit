/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "TextTrackRepresentationCocoa.h"

#if ENABLE(VIDEO_PRESENTATION_MODE)

#import "VideoPresentationManager.h"
#import "VideoPresentationManagerProxyMessages.h"
#import "WebPage.h"
#import <WebCore/GraphicsContext.h>
#import <WebCore/HTMLVideoElement.h>
#import <WebCore/Page.h>

namespace WebKit {

WebTextTrackRepresentationCocoa::WebTextTrackRepresentationCocoa(WebCore::TextTrackRepresentationClient& client, WebCore::HTMLMediaElement& mediaElement)
    : WebCore::TextTrackRepresentationCocoa(client)
    , m_mediaElement(WeakPtr { mediaElement })
{
    auto* page = mediaElement.document().page();
    if (page)
        m_page = WeakPtr { WebPage::fromCorePage(*page) };
}

void WebTextTrackRepresentationCocoa::update()
{
    if (!m_page)
        return;
    Ref fullscreenManager = m_page->videoPresentationManager();
    if (!m_mediaElement || !is<WebCore::HTMLVideoElement>(m_mediaElement))
        return;
    
    auto image = m_client.createTextTrackRepresentationImage();
    if (!image)
        return;
    auto imageSize = image->size();
    auto bitmap = ShareableBitmap::create({ image->size(), image->colorSpace() });
    if (!bitmap)
        return;
    auto context = bitmap->createGraphicsContext();
    if (!context)
        return;
    context->drawNativeImage(*image, imageSize, WebCore::FloatRect({ }, imageSize), WebCore::FloatRect({ }, imageSize), { WebCore::CompositeOperator::Copy });
    auto handle = bitmap->createHandle();
    if (!handle)
        return;
    Ref videoElement = downcast<WebCore::HTMLVideoElement>(*m_mediaElement);
    fullscreenManager->updateTextTrackRepresentationForVideoElement(videoElement, WTFMove(*handle));
}

void WebTextTrackRepresentationCocoa::setContentScale(float scale)
{
    WebCore::TextTrackRepresentationCocoa::setContentScale(scale);
    if (!m_page)
        return;
    Ref fullscreenManager = m_page->videoPresentationManager();
    if (!m_mediaElement || !is<WebCore::HTMLVideoElement>(m_mediaElement))
        return;
    Ref videoElement = downcast<WebCore::HTMLVideoElement>(*m_mediaElement);
    fullscreenManager->setTextTrackRepresentationContentScaleForVideoElement(videoElement, scale);
}

void WebTextTrackRepresentationCocoa::setHidden(bool hidden) const
{
    WebCore::TextTrackRepresentationCocoa::setHidden(hidden);
    if (!m_page)
        return;
    Ref fullscreenManager = m_page->videoPresentationManager();
    if (!m_mediaElement || !is<WebCore::HTMLVideoElement>(m_mediaElement))
        return;
    Ref videoElement = downcast<WebCore::HTMLVideoElement>(*m_mediaElement);
    fullscreenManager->setTextTrackRepresentationIsHiddenForVideoElement(videoElement, hidden);
}

} // namespace WebKit

#endif // ENABLE(VIDEO_PRESENTATION_MODE)
