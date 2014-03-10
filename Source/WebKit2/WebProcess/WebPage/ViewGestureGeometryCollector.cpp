/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
#include "ViewGestureGeometryCollector.h"

#include "ViewGestureGeometryCollectorMessages.h"
#include "WebCoreArgumentCoders.h"
#include "WebFrame.h"
#include "WebPage.h"
#include "WebProcess.h"
#include <WebCore/Frame.h>
#include <WebCore/FrameView.h>
#include <WebCore/HitTestResult.h>
#include <WebCore/RenderView.h>

#if PLATFORM(IOS)
#include "SmartMagnificationControllerMessages.h"
#endif

#if PLATFORM(MAC)
#include "ViewGestureControllerMessages.h"
#endif

using namespace WebCore;

namespace WebKit {

ViewGestureGeometryCollector::ViewGestureGeometryCollector(WebPage& webPage)
    : m_webPage(webPage)
#if PLATFORM(MAC)
    , m_renderTreeSizeNotificationThreshold(0)
    , m_renderTreeSizeNotificationTimer(RunLoop::main(), this, &ViewGestureGeometryCollector::renderTreeSizeNotificationTimerFired)
#endif
{
    WebProcess::shared().addMessageReceiver(Messages::ViewGestureGeometryCollector::messageReceiverName(), m_webPage.pageID(), *this);
}

ViewGestureGeometryCollector::~ViewGestureGeometryCollector()
{
    WebProcess::shared().removeMessageReceiver(Messages::ViewGestureGeometryCollector::messageReceiverName(), m_webPage.pageID());
}

void ViewGestureGeometryCollector::dispatchDidCollectGeometryForSmartMagnificationGesture(FloatPoint origin, FloatRect targetRect, FloatRect visibleContentRect, bool isReplacedElement, double viewportMinimumScale, double viewportMaximumScale)
{
#if PLATFORM(MAC)
    m_webPage.send(Messages::ViewGestureController::DidCollectGeometryForSmartMagnificationGesture(origin, targetRect, visibleContentRect, isReplacedElement, viewportMinimumScale, viewportMaximumScale));
#endif
#if PLATFORM(IOS)
    m_webPage.send(Messages::SmartMagnificationController::DidCollectGeometryForSmartMagnificationGesture(origin, targetRect, visibleContentRect, isReplacedElement, viewportMinimumScale, viewportMaximumScale));
#endif
}

void ViewGestureGeometryCollector::collectGeometryForSmartMagnificationGesture(FloatPoint origin)
{
    FloatRect visibleContentRect = m_webPage.mainFrameView()->unobscuredContentRectIncludingScrollbars();

    if (m_webPage.mainWebFrame()->handlesPageScaleGesture())
        return;

    HitTestRequest request(HitTestRequest::ReadOnly | HitTestRequest::Active | HitTestRequest::DisallowShadowContent);
    IntPoint originInContentsSpace = m_webPage.mainFrameView()->windowToContents(roundedIntPoint(origin));
    HitTestResult hitTestResult = HitTestResult(originInContentsSpace);

    m_webPage.mainFrameView()->renderView()->hitTest(request, hitTestResult);

    if (Node* node = hitTestResult.innerNode()) {
        bool isReplaced;
        FloatRect renderRect = node->renderRect(&isReplaced);

#if PLATFORM(MAC)
        double viewportMinimumScale = 0;
        double viewportMaximumScale = std::numeric_limits<double>::max();
#else
        double viewportMinimumScale = m_webPage.minimumPageScaleFactor();
        double viewportMaximumScale = m_webPage.maximumPageScaleFactor();
#endif

        dispatchDidCollectGeometryForSmartMagnificationGesture(origin, renderRect, visibleContentRect, isReplaced, viewportMinimumScale, viewportMaximumScale);
        return;
    }

    dispatchDidCollectGeometryForSmartMagnificationGesture(FloatPoint(), FloatRect(), FloatRect(), false, 0, 0);
}

#if PLATFORM(MAC)
void ViewGestureGeometryCollector::collectGeometryForMagnificationGesture()
{
    FloatRect visibleContentRect = m_webPage.mainFrameView()->unobscuredContentRectIncludingScrollbars();
    bool frameHandlesMagnificationGesture = m_webPage.mainWebFrame()->handlesPageScaleGesture();
    m_webPage.send(Messages::ViewGestureController::DidCollectGeometryForMagnificationGesture(visibleContentRect, frameHandlesMagnificationGesture));
}

void ViewGestureGeometryCollector::mainFrameDidLayout()
{
    if (m_renderTreeSizeNotificationThreshold && m_webPage.renderTreeSize() >= m_renderTreeSizeNotificationThreshold) {
        m_renderTreeSizeNotificationTimer.startOneShot(0);
        m_renderTreeSizeNotificationThreshold = 0;
    }
}

void ViewGestureGeometryCollector::renderTreeSizeNotificationTimerFired()
{
    m_webPage.send(Messages::ViewGestureController::DidHitRenderTreeSizeThreshold());
}
#endif

} // namespace WebKit

