/*
 * Copyright (C) 2010-2018 Apple Inc. All rights reserved.
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

#pragma once

#include "LayerTreeContext.h"
#include "Plugin.h"
#include "PluginController.h"
#include "WebFrame.h"
#include <WebCore/ActivityState.h>
#include <WebCore/FindOptions.h>
#include <WebCore/Image.h>
#include <WebCore/MediaCanStartListener.h>
#include <WebCore/MediaProducer.h>
#include <WebCore/PluginViewBase.h>
#include <WebCore/ResourceError.h>
#include <WebCore/ResourceResponse.h>
#include <WebCore/SharedBuffer.h>
#include <WebCore/Timer.h>
#include <memory>
#include <wtf/Deque.h>
#include <wtf/RunLoop.h>

// FIXME: Eventually this should move to WebCore.

#if PLATFORM(COCOA)
OBJC_CLASS NSDictionary;
OBJC_CLASS PDFSelection;
#endif

namespace WTF {
class MachSendRight;
}

namespace WebCore {
class Frame;
class HTMLPlugInElement;
class MouseEvent;
}

namespace WebKit {

class WebEvent;

class PluginView : public WebCore::PluginViewBase, public PluginController, private WebCore::MediaCanStartListener, private WebFrame::LoadListener, private WebCore::MediaProducer {
public:
    static Ref<PluginView> create(WebCore::HTMLPlugInElement&, Ref<Plugin>&&, const Plugin::Parameters&);

    WebCore::Frame* frame() const;

    bool isBeingDestroyed() const { return !m_plugin || m_plugin->isBeingDestroyed(); }

    void manualLoadDidReceiveResponse(const WebCore::ResourceResponse&);
    void manualLoadDidReceiveData(const WebCore::SharedBuffer&);
    void manualLoadDidFinishLoading();
    void manualLoadDidFail(const WebCore::ResourceError&);

    void activityStateDidChange(OptionSet<WebCore::ActivityState::Flag> changed);
    void setLayerHostingMode(LayerHostingMode);

#if PLATFORM(COCOA)
    void setDeviceScaleFactor(float);
    void windowAndViewFramesChanged(const WebCore::FloatRect& windowFrameInScreenCoordinates, const WebCore::FloatRect& viewFrameInWindowCoordinates);
    RetainPtr<PDFDocument> pdfDocumentForPrinting() const { return m_plugin->pdfDocumentForPrinting(); }
    id accessibilityHitTest(const WebCore::IntPoint& point) const override { return m_plugin->accessibilityHitTest(point); }
    id accessibilityObject() const override;
    id accessibilityAssociatedPluginParentForElement(WebCore::Element*) const override;
#endif

    WebCore::HTMLPlugInElement* pluginElement() const { return m_pluginElement.get(); }
    const Plugin::Parameters& initialParameters() const { return m_parameters; }
    Plugin* plugin() const { return m_plugin.get(); }

    void setPageScaleFactor(double scaleFactor, WebCore::IntPoint origin);
    double pageScaleFactor() const;
    bool handlesPageScaleFactor() const;
    bool requiresUnifiedScaleFactor() const;

    void pageScaleFactorDidChange();
    void topContentInsetDidChange();

    void webPageDestroyed();

    bool handleEditingCommand(const String& commandName, const String& argument);
    bool isEditingCommandEnabled(const String& commandName);
    
    unsigned countFindMatches(const String& target, WebCore::FindOptions, unsigned maxMatchCount);
    bool findString(const String& target, WebCore::FindOptions, unsigned maxMatchCount);

    String getSelectionString() const;

    bool shouldAllowScripting();

    RefPtr<WebCore::FragmentedSharedBuffer> liveResourceData() const;
    bool performDictionaryLookupAtLocation(const WebCore::FloatPoint&);
    bool existingSelectionContainsPoint(const WebCore::FloatPoint&) const;

private:
    PluginView(WebCore::HTMLPlugInElement&, Ref<Plugin>&&, const Plugin::Parameters&);
    virtual ~PluginView();

    void initializePlugin();

    void viewGeometryDidChange();
    void viewVisibilityDidChange();
    WebCore::IntRect clipRectInWindowCoordinates() const;
    void focusPluginElement();
    
    void pendingURLRequestsTimerFired();
    class URLRequest;
    void performURLRequest(URLRequest*);

    // Perform a URL request where the frame target is not null.
    void performFrameLoadURLRequest(URLRequest*);

    // Perform a URL request where the URL protocol is "javascript:".
    void performJavaScriptURLRequest(URLRequest*);

    class Stream;
    void addStream(Stream*);
    void removeStream(Stream*);
    void cancelAllStreams();

    void redeliverManualStream();

    bool shouldCreateTransientPaintingSnapshot() const;

    // WebCore::PluginViewBase
#if PLATFORM(COCOA)
    PlatformLayer* platformLayer() const override;
#endif
    JSC::JSObject* scriptObject(JSC::JSGlobalObject*) override;
    void storageBlockingStateChanged() override;
    bool scroll(WebCore::ScrollDirection, WebCore::ScrollGranularity) override;
    WebCore::Scrollbar* horizontalScrollbar() override;
    WebCore::Scrollbar* verticalScrollbar() override;
    bool wantsWheelEvents() override;
    bool shouldAllowNavigationFromDrags() const override;
    void willDetachRenderer() override;

    // WebCore::Widget
    void setFrameRect(const WebCore::IntRect&) override;
    void paint(WebCore::GraphicsContext&, const WebCore::IntRect&, WebCore::Widget::SecurityOriginPaintPolicy, WebCore::EventRegionContext*) override;
    void invalidateRect(const WebCore::IntRect&) override;
    void setFocus(bool) override;
    void frameRectsChanged() override;
    void setParent(WebCore::ScrollView*) override;
    void handleEvent(WebCore::Event&) override;
    void notifyWidget(WebCore::WidgetNotification) override;
    void show() override;
    void hide() override;
    void setParentVisible(bool) override;
    bool transformsAffectFrameRect() override;
    void clipRectChanged() override;

    // WebCore::MediaCanStartListener
    void mediaCanStart(WebCore::Document&) override;

    // WebCore::MediaProducer
    WebCore::MediaProducerMediaStateFlags mediaState() const override;
    void pageMutedStateDidChange() override;

    // PluginController
    void loadURL(uint64_t requestID, const String& method, const String& urlString, const String& target, const WebCore::HTTPHeaderMap& headerFields, const Vector<uint8_t>& httpBody, bool allowPopups) override;
    float contentsScaleFactor() override;

    void didInitializePlugin() override;
    void destroyPluginAndReset();

    // WebFrame::LoadListener
    void didFinishLoad(WebFrame*) override;
    void didFailLoad(WebFrame*, bool wasCancelled) override;

    std::unique_ptr<WebEvent> createWebEvent(WebCore::MouseEvent&) const;

    RefPtr<WebCore::HTMLPlugInElement> m_pluginElement;
    RefPtr<Plugin> m_plugin;
    WeakPtr<WebPage> m_webPage;
    Plugin::Parameters m_parameters;

    bool m_isInitialized { false };
    bool m_isWaitingForSynchronousInitialization { false };
    bool m_isWaitingUntilMediaCanStart { false };

    // Pending URLRequests that the plug-in has made.
    Deque<RefPtr<URLRequest>> m_pendingURLRequests;
    RunLoop::Timer<PluginView> m_pendingURLRequestsTimer;

    // Pending frame loads that the plug-in has made.
    typedef HashMap<RefPtr<WebFrame>, RefPtr<URLRequest>> FrameLoadMap;
    FrameLoadMap m_pendingFrameLoads;

    // Streams that the plug-in has requested to load. 
    HashMap<uint64_t, RefPtr<Stream>> m_streams;

    // The manual stream state. This is used so we can deliver a manual stream to a plug-in
    // when it is initialized.
    enum class ManualStreamState { Initial, HasReceivedResponse, Finished, Failed };
    ManualStreamState m_manualStreamState { ManualStreamState::Initial };

    WebCore::ResourceResponse m_manualStreamResponse;
    WebCore::ResourceError m_manualStreamError;
    WebCore::SharedBufferBuilder m_manualStreamData;

    // This snapshot is used to avoid side effects should the plugin run JS during painting.
    RefPtr<ShareableBitmap> m_transientPaintingSnapshot;

    double m_pageScaleFactor { 1 };

    bool m_pluginIsPlayingAudio { false };
};

inline WebCore::MediaProducerMediaStateFlags PluginView::mediaState() const
{
    WebCore::MediaProducerMediaStateFlags mediaState;
    if (m_pluginIsPlayingAudio)
        mediaState.add(WebCore::MediaProducerMediaState::IsPlayingAudio);
    return mediaState;
}

} // namespace WebKit
