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
#include "PageClientLegacyImpl.h"

#include "CoordinatedLayerTreeHostProxy.h"
#include "EwkView.h"
#include "NotImplemented.h"
#include "ewk_view.h"

using namespace WebCore;
using namespace EwkViewCallbacks;

namespace WebKit {

PageClientLegacyImpl::PageClientLegacyImpl(EwkView* view)
    : PageClientBase(view)
{
}

void PageClientLegacyImpl::didCommitLoad()
{
    m_view->scheduleUpdateDisplay();
}

void PageClientLegacyImpl::updateViewportSize()
{
    m_view->page()->drawingArea()->setVisibleContentsRect(IntRect(roundedIntPoint(m_view->pagePosition()), m_view->size()), FloatPoint());
}

FloatRect PageClientLegacyImpl::convertToDeviceSpace(const FloatRect& viewRect)
{
    notImplemented();
    return viewRect;
}

FloatRect PageClientLegacyImpl::convertToUserSpace(const FloatRect& viewRect)
{
    notImplemented();
    return viewRect;
}

void PageClientLegacyImpl::didChangeViewportProperties(const WebCore::ViewportAttributes&)
{
    m_view->scheduleUpdateDisplay();
}

void PageClientLegacyImpl::didChangeContentsSize(const WebCore::IntSize& size)
{
    view()->didChangeContentsSize(size);
}

void PageClientLegacyImpl::pageDidRequestScroll(const IntPoint& position)
{
    m_view->setPagePosition(FloatPoint(position));
    m_view->scheduleUpdateDisplay();
}

void PageClientLegacyImpl::didRenderFrame(const WebCore::IntSize&, const WebCore::IntRect&)
{
    m_view->scheduleUpdateDisplay();
}

void PageClientLegacyImpl::pageTransitionViewportReady()
{
    m_view->scheduleUpdateDisplay();
}

} // namespace WebKit
