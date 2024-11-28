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

#include "config.h"
#include "PluginView.h"

#if ENABLE(PDF_PLUGIN)

#include "FrameInfoData.h"
#include "PDFPlugin.h"
#include "UnifiedPDFPlugin.h"
#include "WebCoreArgumentCoders.h"
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
#include <WebCore/CookieJar.h>
#include <WebCore/Credential.h>
#include <WebCore/CredentialStorage.h>
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

    WebProcess::singleton().webLoaderStrategy().schedulePluginStreamLoad(*frame, *this, ResourceRequest {m_request}, [this, protectedThis = Ref { *this }](RefPtr<NetscapePlugInStreamLoader>&& loader) {
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
    m_pluginView->protectedPlugin()->streamDidReceiveResponse(response);
}

void PluginView::Stream::didReceiveData(NetscapePlugInStreamLoader*, const SharedBuffer& buffer)
{
    m_pluginView->protectedPlugin()->streamDidReceiveData(buffer);
}

void PluginView::Stream::didFail(NetscapePlugInStreamLoader*, const ResourceError&)
{
    // Calling streamDidFail could cause us to be deleted, so we hold on to a reference here.
    Ref protectedThis { *this };

    // We only want to call streamDidFail if the stream was not explicitly cancelled by the plug-in.
    if (!m_streamWasCancelled)
        m_pluginView->protectedPlugin()->streamDidFail();

    ASSERT(m_pluginView->m_stream == this);
    m_pluginView->m_stream = nullptr;
    m_pluginView = nullptr;
}

void PluginView::Stream::didFinishLoading(NetscapePlugInStreamLoader*)
{
    // Calling streamDidFinishLoading could cause us to be deleted, so we hold on to a reference here.
    Ref protectedThis { *this };

    m_pluginView->protectedPlugin()->streamDidFinishLoading();

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
    , m_pendingResourceRequestTimer(RunLoop::main(), this, &PluginView::pendingResourceRequestTimerFired)
{
    protectedPlugin()->startLoading();
    m_webPage->addPluginView(*this);
}

PluginView::~PluginView()
{
    if (RefPtr webPage = m_webPage.get())
        webPage->removePluginView(*this);
    if (RefPtr stream = m_stream)
        stream->cancel();
    protectedPlugin()->destroy();
}

RefPtr<WebPage> PluginView::protectedWebPage() const
{
    return m_webPage.get();
}

LocalFrame* PluginView::frame() const
{
    return m_pluginElement->document().frame();
}

void PluginView::manualLoadDidReceiveResponse(const ResourceResponse& response)
{
    if (!m_isInitialized) {
        ASSERT(m_manualStreamState == ManualStreamState::Initial);
        m_manualStreamState = ManualStreamState::HasReceivedResponse;
        m_manualStreamResponse = response;
        return;
    }

    protectedPlugin()->streamDidReceiveResponse(response);
}

void PluginView::manualLoadDidReceiveData(const SharedBuffer& buffer)
{
    if (!m_isInitialized) {
        ASSERT(m_manualStreamState == ManualStreamState::HasReceivedResponse);
        m_manualStreamData.append(buffer);
        return;
    }

    protectedPlugin()->streamDidReceiveData(buffer);
}

void PluginView::manualLoadDidFinishLoading()
{
    if (!m_isInitialized) {
        ASSERT(m_manualStreamState == ManualStreamState::HasReceivedResponse);
        m_manualStreamState = ManualStreamState::Finished;
        return;
    }

    protectedPlugin()->streamDidFinishLoading();
}

Ref<WebCore::HTMLPlugInElement> PluginView::protectedPluginElement() const
{
    return m_pluginElement;
}

void PluginView::layerHostingStrategyDidChange()
{
    if (!m_isInitialized)
        return;

    // This ensures that we update RenderLayers and compositing when the result of RenderEmbeddedObject::requiresLayer() changes.
    protectedPluginElement()->invalidateStyleAndLayerComposition();
}

void PluginView::manualLoadDidFail()
{
    if (!m_isInitialized) {
        m_manualStreamState = ManualStreamState::Finished;
        m_manualStreamData.reset();
        return;
    }

    protectedPlugin()->streamDidFail();
}

void PluginView::topContentInsetDidChange()
{
    viewGeometryDidChange();
}

void PluginView::didBeginMagnificationGesture()
{
    if (!m_isInitialized)
        return;

    protectedPlugin()->didBeginMagnificationGesture();
}

void PluginView::didEndMagnificationGesture()
{
    if (!m_isInitialized)
        return;

    protectedPlugin()->didEndMagnificationGesture();
}

void PluginView::setPageScaleFactor(double scaleFactor, std::optional<IntPoint> origin)
{
    if (!m_isInitialized)
        return;

    pluginScaleFactorDidChange();
    protectedPlugin()->setPageScaleFactor(scaleFactor, origin);
}

double PluginView::pageScaleFactor() const
{
    return protectedPlugin()->scaleFactor();
}

void PluginView::pluginScaleFactorDidChange()
{
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

    protectedPlugin()->deviceScaleFactorChanged(scaleFactor);
}

id PluginView::accessibilityAssociatedPluginParentForElement(Element* element) const
{
    return protectedPlugin()->accessibilityAssociatedPluginParentForElement(element);
}

id PluginView::accessibilityObject() const
{
    if (!m_isInitialized)
        return nil;

    return protectedPlugin()->accessibilityObject();
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
        protectedPluginElement()->invalidateStyleAndLayerComposition();
    }
    plugin->visibilityDidChange(isVisible());
#endif

    if (RefPtr frame = this->frame()) {
        if (RefPtr frameView = frame->view())
            frameView->setNeedsLayoutAfterViewConfigurationChange();
        if (frame->isMainFrame() && plugin->isFullFramePlugin())
            WebFrame::fromCoreFrame(*frame)->protectedPage()->send(Messages::WebPageProxy::MainFramePluginHandlesPageScaleGestureDidChange(true, plugin->minScaleFactor(), plugin->maxScaleFactor()));
    }
}

Ref<PDFPluginBase> PluginView::protectedPlugin() const
{
    return m_plugin;
}

PluginLayerHostingStrategy PluginView::layerHostingStrategy() const
{
    if (!m_isInitialized)
        return PluginLayerHostingStrategy::None;

    return protectedPlugin()->layerHostingStrategy();
}

#if PLATFORM(COCOA)

PlatformLayer* PluginView::platformLayer() const
{
    if (!m_isInitialized)
        return nullptr;

#if ENABLE(LEGACY_PDFKIT_PLUGIN)
    Ref plugin = m_plugin;
    if (plugin->layerHostingStrategy() == PluginLayerHostingStrategy::PlatformLayer)
        return plugin->platformLayer();
#endif

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

    return protectedPlugin()->horizontalScrollbar();
}

Scrollbar* PluginView::verticalScrollbar()
{
    if (!m_isInitialized)
        return nullptr;

    return protectedPlugin()->verticalScrollbar();
}

bool PluginView::wantsWheelEvents()
{
    if (!m_isInitialized)
        return false;

    return protectedPlugin()->wantsWheelEvents();
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
            protectedPlugin()->updateControlTints(context);
        return;
    }

    // FIXME: We should try to intersect the dirty rect with the plug-in's clip rect here.
    if (frameRect().isEmpty())
        return;

    if (m_transientPaintingSnapshot) {
        if (!context.hasPlatformContext()) {
            RefPtr image = m_transientPaintingSnapshot->createImage();
            if (!image)
                return;
            context.drawImage(*image, frameRect());
        } else {
            auto deviceScaleFactor = 1;
            if (RefPtr page = m_pluginElement->document().page())
                deviceScaleFactor = page->deviceScaleFactor();
            m_transientPaintingSnapshot->paint(context, deviceScaleFactor, frameRect().location(), m_transientPaintingSnapshot->bounds());
        }
        return;
    }

    bool isSnapshotting = [&]() {
        RefPtr frameView = frame()->view();
        if (!frameView)
            return false;

        return frameView->paintBehavior().contains(PaintBehavior::FlattenCompositingLayers);
    }();

    if (!isSnapshotting && protectedPlugin()->layerHostingStrategy() == PluginLayerHostingStrategy::GraphicsLayer)
        return;

    // RenderWidget::paintContents() translated the context so the origin is aligned with frameRect().location().
    // Shift it back to align with the plugin origin.
    GraphicsContextStateSaver stateSaver(context);

    auto widgetOrigin = frameRect().location();
    context.translate(widgetOrigin);

    auto paintRect = dirtyRect;
    paintRect.moveBy(-widgetOrigin);

    protectedPlugin()->paint(context, paintRect);
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

    return protectedPlugin()->countFindMatches(target, options, maxMatchCount);
}

