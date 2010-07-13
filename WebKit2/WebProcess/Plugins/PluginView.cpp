/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#include "PluginView.h"

#include "Plugin.h"
#include <WebCore/FrameLoadRequest.h>
#include <WebCore/FrameLoaderClient.h>
#include <WebCore/FrameView.h>
#include <WebCore/GraphicsContext.h>
#include <WebCore/HTMLPlugInElement.h>
#include <WebCore/HostWindow.h>
#include <WebCore/RenderLayer.h>
#include <WebCore/ScrollView.h>

using namespace WebCore;

namespace WebKit {

class PluginView::URLRequest : public RefCounted<URLRequest> {
public:
    static PassRefPtr<PluginView::URLRequest> create(uint64_t requestID, const FrameLoadRequest& request, bool allowPopups)
    {
        return adoptRef(new URLRequest(requestID, request, allowPopups));
    }

    uint64_t requestID() const { return m_requestID; }
    const String& target() const { return m_request.frameName(); }
    const ResourceRequest & request() const { return m_request.resourceRequest(); }
    bool allowPopups() const { return m_allowPopups; }

private:
    URLRequest(uint64_t requestID, const FrameLoadRequest& request, bool allowPopups)
        : m_requestID(requestID)
        , m_request(request)
        , m_allowPopups(allowPopups)
    {
    }
    
