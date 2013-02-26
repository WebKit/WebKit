/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "InspectorClientImpl.h"

#include "DOMWindow.h"
#include "FloatRect.h"
#include "InspectorInstrumentation.h"
#include "NotImplemented.h"
#include "Page.h"
#include "Settings.h"
#include "WebDevToolsAgentImpl.h"
#include "WebViewClient.h"
#include "WebViewImpl.h"
#include <public/WebRect.h>
#include <public/WebURL.h>
#include <public/WebURLRequest.h>
#include <wtf/Vector.h>

using namespace WebCore;

namespace WebKit {

InspectorClientImpl::InspectorClientImpl(WebViewImpl* webView)
    : m_inspectedWebView(webView)
{
    ASSERT(m_inspectedWebView);
}

InspectorClientImpl::~InspectorClientImpl()
{
}

void InspectorClientImpl::inspectorDestroyed()
{
    if (WebDevToolsAgentImpl* agent = devToolsAgent())
        agent->inspectorDestroyed();
}

InspectorFrontendChannel* InspectorClientImpl::openInspectorFrontend(InspectorController* controller)
{
    if (WebDevToolsAgentImpl* agent = devToolsAgent())
        return agent->openInspectorFrontend(controller);
    return 0;
}

void InspectorClientImpl::closeInspectorFrontend()
{
    if (WebDevToolsAgentImpl* agent = devToolsAgent())
        agent->closeInspectorFrontend();
}

void InspectorClientImpl::bringFrontendToFront()
{
    if (WebDevToolsAgentImpl* agent = devToolsAgent())
        agent->bringFrontendToFront();
}

void InspectorClientImpl::highlight()
{
    if (WebDevToolsAgentImpl* agent = devToolsAgent())
        agent->highlight();
}

void InspectorClientImpl::hideHighlight()
{
    if (WebDevToolsAgentImpl* agent = devToolsAgent())
        agent->hideHighlight();
}

bool InspectorClientImpl::sendMessageToFrontend(const WTF::String& message)
{
    if (WebDevToolsAgentImpl* agent = devToolsAgent())
        return agent->sendMessageToFrontend(message);
    return false;
}

void InspectorClientImpl::updateInspectorStateCookie(const WTF::String& inspectorState)
{
    if (WebDevToolsAgentImpl* agent = devToolsAgent())
        agent->updateInspectorStateCookie(inspectorState);
}

bool InspectorClientImpl::canClearBrowserCache()
{
    return true;
}

void InspectorClientImpl::clearBrowserCache()
{
    if (WebDevToolsAgentImpl* agent = devToolsAgent())
        agent->clearBrowserCache();
}

bool InspectorClientImpl::canClearBrowserCookies()
{
    return true;
}

void InspectorClientImpl::clearBrowserCookies()
{
    if (WebDevToolsAgentImpl* agent = devToolsAgent())
        agent->clearBrowserCookies();
}

bool InspectorClientImpl::canMonitorMainThread()
{
    return true;
}

bool InspectorClientImpl::canOverrideDeviceMetrics()
{
    return true;
}

void InspectorClientImpl::overrideDeviceMetrics(int width, int height, float fontScaleFactor, bool fitWindow)
{
    if (WebDevToolsAgentImpl* agent = devToolsAgent())
        agent->overrideDeviceMetrics(width, height, fontScaleFactor, fitWindow);
}

void InspectorClientImpl::autoZoomPageToFitWidth()
{
    if (WebDevToolsAgentImpl* agent = devToolsAgent())
        agent->autoZoomPageToFitWidth();
}

bool InspectorClientImpl::overridesShowPaintRects()
{
    return m_inspectedWebView->isAcceleratedCompositingActive();
}

void InspectorClientImpl::setShowPaintRects(bool show)
{
    m_inspectedWebView->setShowPaintRects(show);
}

bool InspectorClientImpl::canShowDebugBorders()
{
    return true;
}

void InspectorClientImpl::setShowDebugBorders(bool show)
{
    m_inspectedWebView->setShowDebugBorders(show);
}

bool InspectorClientImpl::canShowFPSCounter()
{
    if (m_inspectedWebView->page())
        return m_inspectedWebView->page()->settings()->forceCompositingMode();
    return false;
}

void InspectorClientImpl::setShowFPSCounter(bool show)
{
    m_inspectedWebView->setShowFPSCounter(show);
}

bool InspectorClientImpl::canContinuouslyPaint()
{
    if (m_inspectedWebView->page())
        return m_inspectedWebView->page()->settings()->forceCompositingMode();
    return false;
}

void InspectorClientImpl::setContinuousPaintingEnabled(bool enabled)
{
    m_inspectedWebView->setContinuousPaintingEnabled(enabled);
}

bool InspectorClientImpl::supportsFrameInstrumentation()
{
    return true;
}

void InspectorClientImpl::getAllocatedObjects(HashSet<const void*>& set)
{
    if (WebDevToolsAgentImpl* agent = devToolsAgent())
        agent->getAllocatedObjects(set);
}

void InspectorClientImpl::dumpUncountedAllocatedObjects(const HashMap<const void*, size_t>& map)
{
    if (WebDevToolsAgentImpl* agent = devToolsAgent())
        agent->dumpUncountedAllocatedObjects(map);
}

bool InspectorClientImpl::captureScreenshot(String* data)
{
    if (WebDevToolsAgentImpl* agent = devToolsAgent())
        return agent->captureScreenshot(data);
    return false;
}

bool InspectorClientImpl::handleJavaScriptDialog(bool accept)
{
    if (WebDevToolsAgentImpl* agent = devToolsAgent())
        return agent->handleJavaScriptDialog(accept);
    return false;
}

bool InspectorClientImpl::canSetFileInputFiles()
{
    return true;
}

WebDevToolsAgentImpl* InspectorClientImpl::devToolsAgent()
{
    return static_cast<WebDevToolsAgentImpl*>(m_inspectedWebView->devToolsAgent());
}

} // namespace WebKit