bool PluginView::findString(const String& target, WebCore::FindOptions options, unsigned maxMatchCount)
{
    if (!m_isInitialized)
        return false;

    return protectedPlugin()->findString(target, options, maxMatchCount);
}

Vector<FloatRect> PluginView::rectsForTextMatchesInRect(const IntRect& clipRect) const
{
    if (!m_isInitialized)
        return { };

    return protectedPlugin()->rectsForTextMatchesInRect(clipRect);
}

bool PluginView::drawsFindOverlay() const
{
    if (!m_isInitialized)
        return false;

    return protectedPlugin()->drawsFindOverlay();
}

RefPtr<TextIndicator> PluginView::textIndicatorForCurrentSelection(OptionSet<WebCore::TextIndicatorOption> options, WebCore::TextIndicatorPresentationTransition transition)
{
    if (!m_isInitialized)
        return { };

    return protectedPlugin()->textIndicatorForCurrentSelection(options, transition);
}

Vector<WebFoundTextRange::PDFData> PluginView::findTextMatches(const String& target, WebCore::FindOptions options)
{
    if (!m_isInitialized)
        return { };

    return protectedPlugin()->findTextMatches(target, options);
}

Vector<FloatRect> PluginView::rectsForTextMatch(const WebFoundTextRange::PDFData& match)
{
    if (!m_isInitialized)
        return { };

    return protectedPlugin()->rectsForTextMatch(match);
}

