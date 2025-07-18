/*
 * Copyright (C) 2010-2025 Apple Inc. All rights reserved.
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
#include "PluginView.h"

#if ENABLE(PDF_PLUGIN)

#include "DocumentEditingContext.h"
#include "FrameInfoData.h"
#include "PDFPlugin.h"
#include "UnifiedPDFPlugin.h"
#include "WebFrame.h"
#include "WebKeyboardEvent.h"
#include "WebLoaderStrategy.h"
#include "WebMouseEvent.h"
#include "WebPage.h"
#include "WebPageProxyMessages.h"
#include "WebProcess.h"
#include "WebWheelEvent.h"
#include <WebCore/BitmapImage.h>
#include <WebCore/Chrome.h>
#include <WebCore/ContainerNodeInlines.h>
#include <WebCore/CookieJar.h>
#include <WebCore/Credential.h>
#include <WebCore/CredentialStorage.h>
#include <WebCore/DocumentInlines.h>
#include <WebCore/DocumentLoader.h>
#include <WebCore/EventHandler.h>
#include <WebCore/EventNames.h>
#include <WebCore/FocusController.h>
#include <WebCore/FrameLoadRequest.h>
#include <WebCore/FrameLoader.h>
#include <WebCore/GraphicsContext.h>
#include <WebCore/HTMLNames.h>
#include <WebCore/HTMLPlugInElement.h>
#include <WebCore/HTTPHeaderNames.h>
#include <WebCore/HostWindow.h>
#include <WebCore/LocalFrame.h>
#include <WebCore/LocalFrameLoaderClient.h>
#include <WebCore/LocalFrameView.h>
#include <WebCore/MIMETypeRegistry.h>
#include <WebCore/MouseEvent.h>
#include <WebCore/NetscapePlugInStreamLoader.h>
#include <WebCore/NetworkStorageSession.h>
#include <WebCore/OriginAccessPatterns.h>
#include <WebCore/PageInlines.h>
#include <WebCore/PlatformMouseEvent.h>
#include <WebCore/ProtectionSpace.h>
#include <WebCore/RenderBoxModelObjectInlines.h>
#include <WebCore/RenderEmbeddedObject.h>
#include <WebCore/ScriptController.h>
#include <WebCore/ScrollView.h>
#include <WebCore/SecurityOrigin.h>
#include <WebCore/SecurityPolicy.h>
#include <WebCore/Settings.h>
#include <WebCore/ShareableBitmap.h>
#include <WebCore/UserGestureIndicator.h>
#include <pal/text/TextEncoding.h>
#include <wtf/CompletionHandler.h>
#include <wtf/text/StringBuilder.h>

namespace WebKit {
using namespace JSC;
using namespace WebCore;

class PluginView::Stream : public RefCounted<PluginView::Stream>, NetscapePlugInStreamLoaderClient {
public:
    static Ref<Stream> create(PluginView* pluginView, const ResourceRequest& request)
    {
        return adoptRef(*new Stream(pluginView, request));
    }
    ~Stream();

    void start();
    void cancel();
    void continueLoad();

private:
    Stream(PluginView* pluginView, const ResourceRequest& request)
        : m_pluginView(pluginView)
        , m_request(request)
        , m_streamWasCancelled(false)
    {
    }

    // NetscapePluginStreamLoaderClient
    void willSendRequest(NetscapePlugInStreamLoader*, ResourceRequest&&, const ResourceResponse& redirectResponse, CompletionHandler<void(ResourceRequest&&)>&&) override;
    void didReceiveResponse(NetscapePlugInStreamLoader*, const ResourceResponse&) override;
    void didReceiveData(NetscapePlugInStreamLoader*, const SharedBuffer&) override;
    void didFail(NetscapePlugInStreamLoader*, const ResourceError&) override;
    void didFinishLoading(NetscapePlugInStreamLoader*) override;

    SingleThreadWeakPtr<PluginView> m_pluginView;
    ResourceRequest m_request;
    CompletionHandler<void(ResourceRequest&&)> m_loadCallback;

    // True if the stream was explicitly cancelled by calling cancel().
    // (As opposed to being cancelled by the user hitting the stop button for example.
    bool m_streamWasCancelled;

    RefPtr<NetscapePlugInStreamLoader> m_loader;
};

PluginView::Stream::~Stream()
{
    if (m_loadCallback)
        m_loadCallback({ });
    ASSERT(!m_pluginView);
}

void PluginView::Stream::start()
{
    ASSERT(!m_loader);

    RefPtr frame = m_pluginView->frame();
    ASSERT(frame);

    WebProcess::singleton().protectedWebLoaderStrategy()->schedulePluginStreamLoad(*frame, *this, ResourceRequest { m_request }, [this, protectedThis = Ref { *this }](RefPtr<NetscapePlugInStreamLoader>&& loader) {
        m_loader = WTFMove(loader);
    });
}

void PluginView::Stream::cancel()
{
    ASSERT(m_loader);

    m_streamWasCancelled = true;

    RefPtr loader = std::exchange(m_loader, nullptr);
    loader->cancel(loader->cancelledError());
}

void PluginView::Stream::continueLoad()
{
    ASSERT(m_loadCallback);
    m_loadCallback(ResourceRequest(m_request));
}

void PluginView::Stream::willSendRequest(NetscapePlugInStreamLoader*, ResourceRequest&& request, const ResourceResponse&, CompletionHandler<void(ResourceRequest&&)>&& decisionHandler)
{
    m_loadCallback = WTFMove(decisionHandler);
    m_request = WTFMove(request);
}

void PluginView::Stream::didReceiveResponse(NetscapePlugInStreamLoader*, const ResourceResponse& response)
{
    m_pluginView->m_plugin->streamDidReceiveResponse(response);
}

void PluginView::Stream::didReceiveData(NetscapePlugInStreamLoader*, const SharedBuffer& buffer)
{
    m_pluginView->m_plugin->streamDidReceiveData(buffer);
}

void PluginView::Stream::didFail(NetscapePlugInStreamLoader*, const ResourceError&)
{
    // Calling streamDidFail could cause us to be deleted, so we hold on to a reference here.
    Ref protectedThis { *this };

    // We only want to call streamDidFail if the stream was not explicitly cancelled by the plug-in.
    if (!m_streamWasCancelled)
        m_pluginView->m_plugin->streamDidFail();

    ASSERT(m_pluginView->m_stream == this);
    m_pluginView->m_stream = nullptr;
    m_pluginView = nullptr;
}

void PluginView::Stream::didFinishLoading(NetscapePlugInStreamLoader*)
{
    // Calling streamDidFinishLoading could cause us to be deleted, so we hold on to a reference here.
    Ref protectedThis { *this };

    m_pluginView->m_plugin->streamDidFinishLoading();

    ASSERT(m_pluginView->m_stream == this);
    m_pluginView->m_stream = nullptr;
    m_pluginView = nullptr;
}

RefPtr<PluginView> PluginView::create(HTMLPlugInElement& element, const URL& mainResourceURL, const String& contentType, bool shouldUseManualLoader)
{
    RefPtr coreFrame = element.document().frame();
    if (!coreFrame)
        return nullptr;

    RefPtr frame = WebFrame::fromCoreFrame(*coreFrame);
    if (!frame)
        return nullptr;

    RefPtr page = frame->page();
    if (!page || !page->shouldUsePDFPlugin(contentType, mainResourceURL.path()))
        return nullptr;

    return adoptRef(*new PluginView(element, mainResourceURL, contentType, shouldUseManualLoader, *page));
}

static Ref<PDFPluginBase> createPlugin(HTMLPlugInElement& element)
{
#if ENABLE(UNIFIED_PDF)
    if (element.document().settings().unifiedPDFEnabled())
        return UnifiedPDFPlugin::create(element);
#endif
#if ENABLE(LEGACY_PDFKIT_PLUGIN)
    return PDFPlugin::create(element);
#endif
    RELEASE_ASSERT_NOT_REACHED();

    RefPtr<PDFPluginBase> nullPluginBase;
    return nullPluginBase.releaseNonNull();
}

PluginView::PluginView(HTMLPlugInElement& element, const URL& mainResourceURL, const String&, bool shouldUseManualLoader, WebPage& page)
    : m_pluginElement(element)
    , m_plugin(createPlugin(element))
    , m_webPage(page)
    , m_mainResourceURL(mainResourceURL)
    , m_shouldUseManualLoader(shouldUseManualLoader)
    , m_pendingResourceRequestTimer(RunLoop::mainSingleton(), this, &PluginView::pendingResourceRequestTimerFired)
{
    m_plugin->startLoading();
    page.addPluginView(*this);
    updateDocumentForPluginSizingBehavior();
}

PluginView::~PluginView()
{
    if (RefPtr webPage = m_webPage.get())
        webPage->removePluginView(*this);
    if (RefPtr stream = m_stream)
        stream->cancel();
    m_plugin->destroy();
}

RefPtr<WebPage> PluginView::protectedWebPage() const
{
    return m_webPage.get();
}

LocalFrame* PluginView::frame() const
{
    return m_pluginElement->document().frame();
}

RefPtr<LocalFrame> PluginView::protectedFrame() const
{
    return frame();
}

void PluginView::manualLoadDidReceiveResponse(const ResourceResponse& response)
{
    if (!m_isInitialized) {
        ASSERT(m_manualStreamState == ManualStreamState::Initial);
        m_manualStreamState = ManualStreamState::HasReceivedResponse;
        m_manualStreamResponse = response;
        return;
    }

    m_plugin->streamDidReceiveResponse(response);
}

void PluginView::manualLoadDidReceiveData(const SharedBuffer& buffer)
{
    if (!m_isInitialized) {
        ASSERT(m_manualStreamState == ManualStreamState::HasReceivedResponse);
        m_manualStreamData.append(buffer);
        return;
    }

    m_plugin->streamDidReceiveData(buffer);
}

void PluginView::manualLoadDidFinishLoading()
{
    if (!m_isInitialized) {
        ASSERT(m_manualStreamState == ManualStreamState::HasReceivedResponse);
        m_manualStreamState = ManualStreamState::Finished;
        return;
    }

    m_plugin->streamDidFinishLoading();
}

void PluginView::layerHostingStrategyDidChange()
{
    if (!m_isInitialized)
        return;

    // This ensures that we update RenderLayers and compositing when the result of RenderEmbeddedObject::requiresLayer() changes.
    m_pluginElement->invalidateStyleAndLayerComposition();
}

void PluginView::manualLoadDidFail()
{
    if (!m_isInitialized) {
        m_manualStreamState = ManualStreamState::Finished;
        m_manualStreamData.reset();
        return;
    }

    m_plugin->streamDidFail();
}

void PluginView::obscuredContentInsetsDidChange()
{
    viewGeometryDidChange();
}

void PluginView::didBeginMagnificationGesture()
{
    if (!m_isInitialized)
        return;

    m_plugin->didBeginMagnificationGesture();
}

void PluginView::didEndMagnificationGesture()
{
    if (!m_isInitialized)
        return;

    m_plugin->didEndMagnificationGesture();
}

void PluginView::setPageScaleFactor(double scaleFactor, std::optional<IntPoint> origin)
{
    if (!m_isInitialized)
        return;

    pluginScaleFactorDidChange();
    m_plugin->setPageScaleFactor(scaleFactor, origin);
}

void PluginView::mainFramePageScaleFactorDidChange()
{
    if (!m_isInitialized)
        return;

    m_plugin->mainFramePageScaleFactorDidChange();
}

double PluginView::pageScaleFactor() const
{
    return m_plugin->scaleFactor();
}

void PluginView::pluginScaleFactorDidChange()
{
    if (!m_plugin->handlesPageScaleFactor())
        return;
    auto scaleFactor = pageScaleFactor();
    RefPtr webPage = m_webPage.get();
    webPage->send(Messages::WebPageProxy::PluginScaleFactorDidChange(scaleFactor));
    webPage->send(Messages::WebPageProxy::PluginZoomFactorDidChange(scaleFactor));
}

void PluginView::webPageDestroyed()
{
    m_webPage = nullptr;
}

#if PLATFORM(COCOA)

void PluginView::setDeviceScaleFactor(float scaleFactor)
{
    if (!m_isInitialized)
        return;

    m_plugin->deviceScaleFactorChanged(scaleFactor);
}

id PluginView::accessibilityAssociatedPluginParentForElement(Element* element) const
{
    return m_plugin->accessibilityAssociatedPluginParentForElement(element);
}

id PluginView::accessibilityObject() const
{
    if (!m_isInitialized)
        return nil;

    return m_plugin->accessibilityObject();
}

#endif

void PluginView::initializePlugin()
{
    if (m_isInitialized)
        return;

    Ref plugin = m_plugin;
    plugin->setView(*this);

    if (!m_shouldUseManualLoader && !m_mainResourceURL.isEmpty())
        loadMainResource();

    m_isInitialized = true;

    viewVisibilityDidChange();
    viewGeometryDidChange();

    redeliverManualStream();

#if PLATFORM(COCOA)
    if (plugin->isComposited() && frame()) {
        frame()->protectedView()->enterCompositingMode();
        m_pluginElement->invalidateStyleAndLayerComposition();
    }
    plugin->visibilityDidChange(isVisible());
#endif

    if (RefPtr frame = this->frame()) {
        if (RefPtr frameView = frame->view())
            frameView->setNeedsLayoutAfterViewConfigurationChange();
        if (frame->isMainFrame() && plugin->isFullFramePlugin())
            WebFrame::fromCoreFrame(*frame)->protectedPage()->send(Messages::WebPageProxy::MainFramePluginHandlesPageScaleGestureDidChange(plugin->handlesPageScaleFactor(), plugin->minScaleFactor(), plugin->maxScaleFactor()));
    }
}

PluginLayerHostingStrategy PluginView::layerHostingStrategy() const
{
    if (!m_isInitialized)
        return PluginLayerHostingStrategy::None;

    return m_plugin->layerHostingStrategy();
}

#if PLATFORM(COCOA)

PlatformLayer* PluginView::platformLayer() const
{
    if (!m_isInitialized)
        return nullptr;

    Ref plugin = m_plugin;
    if (plugin->layerHostingStrategy() == PluginLayerHostingStrategy::PlatformLayer)
        return plugin->platformLayer();

    return nullptr;
}

#endif

GraphicsLayer* PluginView::graphicsLayer() const
{
    if (!m_isInitialized)
        return nullptr;

    Ref plugin = m_plugin;
    if (plugin->layerHostingStrategy() == PluginLayerHostingStrategy::GraphicsLayer)
        return plugin->graphicsLayer();

    return nullptr;
}

bool PluginView::scroll(ScrollDirection direction, ScrollGranularity granularity)
{
    if (!m_isInitialized)
        return false;

#if ENABLE(LEGACY_PDFKIT_PLUGIN)
    if (RefPtr plugin = dynamicDowncast<PDFPlugin>(m_plugin))
        return plugin->scroll(direction, granularity);
#endif

    return false;
}

ScrollPosition PluginView::scrollPositionForTesting() const
{
    if (!m_isInitialized)
        return { };

#if ENABLE(LEGACY_PDFKIT_PLUGIN)
    if (RefPtr plugin = dynamicDowncast<PDFPlugin>(m_plugin))
        return plugin->scrollPositionForTesting();
#endif

    return { };
}

Scrollbar* PluginView::horizontalScrollbar()
{
    if (!m_isInitialized)
        return nullptr;

    return m_plugin->horizontalScrollbar();
}

Scrollbar* PluginView::verticalScrollbar()
{
    if (!m_isInitialized)
        return nullptr;

    return m_plugin->verticalScrollbar();
}

bool PluginView::wantsWheelEvents()
{
    if (!m_isInitialized)
        return false;

    return m_plugin->wantsWheelEvents();
}

void PluginView::setFrameRect(const WebCore::IntRect& rect)
{
    Widget::setFrameRect(rect);
    viewGeometryDidChange();
}

void PluginView::paint(GraphicsContext& context, const IntRect& dirtyRect, Widget::SecurityOriginPaintPolicy, RegionContext*)
{
    if (!m_isInitialized)
        return;

    if (context.paintingDisabled()) {
        if (context.invalidatingControlTints())
            m_plugin->updateControlTints(context);
        return;
    }

    // FIXME: We should try to intersect the dirty rect with the plug-in's clip rect here.
    if (frameRect().isEmpty())
        return;

    if (RefPtr transientPaintingSnapshot = m_transientPaintingSnapshot) {
        if (!context.hasPlatformContext()) {
            RefPtr image = transientPaintingSnapshot->createImage();
            if (!image)
                return;
            context.drawImage(*image, frameRect());
        } else {
            auto deviceScaleFactor = 1;
            if (RefPtr page = m_pluginElement->document().page())
                deviceScaleFactor = page->deviceScaleFactor();
            transientPaintingSnapshot->paint(context, deviceScaleFactor, frameRect().location(), transientPaintingSnapshot->bounds());
        }
        return;
    }

    bool isSnapshotting = [&]() {
        RefPtr frameView = frame()->view();
        if (!frameView)
            return false;

        return frameView->paintBehavior().contains(PaintBehavior::FlattenCompositingLayers);
    }();

    if (!isSnapshotting && m_plugin->layerHostingStrategy() == PluginLayerHostingStrategy::GraphicsLayer)
        return;

    // RenderWidget::paintContents() translated the context so the origin is aligned with frameRect().location().
    // Shift it back to align with the plugin origin.
    GraphicsContextStateSaver stateSaver(context);

    auto widgetOrigin = frameRect().location();
    context.translate(widgetOrigin);

    auto paintRect = dirtyRect;
    paintRect.moveBy(-widgetOrigin);

    m_plugin->paint(context, paintRect);
}

void PluginView::frameRectsChanged()
{
    Widget::frameRectsChanged();
    viewGeometryDidChange();
}

void PluginView::clipRectChanged()
{
    viewGeometryDidChange();
}

void PluginView::setParent(ScrollView* scrollView)
{
    Widget::setParent(scrollView);

    if (scrollView)
        initializePlugin();
}

unsigned PluginView::countFindMatches(const String& target, WebCore::FindOptions options, unsigned maxMatchCount)
{
    if (!m_isInitialized)
        return 0;

    return m_plugin->countFindMatches(target, options, maxMatchCount);
}

bool PluginView::findString(const String& target, WebCore::FindOptions options, unsigned maxMatchCount)
{
    if (!m_isInitialized)
        return false;

    return m_plugin->findString(target, options, maxMatchCount);
}

Vector<FloatRect> PluginView::rectsForTextMatchesInRect(const IntRect& clipRect) const
{
    if (!m_isInitialized)
        return { };

    return m_plugin->rectsForTextMatchesInRect(clipRect);
}

bool PluginView::drawsFindOverlay() const
{
    if (!m_isInitialized)
        return false;

    return m_plugin->drawsFindOverlay();
}

RefPtr<TextIndicator> PluginView::textIndicatorForCurrentSelection(OptionSet<WebCore::TextIndicatorOption> options, WebCore::TextIndicatorPresentationTransition transition)
{
    if (!m_isInitialized)
        return { };

    return m_plugin->textIndicatorForCurrentSelection(options, transition);
}

Vector<WebFoundTextRange::PDFData> PluginView::findTextMatches(const String& target, WebCore::FindOptions options)
{
    if (!m_isInitialized)
        return { };

    return m_plugin->findTextMatches(target, options);
}

Vector<WebCore::FloatRect> PluginView::rectsForTextMatchesInRect(const Vector<WebFoundTextRange::PDFData>& matches, const WebCore::IntRect& clipRect)
{
    if (!m_isInitialized)
        return { };

    return m_plugin->rectsForTextMatchesInRect(matches, clipRect);
}

RefPtr<WebCore::TextIndicator> PluginView::textIndicatorForTextMatch(const WebFoundTextRange::PDFData& match, WebCore::TextIndicatorPresentationTransition transition)
{
    if (!m_isInitialized)
        return { };

    return m_plugin->textIndicatorForTextMatch(match, transition);
}

void PluginView::scrollToRevealTextMatch(const WebFoundTextRange::PDFData& match)
{
    if (!m_isInitialized)
        return;

    return m_plugin->scrollToRevealTextMatch(match);
}

String PluginView::fullDocumentString() const
{
    if (!m_isInitialized)
        return { };

    return m_plugin->fullDocumentString();
}

String PluginView::selectionString() const
{
    if (!m_isInitialized)
        return String();

    return m_plugin->selectionString();
}

std::pair<String, String> PluginView::stringsBeforeAndAfterSelection(int characterCount) const
{
    if (!m_isInitialized)
        return { };

    return m_plugin->stringsBeforeAndAfterSelection(characterCount);
}

void PluginView::handleEvent(Event& event)
{
    if (!m_isInitialized)
        return;

    const CheckedPtr currentEvent = WebPage::currentEvent();
    if (!currentEvent)
        return;

    bool didHandleEvent = false;
    if ((event.type() == eventNames().mousemoveEvent && currentEvent->type() == WebEventType::MouseMove)
        || (event.type() == eventNames().mousedownEvent && currentEvent->type() == WebEventType::MouseDown)
        || (event.type() == eventNames().mouseupEvent && currentEvent->type() == WebEventType::MouseUp)) {
        // FIXME: Clicking in a scroll bar should not change focus.
        RefPtr frame = this->frame();
        if (currentEvent->type() == WebEventType::MouseDown) {
            focusPluginElement();
            frame->eventHandler().setCapturingMouseEventsElement(m_pluginElement.copyRef());
        } else if (currentEvent->type() == WebEventType::MouseUp)
            frame->eventHandler().setCapturingMouseEventsElement(nullptr);

        didHandleEvent = m_plugin->handleMouseEvent(downcast<WebMouseEvent>(*currentEvent));
    } else if ((event.type() == eventNames().wheelEvent || event.type() == eventNames().mousewheelEvent) && currentEvent->type() == WebEventType::Wheel)
        didHandleEvent = m_plugin->handleWheelEvent(downcast<WebWheelEvent>(*currentEvent));
    else if (event.type() == eventNames().mouseoverEvent && currentEvent->type() == WebEventType::MouseMove)
        didHandleEvent = m_plugin->handleMouseEnterEvent(downcast<WebMouseEvent>(*currentEvent));
    else if (event.type() == eventNames().mouseoutEvent && currentEvent->type() == WebEventType::MouseMove)
        didHandleEvent = m_plugin->handleMouseLeaveEvent(downcast<WebMouseEvent>(*currentEvent));
    else if (event.type() == eventNames().contextmenuEvent && currentEvent->type() == WebEventType::MouseDown)
        didHandleEvent = m_plugin->handleContextMenuEvent(downcast<WebMouseEvent>(*currentEvent));
    else if ((event.type() == eventNames().keydownEvent && currentEvent->type() == WebEventType::KeyDown)
        || (event.type() == eventNames().keyupEvent && currentEvent->type() == WebEventType::KeyUp))
        didHandleEvent = m_plugin->handleKeyboardEvent(downcast<WebKeyboardEvent>(*currentEvent));

    if (didHandleEvent)
        event.setDefaultHandled();
}

bool PluginView::handleEditingCommand(const String& commandName, const String& argument)
{
    if (!m_isInitialized)
        return false;

    return m_plugin->handleEditingCommand(commandName, argument);
}

bool PluginView::isEditingCommandEnabled(const String& commandName)
{
    if (!m_isInitialized)
        return false;

    return m_plugin->isEditingCommandEnabled(commandName);
}

bool PluginView::shouldAllowNavigationFromDrags() const
{
    return true;
}

void PluginView::willDetachRenderer()
{
    if (!m_isInitialized)
        return;

    m_plugin->willDetachRenderer();
}

ScrollableArea* PluginView::scrollableArea() const
{
    if (!m_isInitialized)
        return nullptr;

    return m_plugin.ptr();
}

bool PluginView::usesAsyncScrolling() const
{
    if (!m_isInitialized)
        return false;

    return m_plugin->usesAsyncScrolling();
}

std::optional<ScrollingNodeID> PluginView::scrollingNodeID() const
{
    if (!m_isInitialized)
        return std::nullopt;

    return m_plugin->scrollingNodeID();
}

void PluginView::willAttachScrollingNode()
{
    if (!m_isInitialized)
        return;

    return m_plugin->willAttachScrollingNode();
}

void PluginView::didAttachScrollingNode()
{
    if (!m_isInitialized)
        return;

    return m_plugin->didAttachScrollingNode();
}

RefPtr<FragmentedSharedBuffer> PluginView::liveResourceData() const
{
    if (!m_isInitialized) {
        if (m_manualStreamState == ManualStreamState::Finished)
            return m_manualStreamData.get();

        return nullptr;
    }

    return m_plugin->liveResourceData();
}

bool PluginView::performDictionaryLookupAtLocation(const WebCore::FloatPoint& point)
{
    if (!m_isInitialized)
        return false;

    return m_plugin->performDictionaryLookupAtLocation(point);
}

void PluginView::notifyWidget(WidgetNotification notification)
{
    switch (notification) {
    case WidgetNotification::WillPaintFlattened:
        if (shouldCreateTransientPaintingSnapshot())
            m_transientPaintingSnapshot = m_plugin->snapshot();
        break;
    case WidgetNotification::DidPaintFlattened:
        m_transientPaintingSnapshot = nullptr;
        break;
    }
}

void PluginView::show()
{
    bool wasVisible = isVisible();

    setSelfVisible(true);

    if (!wasVisible)
        viewVisibilityDidChange();

    Widget::show();
}

void PluginView::hide()
{
    bool wasVisible = isVisible();

    setSelfVisible(false);

    if (wasVisible)
        viewVisibilityDidChange();

    Widget::hide();
}

void PluginView::setParentVisible(bool isVisible)
{
    if (isParentVisible() == isVisible)
        return;

    Widget::setParentVisible(isVisible);
    viewVisibilityDidChange();
}

bool PluginView::transformsAffectFrameRect()
{
    return false;
}

void PluginView::viewGeometryDidChange()
{
    if (!m_isInitialized || !parent())
        return;

    ASSERT(frame());

    // FIXME: We still don't get the right coordinates for transformed plugins.
    auto pluginToRootViewTransform = [this, protectedThis = Ref { *this }] {
        AffineTransform transform;

        RefPtr frame = this->frame();
        if (frame->isMainFrame() && m_plugin->isFullFramePlugin() && !m_plugin->handlesPageScaleFactor())
            return transform;

        float pageScaleFactor = frame->page() ? frame->page()->pageScaleFactor() : 1;
        IntPoint scaledFrameRectLocation { frameRect().location().scaled(pageScaleFactor) };
        IntPoint scaledLocationInRootViewCoordinates { protectedParent()->contentsToRootView(scaledFrameRectLocation) };

        transform.translate(scaledLocationInRootViewCoordinates);
        transform.scale(pageScaleFactor);

        return transform;
    }();

    m_plugin->geometryDidChange(size(), pluginToRootViewTransform);
}

void PluginView::frameViewLayoutOrVisualViewportChanged(const IntRect& unobscuredContentRect)
{
    if (!m_isInitialized || !parent())
        return;

    m_plugin->frameViewLayoutOrVisualViewportChanged(unobscuredContentRect);
}

void PluginView::viewVisibilityDidChange()
{
    if (!m_isInitialized || !parent())
        return;

    m_plugin->visibilityDidChange(isVisible());
}

IntRect PluginView::clipRectInWindowCoordinates() const
{
    // Get the frame rect in window coordinates.
    IntRect frameRectInWindowCoordinates = protectedParent()->contentsToWindow(frameRect());

    RefPtr frame = this->frame();

    // Get the window clip rect for the plugin element (in window coordinates).
    IntRect windowClipRect = frame->protectedView()->windowClipRectForFrameOwner(m_pluginElement.ptr(), true);

    // Intersect the two rects to get the view clip rect in window coordinates.
    frameRectInWindowCoordinates.intersect(windowClipRect);

    return frameRectInWindowCoordinates;
}

void PluginView::focusPluginElement()
{
    RefPtr frame = this->frame();
    ASSERT(frame);

    Ref pluginElement = m_pluginElement;
    if (RefPtr page = frame->page())
        page->focusController().setFocusedElement(pluginElement.ptr(), *frame);
    else
        frame->protectedDocument()->setFocusedElement(pluginElement.ptr());
}

void PluginView::pendingResourceRequestTimerFired()
{
    ASSERT(m_pendingResourceRequest);
    auto request = std::exchange(m_pendingResourceRequest, nullptr);

    if (request->url().protocolIsJavaScript())
        return;

    // This protector is needed to make sure the PluginView is not destroyed while it is still needed.
    Ref protectedThis { *this };

    ASSERT(!m_stream);
    Ref stream = PluginView::Stream::create(this, *request);
    m_stream = stream.copyRef();
    stream->start();
}

void PluginView::redeliverManualStream()
{
    if (m_manualStreamState == ManualStreamState::Initial) {
        // Nothing to do.
        return;
    }

    if (m_manualStreamState == ManualStreamState::Failed) {
        manualLoadDidFail();
        return;
    }

    // Deliver the response.
    manualLoadDidReceiveResponse(m_manualStreamResponse);

    // Deliver the data.
    if (m_manualStreamData) {
        m_manualStreamData.take()->forEachSegmentAsSharedBuffer([&](auto&& buffer) {
            manualLoadDidReceiveData(buffer);
        });
    }

    if (m_manualStreamState == ManualStreamState::Finished)
        manualLoadDidFinishLoading();
}

RenderEmbeddedObject* PluginView::renderer() const
{
    return dynamicDowncast<RenderEmbeddedObject>(m_pluginElement->renderer());
}

void PluginView::invalidateRect(const IntRect& dirtyRect)
{
    if (!parent() || !m_isInitialized)
        return;

#if PLATFORM(COCOA)
    if (m_plugin->isComposited())
        return;
#endif

    CheckedPtr renderer = this->renderer();
    if (!renderer)
        return;

    auto contentRect = dirtyRect;
    contentRect.move(renderer->borderLeft() + renderer->paddingLeft(), renderer->borderTop() + renderer->paddingTop());
    renderer->repaintRectangle(contentRect);
}

void PluginView::loadMainResource()
{
    auto referrer = SecurityPolicy::generateReferrerHeader(frame()->document()->referrerPolicy(), m_mainResourceURL, frame()->loader().outgoingReferrerURL(), OriginAccessPatternsForWebProcess::singleton());
    if (referrer.isEmpty())
        referrer = { };

    ASSERT(!m_pendingResourceRequest);
    m_pendingResourceRequest = WTF::makeUnique<ResourceRequest>(URL { m_mainResourceURL }, referrer);
    m_pendingResourceRequestTimer.startOneShot(0_s);
}

bool PluginView::shouldCreateTransientPaintingSnapshot() const
{
    if (!m_isInitialized)
        return false;

    if (RefPtr frameView = frame()->view()) {
        if (frameView->paintBehavior().containsAny({ PaintBehavior::SelectionOnly, PaintBehavior::SelectionAndBackgroundsOnly, PaintBehavior::ForceBlackText })) {
            // This paint behavior is used when drawing the find indicator and there's no need to
            // snapshot plug-ins, because they can never be painted as part of the find indicator.
            return false;
        }
    }

    return m_plugin->shouldCreateTransientPaintingSnapshot();
}

bool PluginView::isBeingDestroyed() const
{
    return m_plugin->isBeingDestroyed();
}

void PluginView::releaseMemory()
{
    m_plugin->releaseMemory();
}

RetainPtr<PDFDocument> PluginView::pdfDocumentForPrinting() const
{
    return m_plugin->pdfDocument();
}

WebCore::FloatSize PluginView::pdfDocumentSizeForPrinting() const
{
    return m_plugin->pdfDocumentSizeForPrinting();
}

id PluginView::accessibilityHitTest(const WebCore::IntPoint& point) const
{
    return m_plugin->accessibilityHitTest(point);
}

bool PluginView::performImmediateActionHitTestAtLocation(const WebCore::FloatPoint& point, WebHitTestResultData& data) const
{
    return m_plugin->performImmediateActionHitTestAtLocation(point, data);
}

WebCore::FloatRect PluginView::rectForSelectionInRootView(PDFSelection *selection) const
{
    return m_plugin->rectForSelectionInRootView(selection);
}

bool PluginView::isUsingUISideCompositing() const
{
    return protectedWebPage()->isUsingUISideCompositing();
}

void PluginView::didChangeSettings()
{
    m_plugin->didChangeSettings();
}

void PluginView::windowActivityDidChange()
{
    m_plugin->windowActivityDidChange();
}

void PluginView::didChangeIsInWindow()
{
    m_plugin->didChangeIsInWindow();
}

void PluginView::didSameDocumentNavigationForFrame(WebFrame& frame)
{
    if (!m_isInitialized)
        return;

    return m_plugin->didSameDocumentNavigationForFrame(frame);
}

bool PluginView::sendEditingCommandToPDFForTesting(const String& commandName, const String& argument)
{
    return m_plugin->handleEditingCommand(commandName, argument);
}

void PluginView::setPDFDisplayModeForTesting(const String& mode)
{
    m_plugin->setPDFDisplayModeForTesting(mode);
}

void PluginView::unlockPDFDocumentForTesting(const String& password)
{
    m_plugin->attemptToUnlockPDF(password);
}

Vector<WebCore::FloatRect> PluginView::pdfAnnotationRectsForTesting() const
{
    return m_plugin->annotationRectsForTesting();
}

void PluginView::setPDFTextAnnotationValueForTesting(unsigned pageIndex, unsigned annotationIndex, const String& value)
{
    return m_plugin->setTextAnnotationValueForTesting(pageIndex, annotationIndex, value);
}

void PluginView::registerPDFTestCallback(RefPtr<VoidCallback>&& callback)
{
    m_plugin->registerPDFTest(WTFMove(callback));
}

PDFPluginIdentifier PluginView::pdfPluginIdentifier() const
{
    return m_plugin->identifier();
}

void PluginView::openWithPreview(CompletionHandler<void(const String&, std::optional<FrameInfoData>&&, std::span<const uint8_t>)>&& completionHandler)
{
    m_plugin->openWithPreview(WTFMove(completionHandler));
}

#if PLATFORM(IOS_FAMILY)

void PluginView::setSelectionRange(FloatPoint pointInRootView, TextGranularity granularity)
{
    m_plugin->setSelectionRange(pointInRootView, granularity);
}

SelectionWasFlipped PluginView::moveSelectionEndpoint(FloatPoint pointInRootView, SelectionEndpoint endpoint)
{
    return m_plugin->moveSelectionEndpoint(pointInRootView, endpoint);
}

SelectionEndpoint PluginView::extendInitialSelection(FloatPoint pointInRootView, TextGranularity granularity)
{
    return m_plugin->extendInitialSelection(pointInRootView, granularity);
}

DocumentEditingContext PluginView::documentEditingContext(DocumentEditingContextRequest&& request) const
{
    return m_plugin->documentEditingContext(WTFMove(request));
}

void PluginView::clearSelection()
{
    m_plugin->clearSelection();
}

std::pair<URL, FloatRect> PluginView::linkURLAndBoundsAtPoint(FloatPoint pointInRootView) const
{
    return m_plugin->linkURLAndBoundsAtPoint(pointInRootView);
}

std::tuple<URL, FloatRect, RefPtr<TextIndicator>> PluginView::linkDataAtPoint(FloatPoint pointInRootView)
{
    return m_plugin->linkDataAtPoint(pointInRootView);
}

std::optional<FloatRect> PluginView::highlightRectForTapAtPoint(FloatPoint pointInRootView) const
{
    return m_plugin->highlightRectForTapAtPoint(pointInRootView);
}

void PluginView::handleSyntheticClick(PlatformMouseEvent&& event)
{
    m_plugin->handleSyntheticClick(WTFMove(event));
}

CursorContext PluginView::cursorContext(FloatPoint pointInRootView) const
{
    return m_plugin->cursorContext(pointInRootView);
}

#endif // PLATFORM(IOS_FAMILY)

bool PluginView::populateEditorStateIfNeeded(EditorState& state) const
{
    return m_plugin->populateEditorStateIfNeeded(state);
}

WebCore::FloatRect PluginView::absoluteBoundingRectForSmartMagnificationAtPoint(WebCore::FloatPoint point) const
{
    if (!m_isInitialized)
        return { };

    return m_plugin->absoluteBoundingRectForSmartMagnificationAtPoint(point);
}

void PluginView::updateDocumentForPluginSizingBehavior()
{
    if (!m_plugin->shouldSizeToFitContent())
        return;
    // The styles in PluginDocumentParser are constructed to respond to this class.
    if (RefPtr documentElement = m_pluginElement->protectedDocument()->documentElement())
        documentElement->setAttributeWithoutSynchronization(HTMLNames::classAttr, "plugin-fits-content"_s);
}

bool PluginView::pluginHandlesPageScaleFactor() const
{
    if (!m_isInitialized)
        return false;

    return m_plugin->handlesPageScaleFactor();
}

bool PluginView::pluginDelegatesScrollingToMainFrame() const
{
    if (!m_isInitialized)
        return false;

    return m_plugin->delegatesScrollingToMainFrame();
}

} // namespace WebKit

#endif
