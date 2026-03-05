/*
 * Copyright (C) 2010-2022 Apple Inc. All rights reserved.
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

#include "config.h"
#include "InjectedBundleHitTestResult.h"

#include "InjectedBundleNodeHandle.h"
#include "WebFrame.h"
#include "WebImage.h"
#include "WebLocalFrameLoaderClient.h"
#include <WebCore/BitmapImage.h>
#include <WebCore/DocumentView.h>
#include <WebCore/Element.h>
#include <WebCore/FrameDestructionObserverInlines.h>
#include <WebCore/FrameLoader.h>
#include <WebCore/GraphicsContext.h>
#include <WebCore/HTMLMediaElement.h>
#include <WebCore/LocalFrameInlines.h>
#include <WebCore/LocalFrameView.h>
#include <WebCore/NodeDocument.h>
#include <wtf/URL.h>

namespace WebKit {
using namespace WebCore;

Ref<InjectedBundleHitTestResult> InjectedBundleHitTestResult::create(const HitTestResult& hitTestResult)
{
    return adoptRef(*new InjectedBundleHitTestResult(hitTestResult));
}

RefPtr<InjectedBundleNodeHandle> InjectedBundleHitTestResult::nodeHandle() const
{
    return InjectedBundleNodeHandle::getOrCreate(protect(m_hitTestResult.innerNonSharedNode()).get());
}

RefPtr<InjectedBundleNodeHandle> InjectedBundleHitTestResult::urlElementHandle() const
{
    return InjectedBundleNodeHandle::getOrCreate(protect(m_hitTestResult.URLElement()).get());
}

RefPtr<WebFrame> InjectedBundleHitTestResult::frame() const
{
    RefPtr node = m_hitTestResult.innerNonSharedNode();
    if (!node)
        return nullptr;

    RefPtr frame = node->document().frame();
    if (!frame)
        return nullptr;

    return WebFrame::fromCoreFrame(*frame);
}

RefPtr<WebFrame> InjectedBundleHitTestResult::targetFrame() const
{
    RefPtr frame = m_hitTestResult.targetFrame();
    return frame ? WebFrame::fromCoreFrame(*frame) : nullptr;
}

String InjectedBundleHitTestResult::absoluteImageURL() const
{
    return m_hitTestResult.absoluteImageURL().string();
}

String InjectedBundleHitTestResult::absolutePDFURL() const
{
    return m_hitTestResult.absolutePDFURL().string();
}

String InjectedBundleHitTestResult::absoluteLinkURL() const
{
    return m_hitTestResult.absoluteLinkURL().string();
}

String InjectedBundleHitTestResult::absoluteMediaURL() const
{
    return m_hitTestResult.absoluteMediaURL().string();
}

bool InjectedBundleHitTestResult::mediaIsInFullscreen() const
{
    return m_hitTestResult.mediaIsInFullscreen();
}

bool InjectedBundleHitTestResult::mediaHasAudio() const
{
    return m_hitTestResult.mediaHasAudio();
}

bool InjectedBundleHitTestResult::isDownloadableMedia() const
{
    return m_hitTestResult.isDownloadableMedia();
}

BundleHitTestResultMediaType InjectedBundleHitTestResult::mediaType() const
{
#if !ENABLE(VIDEO)
    return BundleHitTestResultMediaTypeNone;
#else
    if (!is<HTMLMediaElement>(m_hitTestResult.innerNonSharedNode()))
        return BundleHitTestResultMediaTypeNone;
    return m_hitTestResult.mediaIsVideo() ? BundleHitTestResultMediaTypeVideo : BundleHitTestResultMediaTypeAudio;
#endif
}

String InjectedBundleHitTestResult::linkLabel() const
{
    return m_hitTestResult.textContent();
}

String InjectedBundleHitTestResult::linkTitle() const
{
    return m_hitTestResult.titleDisplayString();
}

String InjectedBundleHitTestResult::linkSuggestedFilename() const
{
    return m_hitTestResult.linkSuggestedFilename();
}

IntRect InjectedBundleHitTestResult::imageRect() const
{
    IntRect imageRect = m_hitTestResult.imageRect();
    if (imageRect.isEmpty())
        return imageRect;
        
    // The image rect in HitTestResult is in frame coordinates, but we need it in WKView
    // coordinates since WebKit2 clients don't have enough context to do the conversion themselves.
    auto webFrame = frame();
    if (!webFrame)
        return imageRect;
    
    RefPtr coreFrame = webFrame->coreLocalFrame();
    if (!coreFrame)
        return imageRect;
    
    CheckedPtr view = coreFrame->view();
    if (!view)
        return imageRect;
    
    return view->contentsToRootView(imageRect);
}

RefPtr<WebImage> InjectedBundleHitTestResult::image() const
{
    // For now, we only handle bitmap images.
    RefPtr bitmapImage = dynamicDowncast<BitmapImage>(m_hitTestResult.image());
    if (!bitmapImage)
        return nullptr;

    IntSize size(bitmapImage->size());
    RefPtr webImage = WebImage::create(size, { }, DestinationColorSpace::SRGB());
    if (!webImage->context())
        return nullptr;

    // FIXME: need to handle EXIF rotation.
    auto& graphicsContext = *webImage->context();
    graphicsContext.drawImage(*bitmapImage, { { }, size });

    return webImage;
}

bool InjectedBundleHitTestResult::isSelected() const
{
    return m_hitTestResult.isSelected();
}

} // WebKit