RefPtr<WebCore::TextIndicator> PluginView::textIndicatorForTextMatch(const WebFoundTextRange::PDFData& match, WebCore::TextIndicatorPresentationTransition transition)
{
    if (!m_isInitialized)
        return { };

    return protectedPlugin()->textIndicatorForTextMatch(match, transition);
}

void PluginView::scrollToRevealTextMatch(const WebFoundTextRange::PDFData& match)
{
    if (!m_isInitialized)
        return;

    return protectedPlugin()->scrollToRevealTextMatch(match);
}

String PluginView::selectionString() const
{
    if (!m_isInitialized)
        return String();

    return protectedPlugin()->selectionString();
}

void PluginView::handleEvent(Event& event)
{
    if (!m_isInitialized)
        return;

    const WebEvent* currentEvent = WebPage::currentEvent();
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
            frame->checkedEventHandler()->setCapturingMouseEventsElement(m_pluginElement.copyRef());
        } else if (currentEvent->type() == WebEventType::MouseUp)
            frame->checkedEventHandler()->setCapturingMouseEventsElement(nullptr);

        didHandleEvent = protectedPlugin()->handleMouseEvent(static_cast<const WebMouseEvent&>(*currentEvent));
    } else if ((event.type() == eventNames().wheelEvent || event.type() == eventNames().mousewheelEvent) && currentEvent->type() == WebEventType::Wheel)
        didHandleEvent = protectedPlugin()->handleWheelEvent(static_cast<const WebWheelEvent&>(*currentEvent));
    else if (event.type() == eventNames().mouseoverEvent && currentEvent->type() == WebEventType::MouseMove)
        didHandleEvent = protectedPlugin()->handleMouseEnterEvent(static_cast<const WebMouseEvent&>(*currentEvent));
    else if (event.type() == eventNames().mouseoutEvent && currentEvent->type() == WebEventType::MouseMove)
        didHandleEvent = protectedPlugin()->handleMouseLeaveEvent(static_cast<const WebMouseEvent&>(*currentEvent));
    else if (event.type() == eventNames().contextmenuEvent && currentEvent->type() == WebEventType::MouseDown)
        didHandleEvent = protectedPlugin()->handleContextMenuEvent(static_cast<const WebMouseEvent&>(*currentEvent));
    else if ((event.type() == eventNames().keydownEvent && currentEvent->type() == WebEventType::KeyDown)
        || (event.type() == eventNames().keyupEvent && currentEvent->type() == WebEventType::KeyUp))
        didHandleEvent = protectedPlugin()->handleKeyboardEvent(static_cast<const WebKeyboardEvent&>(*currentEvent));

    if (didHandleEvent)
        event.setDefaultHandled();
}

