/*
 * Copyright (C) 2021 Metrological Group B.V.
 * Copyright (C) 2021 Igalia S.L.
 * Copyright (C) 2007, 2008, 2011, 2013 Apple Inc. All rights reserved.
 *           (C) 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "DocumentFontLoader.h"

#include "CSSFontSelector.h"
#include "CachedFont.h"
#include "CachedResourceLoader.h"
#include "CachedResourceRequest.h"
#include "CachedResourceRequestInitiatorTypes.h"
#include "Frame.h"
#include "FrameDestructionObserverInlines.h"
#include "FrameLoader.h"

namespace WebCore {

DocumentFontLoader::DocumentFontLoader(Document& document)
    : m_document(document)
    , m_fontLoadingTimer(*this, &DocumentFontLoader::fontLoadingTimerFired)
{
}

DocumentFontLoader::~DocumentFontLoader()
{
    stopLoadingAndClearFonts();
}

CachedFont* DocumentFontLoader::cachedFont(URL&& url, bool isSVG, bool isInitiatingElementInUserAgentShadowTree, LoadedFromOpaqueSource loadedFromOpaqueSource)
{
    ResourceLoaderOptions options = CachedResourceLoader::defaultCachedResourceOptions();
    options.contentSecurityPolicyImposition = isInitiatingElementInUserAgentShadowTree ? ContentSecurityPolicyImposition::SkipPolicyCheck : ContentSecurityPolicyImposition::DoPolicyCheck;
    options.loadedFromOpaqueSource = loadedFromOpaqueSource;

    CachedResourceRequest request(ResourceRequest(WTFMove(url)), options);
    request.setInitiatorType(cachedResourceRequestInitiatorTypes().css);
    return m_document.cachedResourceLoader().requestFont(WTFMove(request), isSVG).value_or(nullptr).get();
}

void DocumentFontLoader::beginLoadingFontSoon(CachedFont& font)
{
    if (m_isStopped)
        return;

    m_fontsToBeginLoading.append(&font);
    // Increment the request count now, in order to prevent didFinishLoad from being dispatched
    // after this font has been requested but before it began loading. Balanced by
    // decrementRequestCount() in fontLoadingTimerFired() and in stopLoadingAndClearFonts().
    m_document.cachedResourceLoader().incrementRequestCount(font);

    if (!m_isFontLoadingSuspended && !m_fontLoadingTimer.isActive())
        m_fontLoadingTimer.startOneShot(0_s);
}

void DocumentFontLoader::loadPendingFonts()
{
    if (m_isFontLoadingSuspended)
        return;

    Vector<CachedResourceHandle<CachedFont>> fontsToBeginLoading;
    fontsToBeginLoading.swap(m_fontsToBeginLoading);

    auto& cachedResourceLoader = m_document.cachedResourceLoader();
    for (auto& fontHandle : fontsToBeginLoading) {
        fontHandle->beginLoadIfNeeded(cachedResourceLoader);
        // Balances incrementRequestCount() in beginLoadingFontSoon().
        cachedResourceLoader.decrementRequestCount(*fontHandle);
    }
}

void DocumentFontLoader::fontLoadingTimerFired()
{
    loadPendingFonts();

    // FIXME: Use SubresourceLoader instead.
    // Call FrameLoader::loadDone before FrameLoader::subresourceLoadDone to match the order in SubresourceLoader::notifyDone.
    m_document.cachedResourceLoader().loadDone(LoadCompletionType::Finish);
    // Ensure that if the request count reaches zero, the frame loader will know about it.
    // New font loads may be triggered by layout after the document load is complete but before we have dispatched
    // didFinishLoading for the frame. Make sure the delegate is always dispatched by checking explicitly.
    if (m_document.frame())
        m_document.frame()->loader().checkLoadComplete();
}

void DocumentFontLoader::stopLoadingAndClearFonts()
{
    if (m_isStopped)
        return;

    m_fontLoadingTimer.stop();
    auto& cachedResourceLoader = m_document.cachedResourceLoader();
    for (auto& fontHandle : m_fontsToBeginLoading) {
        // Balances incrementRequestCount() in beginLoadingFontSoon().
        cachedResourceLoader.decrementRequestCount(*fontHandle);
    }
    m_fontsToBeginLoading.clear();
    m_document.fontSelector().clearFonts();

    m_isFontLoadingSuspended = true;
    m_isStopped = true;
}

void DocumentFontLoader::suspendFontLoading()
{
    if (m_isFontLoadingSuspended)
        return;

    m_fontLoadingTimer.stop();
    m_isFontLoadingSuspended = true;
}

void DocumentFontLoader::resumeFontLoading()
{
    if (!m_isFontLoadingSuspended || m_isStopped)
        return;

    m_isFontLoadingSuspended = false;
    if (!m_fontsToBeginLoading.isEmpty())
        m_fontLoadingTimer.startOneShot(0_s);
}

} // namespace WebCore
