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

#if ENABLE(PDF_PLUGIN)

#include "CursorContext.h"
#include "PDFPluginIdentifier.h"
#include "WebFoundTextRange.h"
#include <WebCore/FindOptions.h>
#include <WebCore/PluginViewBase.h>
#include <WebCore/ResourceResponse.h>
#include <WebCore/SharedBuffer.h>
#include <WebCore/TextIndicator.h>
#include <WebCore/Timer.h>
#include <memory>
#include <wtf/CompletionHandler.h>
#include <wtf/RunLoop.h>

OBJC_CLASS NSDictionary;
OBJC_CLASS PDFDocument;
OBJC_CLASS PDFSelection;

namespace WebCore {
class HTMLPlugInElement;
class LocalFrame;
class PlatformMouseEvent;
class RenderEmbeddedObject;
class ShareableBitmap;
class VoidCallback;
enum class TextGranularity : uint8_t;
}

namespace WebKit {

class PDFPluginBase;
class WebFrame;
class WebPage;
enum class SelectionEndpoint : bool;
enum class SelectionWasFlipped : bool;
struct DocumentEditingContextRequest;
struct DocumentEditingContext;
struct EditorState;
struct FrameInfoData;
struct WebHitTestResultData;

class PluginView final : public WebCore::PluginViewBase {
public:
    static RefPtr<PluginView> create(WebCore::HTMLPlugInElement&, const URL&, const String& contentType, bool shouldUseManualLoader);

    WebCore::LocalFrame* frame() const;
    RefPtr<WebCore::LocalFrame> protectedFrame() const;

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

    void layerHostingStrategyDidChange() final;

    WebCore::HTMLPlugInElement& pluginElement() const { return m_pluginElement; }
    const URL& mainResourceURL() const { return m_mainResourceURL; }

    void didBeginMagnificationGesture();
    void didEndMagnificationGesture();
    void setPageScaleFactor(double, std::optional<WebCore::IntPoint> origin);
    void mainFramePageScaleFactorDidChange();
    double pageScaleFactor() const;
    void pluginScaleFactorDidChange();
#if PLATFORM(IOS_FAMILY)
    std::pair<URL, WebCore::FloatRect> linkURLAndBoundsAtPoint(WebCore::FloatPoint pointInRootView) const;
    std::tuple<URL, WebCore::FloatRect, RefPtr<WebCore::TextIndicator>> linkDataAtPoint(WebCore::FloatPoint pointInRootView);
    std::optional<WebCore::FloatRect> highlightRectForTapAtPoint(WebCore::FloatPoint pointInRootView) const;
    void handleSyntheticClick(WebCore::PlatformMouseEvent&&);
    void setSelectionRange(WebCore::FloatPoint pointInRootView, WebCore::TextGranularity);
    void clearSelection();
    SelectionWasFlipped moveSelectionEndpoint(WebCore::FloatPoint pointInRootView, SelectionEndpoint);
    SelectionEndpoint extendInitialSelection(WebCore::FloatPoint pointInRootView, WebCore::TextGranularity);
    CursorContext cursorContext(WebCore::FloatPoint pointInRootView) const;
    DocumentEditingContext documentEditingContext(DocumentEditingContextRequest&&) const;
#endif

    bool populateEditorStateIfNeeded(EditorState&) const;

    void obscuredContentInsetsDidChange();

    void webPageDestroyed();

    bool handleEditingCommand(const String& commandName, const String& argument);
    bool isEditingCommandEnabled(const String& commandName);
    
    unsigned countFindMatches(const String& target, WebCore::FindOptions, unsigned maxMatchCount);
    bool findString(const String& target, WebCore::FindOptions, unsigned maxMatchCount);
    Vector<WebCore::FloatRect> rectsForTextMatchesInRect(const WebCore::IntRect&) const;
    bool drawsFindOverlay() const;
    RefPtr<WebCore::TextIndicator> textIndicatorForCurrentSelection(OptionSet<WebCore::TextIndicatorOption>, WebCore::TextIndicatorPresentationTransition);

    Vector<WebFoundTextRange::PDFData> findTextMatches(const String& target, WebCore::FindOptions);
    Vector<WebCore::FloatRect> rectsForTextMatchesInRect(const Vector<WebFoundTextRange::PDFData>&, const WebCore::IntRect& clipRect);
    RefPtr<WebCore::TextIndicator> textIndicatorForTextMatch(const WebFoundTextRange::PDFData&, WebCore::TextIndicatorPresentationTransition);
    void scrollToRevealTextMatch(const WebFoundTextRange::PDFData&);

    String fullDocumentString() const;
    String selectionString() const;
    std::pair<String, String> stringsBeforeAndAfterSelection(int characterCount) const;