bool PluginView::handleEditingCommand(const String& commandName, const String& argument)
{
    if (!m_isInitialized)
        return false;

    return protectedPlugin()->handleEditingCommand(commandName, argument);
}

bool PluginView::isEditingCommandEnabled(const String& commandName)
{
    if (!m_isInitialized)
        return false;

    return protectedPlugin()->isEditingCommandEnabled(commandName);
}

bool PluginView::shouldAllowNavigationFromDrags() const
{
    return true;
}

void PluginView::willDetachRenderer()
{
    if (!m_isInitialized)
        return;

    protectedPlugin()->willDetachRenderer();
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

    return protectedPlugin()->usesAsyncScrolling();
}

std::optional<ScrollingNodeID> PluginView::scrollingNodeID() const
{
    if (!m_isInitialized)
        return std::nullopt;

    return protectedPlugin()->scrollingNodeID();
}

void PluginView::willAttachScrollingNode()
{
    if (!m_isInitialized)
        return;

    return protectedPlugin()->willAttachScrollingNode();
}

void PluginView::didAttachScrollingNode()
{
    if (!m_isInitialized)
        return;

    return protectedPlugin()->didAttachScrollingNode();
}

RefPtr<FragmentedSharedBuffer> PluginView::liveResourceData() const
{
    if (!m_isInitialized) {
        if (m_manualStreamState == ManualStreamState::Finished)
            return m_manualStreamData.get();

        return nullptr;
    }

    return protectedPlugin()->liveResourceData();
}

bool PluginView::performDictionaryLookupAtLocation(const WebCore::FloatPoint& point)
{
    if (!m_isInitialized)
        return false;

    return protectedPlugin()->performDictionaryLookupAtLocation(point);
}

