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

#ifndef PluginView_h
#define PluginView_h

#include "Plugin.h"
#include "PluginController.h"
#include "RunLoop.h"
#include "WebFrame.h"

#include <WebCore/MediaCanStartListener.h>
#include <WebCore/Widget.h>
#include <wtf/Deque.h>

// FIXME: Eventually this should move to WebCore.

namespace WebCore {
    class HTMLPlugInElement;
}

namespace WebKit {

class PluginView : public WebCore::Widget, WebCore::MediaCanStartListener, PluginController, WebFrame::LoadListener {
public:
    static PassRefPtr<PluginView> create(WebCore::HTMLPlugInElement* pluginElement, PassRefPtr<Plugin> plugin, const Plugin::Parameters& parameters)
    {
        return adoptRef(new PluginView(pluginElement, plugin, parameters));
    }

private:
    PluginView(WebCore::HTMLPlugInElement*, PassRefPtr<Plugin>, const Plugin::Parameters& parameters);
    virtual ~PluginView();

    void initializePlugin();
    void destroyPlugin();

    void viewGeometryDidChange();
    WebCore::IntRect clipRectInWindowCoordinates() const;

    void pendingURLRequestsTimerFired();
    class URLRequest;
    void performURLRequest(URLRequest*);

    // Perform an URL request where the frame target is not null.
    void performFrameLoadURLRequest(URLRequest*);

    // WebCore::Widget
    virtual void setFrameRect(const WebCore::IntRect&);
    virtual void paint(WebCore::GraphicsContext*, const WebCore::IntRect&);
    virtual void invalidateRect(const WebCore::IntRect&);
    virtual void frameRectsChanged();
    virtual void setParent(WebCore::ScrollView*);

    // WebCore::MediaCanStartListener
    virtual void mediaCanStart();

    // PluginController
    virtual void invalidate(const WebCore::IntRect&);
    virtual WebCore::String userAgent(const WebCore::KURL&);
    virtual void loadURL(uint64_t requestID, const WebCore::String& urlString, const WebCore::String& target, bool allowPopups);

    // WebFrame::LoadListener
    virtual void didFinishLoad(WebFrame*);
    virtual void didFailLoad(WebFrame*, bool wasCancelled);

    WebCore::HTMLPlugInElement* m_pluginElement;
    RefPtr<Plugin> m_plugin;
    Plugin::Parameters m_parameters;
    
    bool m_isInitialized;
    bool m_isWaitingUntilMediaCanStart;

    // Pending URLRequests that the plug-in has made.
    Deque<RefPtr<URLRequest> > m_pendingURLRequests;
    RunLoop::Timer<PluginView> m_pendingURLRequestsTimer;

    // Pending frame loads that the plug-in has made.
    typedef HashMap<RefPtr<WebFrame>, RefPtr<URLRequest> > FrameLoadMap;
    FrameLoadMap m_pendingFrameLoads;

};

} // namespace WebKit

#endif // PluginView_h
