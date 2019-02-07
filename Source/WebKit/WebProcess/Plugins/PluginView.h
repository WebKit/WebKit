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
#include "NPRuntimeObjectMap.h"
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

    void recreateAndInitialize(Ref<Plugin>&&);

    WebCore::Frame* frame() const;

    bool isBeingDestroyed() const { return !m_plugin || m_plugin->isBeingDestroyed(); }

    void manualLoadDidReceiveResponse(const WebCore::ResourceResponse&);
    void manualLoadDidReceiveData(const char* bytes, int length);
    void manualLoadDidFinishLoading();
    void manualLoadDidFail(const WebCore::ResourceError&);

    void activityStateDidChange(OptionSet<WebCore::ActivityState::Flag> changed);
    void setLayerHostingMode(LayerHostingMode);

#if PLATFORM(COCOA)
    void setDeviceScaleFactor(float);
    void windowAndViewFramesChanged(const WebCore::FloatRect& windowFrameInScreenCoordinates, const WebCore::FloatRect& viewFrameInWindowCoordinates);
    bool sendComplexTextInput(uint64_t pluginComplexTextInputIdentifier, const String& textInput);
    RetainPtr<PDFDocument> pdfDocumentForPrinting() const { return m_plugin->pdfDocumentForPrinting(); }
    NSObject *accessibilityObject() const;
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

    RefPtr<WebCore::SharedBuffer> liveResourceData() const;
    bool performDictionaryLookupAtLocation(const WebCore::FloatPoint&);
    String getSelectionForWordAtPoint(const WebCore::FloatPoint&) const;
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

    void pluginSnapshotTimerFired();
    void pluginDidReceiveUserInteraction();

    bool shouldCreateTransientPaintingSnapshot() const;

    // WebCore::PluginViewBase
#if PLATFORM(COCOA)
    PlatformLayer* platformLayer() const override;
#endif
    JSC::JSObject* scriptObject(JSC::JSGlobalObject*) override;
    void storageBlockingStateChanged() override;
    void privateBrowsingStateChanged(bool) override;
    bool getFormValue(String&) override;
    bool scroll(WebCore::ScrollDirection, WebCore::ScrollGranularity) override;
    WebCore::Scrollbar* horizontalScrollbar() override;
    WebCore::Scrollbar* verticalScrollbar() override;
    bool wantsWheelEvents() override;
    bool shouldAlwaysAutoStart() const override;
    void beginSnapshottingRunningPlugin() override;
    bool shouldAllowNavigationFromDrags() const override;
    bool shouldNotAddLayer() const override;
    void willDetachRenderer() override;

    // WebCore::Widget
    void setFrameRect(const WebCore::IntRect&) override;
    void paint(WebCore::GraphicsContext&, const WebCore::IntRect&, WebCore::Widget::SecurityOriginPaintPolicy) override;
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
    MediaProducer::MediaStateFlags mediaState() const override { return m_pluginIsPlayingAudio ? MediaProducer::IsPlayingAudio : MediaProducer::IsNotPlaying; }
    void pageMutedStateDidChange() override;

    // PluginController
    void invalidate(const WebCore::IntRect&) override;
    String userAgent() override;
    void loadURL(uint64_t requestID, const String& method, const String& urlString, const String& target, const WebCore::HTTPHeaderMap& headerFields, const Vector<uint8_t>& httpBody, bool allowPopups) override;
    void cancelStreamLoad(uint64_t streamID) override;
    void continueStreamLoad(uint64_t streamID) override;
    void cancelManualStreamLoad() override;
#if ENABLE(NETSCAPE_PLUGIN_API)
    NPObject* windowScriptNPObject() override;
    NPObject* pluginElementNPObject() override;
    bool evaluate(NPObject*, const String& scriptString, NPVariant* result, bool allowPopups) override;
    void setPluginIsPlayingAudio(bool) override;
    bool isMuted() const override;
#endif
    void setStatusbarText(const String&) override;
    bool isAcceleratedCompositingEnabled() override;
    void pluginProcessCrashed() override;