    RefPtr<WebCore::FragmentedSharedBuffer> liveResourceData() const;

    bool performDictionaryLookupAtLocation(const WebCore::FloatPoint&);
    bool performImmediateActionHitTestAtLocation(const WebCore::FloatPoint&, WebHitTestResultData&) const;

    WebCore::FloatRect rectForSelectionInRootView(PDFSelection *) const;
    
    bool isUsingUISideCompositing() const;

    void invalidateRect(const WebCore::IntRect&) final;

    void didChangeSettings();

    void windowActivityDidChange();

    void didChangeIsInWindow();

    void didSameDocumentNavigationForFrame(WebFrame&);

    PDFPluginIdentifier pdfPluginIdentifier() const;

    void openWithPreview(CompletionHandler<void(const String&, std::optional<FrameInfoData>&&, std::span<const uint8_t>)>&&);

    void focusPluginElement();

    bool pluginHandlesPageScaleFactor() const;

    WebCore::FloatRect absoluteBoundingRectForSmartMagnificationAtPoint(WebCore::FloatPoint) const;

    void frameViewLayoutOrVisualViewportChanged(const WebCore::IntRect& unobscuredContentRect);

    bool pluginDelegatesScrollingToMainFrame() const;

    bool isPresentingLockedContent() const;

    void effectiveAppearanceDidChange();

private:
    PluginView(WebCore::HTMLPlugInElement&, const URL&, const String& contentType, bool shouldUseManualLoader, WebPage&);
    virtual ~PluginView();

    bool isPluginView() const final { return true; }

    void initializePlugin();

    void viewGeometryDidChange();
    void viewVisibilityDidChange();

    WebCore::IntRect clipRectInWindowCoordinates() const;
    
    void pendingResourceRequestTimerFired();

    void loadMainResource();
    void redeliverManualStream();

    bool shouldCreateTransientPaintingSnapshot() const;

    void updateDocumentForPluginSizingBehavior();

    WebCore::RenderEmbeddedObject* renderer() const;

    // WebCore::PluginViewBase
    WebCore::PluginLayerHostingStrategy layerHostingStrategy() const final;

    PlatformLayer* platformLayer() const final;
    WebCore::GraphicsLayer* graphicsLayer() const final;

    bool scroll(WebCore::ScrollDirection, WebCore::ScrollGranularity) final;
    WebCore::ScrollPosition scrollPositionForTesting() const final;
    WebCore::Scrollbar* horizontalScrollbar() final;
    WebCore::Scrollbar* verticalScrollbar() final;
    bool wantsWheelEvents() final;
    bool shouldAllowNavigationFromDrags() const final;
    void willDetachRenderer() final;

    WebCore::ScrollableArea* scrollableArea() const final;
    bool usesAsyncScrolling() const final;
    std::optional<WebCore::ScrollingNodeID> scrollingNodeID() const final;
    void willAttachScrollingNode() final;
    void didAttachScrollingNode() final;

    // WebCore::Widget
    void setFrameRect(const WebCore::IntRect&) final;
    void paint(WebCore::GraphicsContext&, const WebCore::IntRect&, WebCore::Widget::SecurityOriginPaintPolicy, WebCore::RegionContext*) final;
    void frameRectsChanged() final;
    void setParent(WebCore::ScrollView*) final;
    void handleEvent(WebCore::Event&) final;
    void notifyWidget(WebCore::WidgetNotification) final;
    void show() final;
    void hide() final;
    void setParentVisible(bool) final;
    bool transformsAffectFrameRect() final;
    void clipRectChanged() final;

    void releaseMemory() final;

    RefPtr<WebPage> protectedWebPage() const;

    const Ref<WebCore::HTMLPlugInElement> m_pluginElement;
    const Ref<PDFPluginBase> m_plugin;
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
    RefPtr<WebCore::ShareableBitmap> m_transientPaintingSnapshot;

    bool sendEditingCommandToPDFForTesting(const String& commandName, const String& argument) final;
    void setPDFDisplayModeForTesting(const String&) final;
    Vector<WebCore::FloatRect> pdfAnnotationRectsForTesting() const override;
    void unlockPDFDocumentForTesting(const String& password) final;
    void setPDFTextAnnotationValueForTesting(unsigned pageIndex, unsigned annotationIndex, const String& value) final;
    void registerPDFTestCallback(RefPtr<WebCore::VoidCallback>&&) final;
};

} // namespace WebKit

SPECIALIZE_TYPE_TRAITS_BEGIN(WebKit::PluginView) \
    static bool isType(const WebCore::Widget& widget) { return widget.isPluginView(); } \
SPECIALIZE_TYPE_TRAITS_END()

#endif