void PluginView::notifyWidget(WidgetNotification notification)
{
    switch (notification) {
    case WillPaintFlattened:
        if (shouldCreateTransientPaintingSnapshot())
            m_transientPaintingSnapshot = protectedPlugin()->snapshot();
        break;
    case DidPaintFlattened:
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
    float pageScaleFactor = frame()->page() ? frame()->page()->pageScaleFactor() : 1;

    IntPoint scaledFrameRectLocation(frameRect().location().x() * pageScaleFactor, frameRect().location().y() * pageScaleFactor);
    IntPoint scaledLocationInRootViewCoordinates(protectedParent()->contentsToRootView(scaledFrameRectLocation));

    // FIXME: We still don't get the right coordinates for transformed plugins.
    AffineTransform transform;
    transform.translate(scaledLocationInRootViewCoordinates.x(), scaledLocationInRootViewCoordinates.y());
    transform.scale(pageScaleFactor);

    protectedPlugin()->geometryDidChange(size(), transform);
}

void PluginView::viewVisibilityDidChange()
{
    if (!m_isInitialized || !parent())
        return;

    protectedPlugin()->visibilityDidChange(isVisible());
}

IntRect PluginView::clipRectInWindowCoordinates() const
{
    // Get the frame rect in window coordinates.
    IntRect frameRectInWindowCoordinates = protectedParent()->contentsToWindow(frameRect());

    RefPtr frame = this->frame();

    // Get the window clip rect for the plugin element (in window coordinates).
    IntRect windowClipRect = frame->protectedView()->windowClipRectForFrameOwner(protectedPluginElement().ptr(), true);

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
        page->checkedFocusController()->setFocusedElement(pluginElement.ptr(), *frame);
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

CheckedPtr<RenderEmbeddedObject> PluginView::checkedRenderer() const
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

    CheckedPtr renderer = checkedRenderer();
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
    m_pendingResourceRequest = WTF::makeUnique<ResourceRequest>(m_mainResourceURL, referrer);
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

    return protectedPlugin()->shouldCreateTransientPaintingSnapshot();
}

bool PluginView::isBeingDestroyed() const
{
    return protectedPlugin()->isBeingDestroyed();
}

void PluginView::releaseMemory()
{
    protectedPlugin()->releaseMemory();
}

RetainPtr<PDFDocument> PluginView::pdfDocumentForPrinting() const
{
    return protectedPlugin()->pdfDocument();
}

WebCore::FloatSize PluginView::pdfDocumentSizeForPrinting() const
{
    return protectedPlugin()->pdfDocumentSizeForPrinting();
}

id PluginView::accessibilityHitTest(const WebCore::IntPoint& point) const
{
    return protectedPlugin()->accessibilityHitTest(point);
}

bool PluginView::performImmediateActionHitTestAtLocation(const WebCore::FloatPoint& point, WebHitTestResultData& data) const
{
    return protectedPlugin()->performImmediateActionHitTestAtLocation(point, data);
}

WebCore::FloatRect PluginView::rectForSelectionInRootView(PDFSelection *selection) const
{
    return protectedPlugin()->rectForSelectionInRootView(selection);
}

bool PluginView::isUsingUISideCompositing() const
{
    return protectedWebPage()->isUsingUISideCompositing();
}

void PluginView::didChangeSettings()
{
    protectedPlugin()->didChangeSettings();
}

void PluginView::windowActivityDidChange()
{
    protectedPlugin()->windowActivityDidChange();
}

void PluginView::didChangeIsInWindow()
{
    protectedPlugin()->didChangeIsInWindow();
}

void PluginView::didSameDocumentNavigationForFrame(WebFrame& frame)
{
    if (!m_isInitialized)
        return;

    return protectedPlugin()->didSameDocumentNavigationForFrame(frame);
}

bool PluginView::sendEditingCommandToPDFForTesting(const String& commandName, const String& argument)
{
    return protectedPlugin()->handleEditingCommand(commandName, argument);
}

void PluginView::setPDFDisplayModeForTesting(const String& mode)
{
    protectedPlugin()->setPDFDisplayModeForTesting(mode);
}

void PluginView::unlockPDFDocumentForTesting(const String& password)
{
    protectedPlugin()->attemptToUnlockPDF(password);
}

Vector<WebCore::FloatRect> PluginView::pdfAnnotationRectsForTesting() const
{
    return protectedPlugin()->annotationRectsForTesting();
}

void PluginView::setPDFTextAnnotationValueForTesting(unsigned pageIndex, unsigned annotationIndex, const String& value)
{
    return protectedPlugin()->setTextAnnotationValueForTesting(pageIndex, annotationIndex, value);
}

void PluginView::registerPDFTestCallback(RefPtr<VoidCallback>&& callback)
{
    protectedPlugin()->registerPDFTest(WTFMove(callback));
}

PDFPluginIdentifier PluginView::pdfPluginIdentifier() const
{
    return protectedPlugin()->identifier();
}

void PluginView::openWithPreview(CompletionHandler<void(const String&, FrameInfoData&&, std::span<const uint8_t>, const String&)>&& completionHandler)
{
    protectedPlugin()->openWithPreview(WTFMove(completionHandler));
}

#if PLATFORM(IOS_FAMILY)
void PluginView::pluginDidInstallPDFDocument(double initialScale)
{
    protectedWebPage()->pluginDidInstallPDFDocument(initialScale);
}
#endif

} // namespace WebKit

#endif
