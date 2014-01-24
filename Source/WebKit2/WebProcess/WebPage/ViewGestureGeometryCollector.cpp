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

#include "ViewGestureControllerMessages.h"
#include "ViewGestureGeometryCollectorMessages.h"
#include "WebCoreArgumentCoders.h"
#include "WebFrame.h"
#include "WebPage.h"
#include "WebProcess.h"
#include <WebCore/FrameView.h>
#include <WebCore/HitTestResult.h>
#include <WebCore/RenderView.h>

using namespace WebCore;

namespace WebKit {

ViewGestureGeometryCollector::ViewGestureGeometryCollector(WebPage& webPage)
    : m_webPage(webPage)
{
    WebProcess::shared().addMessageReceiver(Messages::ViewGestureGeometryCollector::messageReceiverName(), m_webPage.pageID(), *this);
}

ViewGestureGeometryCollector::~ViewGestureGeometryCollector()
{
    WebProcess::shared().removeMessageReceiver(Messages::ViewGestureGeometryCollector::messageReceiverName(), m_webPage.pageID());
}

void ViewGestureGeometryCollector::collectGeometryForMagnificationGesture()
{
    FloatRect visibleContentRect = m_webPage.mainFrameView()->visibleContentRectIncludingScrollbars();
    bool frameHandlesMagnificationGesture = m_webPage.mainWebFrame()->handlesPageScaleGesture();
    m_webPage.send(Messages::ViewGestureController::DidCollectGeometryForMagnificationGesture(visibleContentRect, frameHandlesMagnificationGesture));
}

void ViewGestureGeometryCollector::collectGeometryForSmartMagnificationGesture(FloatPoint origin)
{
    FloatRect visibleContentRect = m_webPage.mainFrameView()->visibleContentRectIncludingScrollbars();
    bool frameHandlesMagnificationGesture = m_webPage.mainWebFrame()->handlesPageScaleGesture();

    FloatPoint scrolledOrigin = origin;
    scrolledOrigin.moveBy(m_webPage.mainFrameView()->scrollPosition());

    HitTestRequest request(HitTestRequest::ReadOnly | HitTestRequest::Active | HitTestRequest::DisallowShadowContent);
    HitTestResult hitTestResult = HitTestResult(scrolledOrigin);
    m_webPage.mainFrameView()->renderView()->hitTest(request, hitTestResult);

    if (hitTestResult.innerNode()) {
        bool isReplaced;
        FloatRect renderRect = hitTestResult.innerNode()->renderRect(&isReplaced);
        m_webPage.send(Messages::ViewGestureController::DidCollectGeometryForSmartMagnificationGesture(origin, renderRect, visibleContentRect, isReplaced, frameHandlesMagnificationGesture));
    }
}

} // namespace WebKit