    uint64_t m_requestID;
    FrameLoadRequest m_request;
    bool m_allowPopups;
};

PluginView::PluginView(WebCore::HTMLPlugInElement* pluginElement, PassRefPtr<Plugin> plugin, const Plugin::Parameters& parameters)
    : m_pluginElement(pluginElement)
    , m_plugin(plugin)
    , m_parameters(parameters)
    , m_isInitialized(false)
    , m_isWaitingUntilMediaCanStart(false)
    , m_pendingURLRequestsTimer(RunLoop::main(), this, &PluginView::pendingURLRequestsTimerFired)
{
}

PluginView::~PluginView()
{
    if (m_isWaitingUntilMediaCanStart)
        m_pluginElement->document()->removeMediaCanStartListener(this);

    FrameLoadMap::iterator end = m_pendingFrameLoads.end();
    for (FrameLoadMap::iterator it = m_pendingFrameLoads.begin(), end = m_pendingFrameLoads.end(); it != end; ++it)
        it->first->setLoadListener(0);
    
    if (m_plugin && m_isInitialized)
        m_plugin->destroy();
}

void PluginView::initializePlugin()
{
    if (m_isInitialized)
        return;

    if (!m_plugin) {
        // We've already tried and failed to initialize the plug-in.
        return;
    }

    if (Frame* frame = m_pluginElement->document()->frame()) {
        if (Page* page = frame->page()) {
            
            // We shouldn't initialize the plug-in right now, add a listener.
            if (!page->canStartMedia()) {
                if (m_isWaitingUntilMediaCanStart)
                    return;
                
                m_isWaitingUntilMediaCanStart = true;
                m_pluginElement->document()->addMediaCanStartListener(this);
                return;
            }
        }
    }
    
    if (!m_plugin->initialize(this, m_parameters)) {
        // We failed to initialize the plug-in.
        m_plugin = 0;

        return;
    }
    
    m_isInitialized = true;
}

void PluginView::setFrameRect(const WebCore::IntRect& rect)
{
    Widget::setFrameRect(rect);
    viewGeometryDidChange();
}

void PluginView::paint(GraphicsContext* context, const IntRect& dirtyRect)
{
    if (context->paintingDisabled() || !m_plugin || !m_isInitialized)
        return;
    
    IntRect dirtyRectInWindowCoordinates = parent()->contentsToWindow(dirtyRect);
    
    IntRect paintRectInWindowCoordinates = intersection(dirtyRectInWindowCoordinates, clipRectInWindowCoordinates());
    if (paintRectInWindowCoordinates.isEmpty())
        return;

    context->save();

    // Translate the context so that the origin is at the top left corner of the plug-in view.
    context->translate(frameRect().x(), frameRect().y());

    m_plugin->paint(context, paintRectInWindowCoordinates);
    context->restore();
}

void PluginView::frameRectsChanged()
{
    Widget::frameRectsChanged();
    viewGeometryDidChange();
}

void PluginView::setParent(ScrollView* scrollView)
{
    Widget::setParent(scrollView);
    
    if (scrollView)
        initializePlugin();

    viewGeometryDidChange();
}

void PluginView::viewGeometryDidChange()
{
    if (!parent() || !m_plugin || !m_isInitialized)
        return;

    // Get the frame rect in window coordinates.
    IntRect frameRectInWindowCoordinates = parent()->contentsToWindow(frameRect());

    m_plugin->geometryDidChange(frameRectInWindowCoordinates, clipRectInWindowCoordinates());
}

IntRect PluginView::clipRectInWindowCoordinates() const
{
    ASSERT(parent());

    // Get the frame rect in window coordinates.
    IntRect frameRectInWindowCoordinates = parent()->contentsToWindow(frameRect());

    // Get the window clip rect for the enclosing layer (in window coordinates).
    RenderLayer* layer = m_pluginElement->renderer()->enclosingLayer();
    FrameView* parentView = m_pluginElement->document()->frame()->view();
    IntRect windowClipRect = parentView->windowClipRectForLayer(layer, true);

    // Intersect the two rects to get the view clip rect in window coordinates.
    return intersection(frameRectInWindowCoordinates, windowClipRect);
}

void PluginView::pendingURLRequestsTimerFired()
{
    ASSERT(!m_pendingURLRequests.isEmpty());
    
    RefPtr<URLRequest> urlRequest = m_pendingURLRequests.takeFirst();

    // If there are more requests to perform, reschedule the timer.
    if (!m_pendingURLRequests.isEmpty())
        m_pendingURLRequestsTimer.startOneShot(0);
    
    performURLRequest(urlRequest.get());
}
    
void PluginView::performURLRequest(URLRequest* request)
{
    if (!request->target().isNull())
        return performFrameLoadURLRequest(request);
}

void PluginView::performFrameLoadURLRequest(URLRequest* request)
{
    ASSERT(!request->target().isNull());

    Frame* frame = m_pluginElement->document()->frame();
    if (!frame)
        return;

    // First, try to find a target frame.
    Frame* targetFrame = frame->loader()->findFrameForNavigation(request->target());
    if (!targetFrame) {
        // We did not find a target frame. Ask our frame to load the page. This may or may not create a popup window.
        frame->loader()->load(request->request(), request->target(), false);

        // FIXME: We don't know whether the window was successfully created here so we just assume that it worked.
        // It's better than not telling the plug-in anything.
        m_plugin->frameDidFinishLoading(request->requestID());
        return;
    }

    // Now ask the frame to load the request.
    targetFrame->loader()->load(request->request(), false);

    WebFrame* targetWebFrame = static_cast<WebFrameLoaderClient*>(targetFrame->loader()->client())->webFrame();
    if (WebFrame::LoadListener* loadListener = targetWebFrame->loadListener()) {
        // Check if another plug-in view or even this view is waiting for the frame to load.
        // If it is, tell it that the load was cancelled because it will be anyway.
        loadListener->didFailLoad(targetWebFrame, true);
    }
    
    m_pendingFrameLoads.set(targetWebFrame, request);
    targetWebFrame->setLoadListener(this);
}

void PluginView::invalidateRect(const IntRect& dirtyRect)
{
    if (!parent() || !m_plugin || !m_isInitialized)
        return;

    IntRect dirtyRectInWindowCoordinates = convertToContainingWindow(dirtyRect);

    parent()->hostWindow()->invalidateContentsAndWindow(intersection(dirtyRectInWindowCoordinates, clipRectInWindowCoordinates()), false);
}

void PluginView::mediaCanStart()
{
    ASSERT(m_isWaitingUntilMediaCanStart);
    m_isWaitingUntilMediaCanStart = false;
    
    initializePlugin();
}

void PluginView::invalidate(const IntRect& dirtyRect)
{
    invalidateRect(dirtyRect);
}

String PluginView::userAgent(const KURL& url)
{
    Frame* frame = m_pluginElement->document()->frame();
    if (!frame)
        return String();
    
    return frame->loader()->client()->userAgent(url);
}

void PluginView::loadURL(uint64_t requestID, const String& urlString, const String& target, bool allowPopups)
{
    FrameLoadRequest frameLoadRequest;
    frameLoadRequest.setFrameName(target);
    frameLoadRequest.resourceRequest().setHTTPMethod("GET");
    frameLoadRequest.resourceRequest().setURL(m_pluginElement->document()->completeURL(urlString));
    
    m_pendingURLRequests.append(URLRequest::create(requestID, frameLoadRequest, allowPopups));
    m_pendingURLRequestsTimer.startOneShot(0);
}

void PluginView::didFinishLoad(WebFrame* webFrame)
{
    RefPtr<URLRequest> request = m_pendingFrameLoads.take(webFrame);
    ASSERT(request);
    webFrame->setLoadListener(0);

    m_plugin->frameDidFinishLoading(request->requestID());
}

void PluginView::didFailLoad(WebFrame* webFrame, bool wasCancelled)
{
    RefPtr<URLRequest> request = m_pendingFrameLoads.take(webFrame);
    ASSERT(request);
    webFrame->setLoadListener(0);
    
    m_plugin->frameDidFail(request->requestID(), wasCancelled);
}

} // namespace WebKit
