/*
 * Copyright (C) 2011 Samsung Electronics
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
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
#include "PageClientDefaultImpl.h"

#include "EwkViewImpl.h"
#include "ewk_view.h"

#if USE(TILED_BACKING_STORE)
#include "PageViewportController.h"
#include "PageViewportControllerClientEfl.h"
#endif

using namespace WebCore;
using namespace EwkViewCallbacks;

namespace WebKit {

PageClientDefaultImpl::PageClientDefaultImpl(EwkViewImpl* viewImpl)
    : PageClientBase(viewImpl)
{
}

void PageClientDefaultImpl::didCommitLoad()
{
#if USE(TILED_BACKING_STORE)
    ASSERT(m_pageViewportController);
    m_pageViewportController->didCommitLoad();
#endif
}

void PageClientDefaultImpl::updateViewportSize()
{
#if USE(TILED_BACKING_STORE)
    if (!m_pageViewportControllerClient) {
        m_pageViewportControllerClient = PageViewportControllerClientEfl::create(m_viewImpl);
        m_pageViewportController = adoptPtr(new PageViewportController(m_viewImpl->page(), m_pageViewportControllerClient.get()));
    }
    m_pageViewportControllerClient->updateViewportSize();
#else
    UNUSED_PARAM(size);
#endif
}

FloatRect PageClientDefaultImpl::convertToDeviceSpace(const FloatRect& userRect)
{
    FloatRect result = userRect;
    result.scale(m_viewImpl->page()->deviceScaleFactor());
    return result;
}

FloatRect PageClientDefaultImpl::convertToUserSpace(const FloatRect& deviceRect)
{
    FloatRect result = deviceRect;
    result.scale(1 / m_viewImpl->page()->deviceScaleFactor());
    return result;
}

void PageClientDefaultImpl::didChangeViewportProperties(const WebCore::ViewportAttributes& attr)
{
#if USE(TILED_BACKING_STORE)
    ASSERT(m_pageViewportController);
    m_pageViewportController->didChangeViewportAttributes(attr);
#else
    UNUSED_PARAM(attr);
#endif
}

void PageClientDefaultImpl::didChangeContentsSize(const WebCore::IntSize& size)
{
#if USE(TILED_BACKING_STORE)
    ASSERT(m_pageViewportController);
    m_pageViewportController->didChangeContentsSize(size);
#endif

    m_viewImpl->smartCallback<ContentsSizeChanged>().call(size);
}

#if USE(TILED_BACKING_STORE)
void PageClientDefaultImpl::pageDidRequestScroll(const IntPoint& position)
{
    ASSERT(m_pageViewportController);
    m_pageViewportController->pageDidRequestScroll(position);
}

void PageClientDefaultImpl::didRenderFrame(const WebCore::IntSize& contentsSize, const WebCore::IntRect& coveredRect)
{
    ASSERT(m_pageViewportController);
    m_pageViewportController->didRenderFrame(contentsSize, coveredRect);
}

void PageClientDefaultImpl::pageTransitionViewportReady()
{
    ASSERT(m_pageViewportController);
    m_pageViewportController->pageTransitionViewportReady();
}
#endif

} // namespace WebKit