#if PLATFORM(COCOA)
    void pluginFocusOrWindowFocusChanged(bool pluginHasFocusAndWindowHasFocus) override;
    void setComplexTextInputState(PluginComplexTextInputState) override;
    const WTF::MachSendRight& compositingRenderServerPort() override;
#endif
    float contentsScaleFactor() override;
    String proxiesForURL(const String&) override;
    String cookiesForURL(const String&) override;
    void setCookiesForURL(const String& urlString, const String& cookieString) override;
    bool getAuthenticationInfo(const WebCore::ProtectionSpace&, String& username, String& password) override;
    bool isPrivateBrowsingEnabled() override;
    bool asynchronousPluginInitializationEnabled() const override;
    bool asynchronousPluginInitializationEnabledForAllPlugins() const override;
    bool artificialPluginInitializationDelayEnabled() const override;
    void protectPluginFromDestruction() override;
    void unprotectPluginFromDestruction() override;
#if PLATFORM(X11)
    uint64_t createPluginContainer() override;
    void windowedPluginGeometryDidChange(const WebCore::IntRect& frameRect, const WebCore::IntRect& clipRect, uint64_t windowID) override;
    void windowedPluginVisibilityDidChange(bool isVisible, uint64_t windowID) override;
#endif

    void didInitializePlugin() override;
    void didFailToInitializePlugin() override;
    void destroyPluginAndReset();

    // WebFrame::LoadListener
    void didFinishLoad(WebFrame*) override;
    void didFailLoad(WebFrame*, bool wasCancelled) override;

    std::unique_ptr<WebEvent> createWebEvent(WebCore::MouseEvent&) const;

    RefPtr<WebCore::HTMLPlugInElement> m_pluginElement;
    RefPtr<Plugin> m_plugin;
    WebPage* m_webPage;
    Plugin::Parameters m_parameters;

    bool m_isInitialized { false };
    bool m_isWaitingForSynchronousInitialization { false };
    bool m_isWaitingUntilMediaCanStart { false };
    bool m_pluginProcessHasCrashed { false };

#if ENABLE(PRIMARY_SNAPSHOTTED_PLUGIN_HEURISTIC)
    bool m_didPlugInStartOffScreen { false };
#endif

    // Pending URLRequests that the plug-in has made.
    Deque<RefPtr<URLRequest>> m_pendingURLRequests;
    RunLoop::Timer<PluginView> m_pendingURLRequestsTimer;

    // Pending frame loads that the plug-in has made.
    typedef HashMap<RefPtr<WebFrame>, RefPtr<URLRequest>> FrameLoadMap;
    FrameLoadMap m_pendingFrameLoads;

    // Streams that the plug-in has requested to load. 
    HashMap<uint64_t, RefPtr<Stream>> m_streams;

#if ENABLE(NETSCAPE_PLUGIN_API)
    // A map of all related NPObjects for this plug-in view.
    NPRuntimeObjectMap m_npRuntimeObjectMap { this };
#endif

    // The manual stream state. This is used so we can deliver a manual stream to a plug-in
    // when it is initialized.
    enum class ManualStreamState { Initial, HasReceivedResponse, Finished, Failed };
    ManualStreamState m_manualStreamState { ManualStreamState::Initial };

    WebCore::ResourceResponse m_manualStreamResponse;
    WebCore::ResourceError m_manualStreamError;
    RefPtr<WebCore::SharedBuffer> m_manualStreamData;

    // This snapshot is used to avoid side effects should the plugin run JS during painting.
    RefPtr<ShareableBitmap> m_transientPaintingSnapshot;
    // This timer is used when plugin snapshotting is enabled, to capture a plugin placeholder.
    WebCore::ResettableOneShotTimer m_pluginSnapshotTimer;
#if ENABLE(PRIMARY_SNAPSHOTTED_PLUGIN_HEURISTIC) || PLATFORM(COCOA)
    unsigned m_countSnapshotRetries { 0 };
#endif
    bool m_didReceiveUserInteraction { false };

    double m_pageScaleFactor { 1 };

    bool m_pluginIsPlayingAudio { false };
};

} // namespace WebKit
