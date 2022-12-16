/*
 * Copyright (C) 2010-2022 Apple Inc. All rights reserved.
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

#if ENABLE(PDFKIT_PLUGIN)

#include <WebCore/FindOptions.h>
#include <WebCore/PluginViewBase.h>
#include <WebCore/ResourceResponse.h>
#include <WebCore/SharedBuffer.h>
#include <WebCore/Timer.h>
#include <memory>
#include <wtf/RunLoop.h>

OBJC_CLASS NSDictionary;
OBJC_CLASS PDFDocument;
OBJC_CLASS PDFSelection;

namespace WebCore {
class Frame;
class HTMLPlugInElement;
}

namespace WebKit {

class PDFPlugin;
class ShareableBitmap;
class WebPage;

struct WebHitTestResultData;

class PluginView final : public WebCore::PluginViewBase {
public:
    static RefPtr<PluginView> create(WebCore::HTMLPlugInElement&, const URL&, const String& contentType, bool shouldUseManualLoader);

    WebCore::Frame* frame() const;

    bool isBeingDestroyed() const;

    void manualLoadDidReceiveResponse(const WebCore::ResourceResponse&);
    void manualLoadDidReceiveData(const WebCore::SharedBuffer&);
    void manualLoadDidFinishLoading();
    void manualLoadDidFail();

    void setDeviceScaleFactor(float);
    RetainPtr<PDFDocument> pdfDocumentForPrinting() const;
    WebCore::FloatSize pdfDocumentSizeForPrinting() const;
    id accessibilityHitTest(const WebCore::IntPoint&) const final;
    id accessibilityObject() const final;
    id accessibilityAssociatedPluginParentForElement(WebCore::Element*) const final;

    WebCore::HTMLPlugInElement& pluginElement() const { return m_pluginElement; }
    const URL& mainResourceURL() const { return m_mainResourceURL; }

    void setPageScaleFactor(double);
    double pageScaleFactor() const;

    void pageScaleFactorDidChange();
    void topContentInsetDidChange();

    void webPageDestroyed();

    bool handleEditingCommand(const String& commandName, const String& argument);
    bool isEditingCommandEnabled(const String& commandName);
    
    unsigned countFindMatches(const String& target, WebCore::FindOptions, unsigned maxMatchCount);
    bool findString(const String& target, WebCore::FindOptions, unsigned maxMatchCount);

    String getSelectionString() const;

    RefPtr<WebCore::FragmentedSharedBuffer> liveResourceData() const;
    bool performDictionaryLookupAtLocation(const WebCore::FloatPoint&);
    bool existingSelectionContainsPoint(const WebCore::FloatPoint&) const;

    std::tuple<String, PDFSelection *, NSDictionary *> lookupTextAtLocation(const WebCore::FloatPoint&, WebHitTestResultData&) const;
    WebCore::FloatRect rectForSelectionInRootView(PDFSelection *) const;
    CGFloat contentScaleFactor() const;
    
    bool isUsingUISideCompositing() const;

private:
    PluginView(WebCore::HTMLPlugInElement&, const URL&, const String& contentType, bool shouldUseManualLoader, WebPage&);
    virtual ~PluginView();

    void initializePlugin();

    void viewGeometryDidChange();
    void viewVisibilityDidChange();
    WebCore::IntRect clipRectInWindowCoordinates() const;
    void focusPluginElement();
    
    void pendingResourceRequestTimerFired();

    void loadMainResource();
    void redeliverManualStream();

    bool shouldCreateTransientPaintingSnapshot() const;

    // WebCore::PluginViewBase
    PlatformLayer* platformLayer() const final;
    bool scroll(WebCore::ScrollDirection, WebCore::ScrollGranularity) final;
    WebCore::Scrollbar* horizontalScrollbar() final;
    WebCore::Scrollbar* verticalScrollbar() final;
    bool wantsWheelEvents() final;
    bool shouldAllowNavigationFromDrags() const final;
    void willDetachRenderer() final;

    // WebCore::Widget
    void setFrameRect(const WebCore::IntRect&) final;
    void paint(WebCore::GraphicsContext&, const WebCore::IntRect&, WebCore::Widget::SecurityOriginPaintPolicy, WebCore::EventRegionContext*) final;
    void invalidateRect(const WebCore::IntRect&) final;
    void frameRectsChanged() final;
    void setParent(WebCore::ScrollView*) final;
    void handleEvent(WebCore::Event&) final;
    void notifyWidget(WebCore::WidgetNotification) final;
    void show() final;
    void hide() final;
    void setParentVisible(bool) final;
    bool transformsAffectFrameRect() final;
    void clipRectChanged() final;

    Ref<WebCore::HTMLPlugInElement> m_pluginElement;
    Ref<PDFPlugin> m_plugin;
    WeakPtr<WebPage> m_webPage;
    URL m_mainResourceURL;
    String m_mainResourceContentType;
    bool m_shouldUseManualLoader { false };

    bool m_isInitialized { false };

    // Pending request that the plug-in has made.
    std::unique_ptr<const WebCore::ResourceRequest> m_pendingResourceRequest;
    RunLoop::Timer m_pendingResourceRequestTimer;

    // Stream that the plug-in has requested to load.
    class Stream;
    RefPtr<Stream> m_stream;

    // The manual stream state. We deliver a manual stream to a plug-in when it is initialized.
    enum class ManualStreamState { Initial, HasReceivedResponse, Finished, Failed };
    ManualStreamState m_manualStreamState { ManualStreamState::Initial };
    WebCore::ResourceResponse m_manualStreamResponse;
    WebCore::SharedBufferBuilder m_manualStreamData;

    // This snapshot is used to avoid side effects should the plugin run JS during painting.
    RefPtr<ShareableBitmap> m_transientPaintingSnapshot;

    double m_pageScaleFactor { 1 };
};

} // namespace WebKit

#endif
