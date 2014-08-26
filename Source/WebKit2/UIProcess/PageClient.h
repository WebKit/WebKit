/*
 * Copyright (C) 2010, 2011 Apple Inc. All rights reserved.
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

#ifndef PageClient_h
#define PageClient_h

#include "ShareableBitmap.h"
#include "WebColorPicker.h"
#include "WebPageProxy.h"
#include "WebPopupMenuProxy.h"
#include <WebCore/AlternativeTextClient.h>
#include <WebCore/EditorClient.h>
#include <wtf/Forward.h>

#if PLATFORM(COCOA)
#include "PluginComplexTextInputState.h"

OBJC_CLASS CALayer;

#if USE(APPKIT)
OBJC_CLASS WKView;
OBJC_CLASS NSTextAlternatives;
#endif
#endif

namespace WebCore {
class Cursor;
struct ViewportAttributes;
struct Highlight;
}

namespace WebKit {

class DrawingAreaProxy;
class FindIndicator;
class NativeWebKeyboardEvent;
class RemoteLayerTreeTransaction;
class ViewSnapshot;
class WebContextMenuProxy;
class WebEditCommandProxy;
class WebPopupMenuProxy;

#if ENABLE(TOUCH_EVENTS)
class NativeWebTouchEvent;
#endif

#if ENABLE(INPUT_TYPE_COLOR)
class WebColorPicker;
#endif

#if ENABLE(FULLSCREEN_API)
class WebFullScreenManagerProxyClient;
#endif

#if PLATFORM(COCOA)
struct ColorSpaceData;
#endif

class PageClient {
public:
    virtual ~PageClient() { }

    // Create a new drawing area proxy for the given page.
    virtual std::unique_ptr<DrawingAreaProxy> createDrawingAreaProxy() = 0;

    // Tell the view to invalidate the given rect. The rect is in view coordinates.
    virtual void setViewNeedsDisplay(const WebCore::IntRect&) = 0;

    // Tell the view to immediately display its invalid rect.
    virtual void displayView() = 0;

    // Return true if scrollView() can copy bits in the view.
    virtual bool canScrollView() = 0;
    // Tell the view to scroll scrollRect by scrollOffset.
    virtual void scrollView(const WebCore::IntRect& scrollRect, const WebCore::IntSize& scrollOffset) = 0;
    // Tell the view to scroll to the given position, and whether this was a programmatic scroll.
    virtual void requestScroll(const WebCore::FloatPoint& scrollPosition, bool isProgrammaticScroll) = 0;

    // Return the size of the view the page is associated with.
    virtual WebCore::IntSize viewSize() = 0;

    // Return whether the view's containing window is active.
    virtual bool isViewWindowActive() = 0;

    // Return whether the view is focused.
    virtual bool isViewFocused() = 0;

    // Return whether the view is visible.
    virtual bool isViewVisible() = 0;

    // Return whether the view is visible, or occluded by another window.
    virtual bool isViewVisibleOrOccluded() { return isViewVisible(); }

    // Return whether the view is in a window.
    virtual bool isViewInWindow() = 0;

    // Return whether the view is visually idle.
    virtual bool isVisuallyIdle() { return !isViewVisible(); }

    // Return the layer hosting mode for the view.
    virtual LayerHostingMode viewLayerHostingMode() { return LayerHostingMode::InProcess; }

    virtual void processDidExit() = 0;
    virtual void didRelaunchProcess() = 0;
    virtual void pageClosed() = 0;

    virtual void preferencesDidChange() = 0;

    virtual void toolTipChanged(const String&, const String&) = 0;

    virtual bool decidePolicyForGeolocationPermissionRequest(WebFrameProxy&, WebSecurityOrigin&, GeolocationPermissionRequestProxy&)
    {
        return false;
    }

    virtual void didCommitLoadForMainFrame(const String& mimeType, bool useCustomContentProvider) = 0;

#if USE(TILED_BACKING_STORE)
    virtual void pageDidRequestScroll(const WebCore::IntPoint&) = 0;
    virtual void didRenderFrame(const WebCore::IntSize& contentsSize, const WebCore::IntRect& coveredRect) = 0;
    virtual void pageTransitionViewportReady() = 0;
#endif
#if USE(COORDINATED_GRAPHICS)
    virtual void didFindZoomableArea(const WebCore::IntPoint&, const WebCore::IntRect&) = 0;
#endif

#if PLATFORM(EFL) || PLATFORM(GTK)
    virtual void updateTextInputState() = 0;
#endif // PLATFORM(EFL) || PLATOFRM(GTK)

    virtual void handleDownloadRequest(DownloadProxy*) = 0;

    virtual bool handleRunOpenPanel(WebPageProxy*, WebFrameProxy*, WebOpenPanelParameters*, WebOpenPanelResultListenerProxy*) { return false; }

#if PLATFORM(EFL)
    virtual void didChangeContentSize(const WebCore::IntSize&) = 0;
#endif

#if PLATFORM(GTK)
    virtual void startDrag(const WebCore::DragData&, PassRefPtr<ShareableBitmap> dragImage) = 0;
#endif

    virtual void setCursor(const WebCore::Cursor&) = 0;
    virtual void setCursorHiddenUntilMouseMoves(bool) = 0;
    virtual void didChangeViewportProperties(const WebCore::ViewportAttributes&) = 0;

    virtual void registerEditCommand(PassRefPtr<WebEditCommandProxy>, WebPageProxy::UndoOrRedo) = 0;
    virtual void clearAllEditCommands() = 0;
    virtual bool canUndoRedo(WebPageProxy::UndoOrRedo) = 0;
    virtual void executeUndoRedo(WebPageProxy::UndoOrRedo) = 0;
#if PLATFORM(COCOA)
    virtual void accessibilityWebProcessTokenReceived(const IPC::DataReference&) = 0;
    virtual bool executeSavedCommandBySelector(const String& selector) = 0;
    virtual void setDragImage(const WebCore::IntPoint& clientPosition, PassRefPtr<ShareableBitmap> dragImage, bool isLinkDrag) = 0;
    virtual void updateSecureInputState() = 0;
    virtual void resetSecureInputState() = 0;
    virtual void notifyInputContextAboutDiscardedComposition() = 0;
    virtual void makeFirstResponder() = 0;
    virtual void setAcceleratedCompositingRootLayer(LayerOrView *) = 0;
    virtual LayerOrView *acceleratedCompositingRootLayer() const = 0;
    virtual PassRefPtr<ViewSnapshot> takeViewSnapshot() = 0;
    virtual void wheelEventWasNotHandledByWebCore(const NativeWebWheelEvent&) = 0;
#endif

#if USE(APPKIT)
    virtual void setPromisedData(const String& pasteboardName, PassRefPtr<WebCore::SharedBuffer> imageBuffer, const String& filename, const String& extension, const String& title,
                                 const String& url, const String& visibleUrl, PassRefPtr<WebCore::SharedBuffer> archiveBuffer) = 0;
#endif

#if PLATFORM(GTK)
    virtual void getEditorCommandsForKeyEvent(const NativeWebKeyboardEvent&, const AtomicString&, Vector<WTF::String>&) = 0;
#endif
    virtual WebCore::FloatRect convertToDeviceSpace(const WebCore::FloatRect&) = 0;
    virtual WebCore::FloatRect convertToUserSpace(const WebCore::FloatRect&) = 0;
    virtual WebCore::IntPoint screenToRootView(const WebCore::IntPoint&) = 0;
    virtual WebCore::IntRect rootViewToScreen(const WebCore::IntRect&) = 0;
#if PLATFORM(IOS)
    virtual WebCore::IntPoint accessibilityScreenToRootView(const WebCore::IntPoint&) = 0;
    virtual WebCore::IntRect rootViewToAccessibilityScreen(const WebCore::IntRect&) = 0;
#endif
    
    virtual void doneWithKeyEvent(const NativeWebKeyboardEvent&, bool wasEventHandled) = 0;
#if ENABLE(TOUCH_EVENTS)
    virtual void doneWithTouchEvent(const NativeWebTouchEvent&, bool wasEventHandled) = 0;
#endif

    virtual PassRefPtr<WebPopupMenuProxy> createPopupMenuProxy(WebPageProxy*) = 0;
    virtual PassRefPtr<WebContextMenuProxy> createContextMenuProxy(WebPageProxy*) = 0;

#if ENABLE(INPUT_TYPE_COLOR)
    virtual PassRefPtr<WebColorPicker> createColorPicker(WebPageProxy*, const WebCore::Color& initialColor, const WebCore::IntRect&) = 0;
#endif

    virtual void setFindIndicator(PassRefPtr<FindIndicator>, bool fadeOut, bool animate) = 0;

    virtual void enterAcceleratedCompositingMode(const LayerTreeContext&) = 0;
    virtual void exitAcceleratedCompositingMode() = 0;
    virtual void updateAcceleratedCompositingMode(const LayerTreeContext&) = 0;

#if PLATFORM(MAC)
    virtual void pluginFocusOrWindowFocusChanged(uint64_t pluginComplexTextInputIdentifier, bool pluginHasFocusAndWindowHasFocus) = 0;
    virtual void setPluginComplexTextInputState(uint64_t pluginComplexTextInputIdentifier, PluginComplexTextInputState) = 0;
    virtual void didPerformDictionaryLookup(const AttributedString&, const DictionaryPopupInfo&) = 0;
    virtual void dismissDictionaryLookupPanel() = 0;
    virtual void showCorrectionPanel(WebCore::AlternativeTextType, const WebCore::FloatRect& boundingBoxOfReplacedString, const String& replacedString, const String& replacementString, const Vector<String>& alternativeReplacementStrings) = 0;
    virtual void dismissCorrectionPanel(WebCore::ReasonForDismissingAlternativeText) = 0;
    virtual String dismissCorrectionPanelSoon(WebCore::ReasonForDismissingAlternativeText) = 0;
    virtual void recordAutocorrectionResponse(WebCore::AutocorrectionResponseType, const String& replacedString, const String& replacementString) = 0;
    virtual void recommendedScrollbarStyleDidChange(int32_t newStyle) = 0;
    virtual void removeNavigationGestureSnapshot() = 0;

    virtual ColorSpaceData colorSpace() = 0;

#if USE(APPKIT)
    virtual WKView* wkView() const = 0;
    virtual void intrinsicContentSizeDidChange(const WebCore::IntSize& intrinsicContentSize) = 0;
#if USE(DICTATION_ALTERNATIVES)
    virtual uint64_t addDictationAlternatives(const RetainPtr<NSTextAlternatives>&) = 0;
    virtual void removeDictationAlternatives(uint64_t dictationContext) = 0;
    virtual void showDictationAlternativeUI(const WebCore::FloatRect& boundingBoxOfDictatedText, uint64_t dictationContext) = 0;
    virtual Vector<String> dictationAlternatives(uint64_t dictationContext) = 0;
#endif // USE(DICTATION_ALTERNATIVES)
#if USE(INSERTION_UNDO_GROUPING)
    virtual void registerInsertionUndoGrouping() = 0;
#endif // USE(INSERTION_UNDO_GROUPING)
#endif // USE(APPKIT)
#endif // PLATFORM(MAC)

#if PLATFORM(IOS)
    virtual void commitPotentialTapFailed() = 0;
    virtual void didGetTapHighlightGeometries(uint64_t requestID, const WebCore::Color&, const Vector<WebCore::FloatQuad>& highlightedQuads, const WebCore::IntSize& topLeftRadius, const WebCore::IntSize& topRightRadius, const WebCore::IntSize& bottomLeftRadius, const WebCore::IntSize& bottomRightRadius) = 0;

    virtual void didCommitLayerTree(const RemoteLayerTreeTransaction&) = 0;
    virtual void dynamicViewportUpdateChangedTarget(double newScale, const WebCore::FloatPoint& newScrollPosition, uint64_t transactionID) = 0;
    virtual void restorePageState(const WebCore::FloatRect&, double) = 0;
    virtual void restorePageCenterAndScale(const WebCore::FloatPoint&, double) = 0;

    virtual void startAssistingNode(const AssistedNodeInformation&, bool userIsInteracting, bool blurPreviousNode, API::Object* userData) = 0;
    virtual void stopAssistingNode() = 0;
    virtual bool isAssistingNode() = 0;
    virtual void selectionDidChange() = 0;
    virtual bool interpretKeyEvent(const NativeWebKeyboardEvent&, bool isCharEvent) = 0;
    virtual void positionInformationDidChange(const InteractionInformationAtPosition&) = 0;
    virtual void saveImageToLibrary(PassRefPtr<WebCore::SharedBuffer>) = 0;
    virtual void didUpdateBlockSelectionWithTouch(uint32_t touch, uint32_t flags, float growThreshold, float shrinkThreshold) = 0;
    virtual void showPlaybackTargetPicker(bool hasVideo, const WebCore::IntRect& elementRect) = 0;
    virtual void zoomToRect(WebCore::FloatRect, double minimumScale, double maximumScale) = 0;
    virtual void didChangeViewportMetaTagWidth(float) = 0;
    virtual void setUsesMinimalUI(bool) = 0;
    virtual double minimumZoomScale() const = 0;
    virtual WebCore::FloatSize contentsSize() const = 0;
    virtual void overflowScrollViewWillStartPanGesture() = 0;
    virtual void overflowScrollViewDidScroll() = 0;
    virtual void overflowScrollWillStartScroll() = 0;
    virtual void overflowScrollDidEndScroll() = 0;
    virtual void didFinishDrawingPagesToPDF(const IPC::DataReference&) = 0;
    virtual Vector<String> mimeTypesWithCustomContentProviders() = 0;

#if ENABLE(INSPECTOR)
    virtual void showInspectorHighlight(const WebCore::Highlight&) = 0;
    virtual void hideInspectorHighlight() = 0;

    virtual void showInspectorIndication() = 0;
    virtual void hideInspectorIndication() = 0;

    virtual void enableInspectorNodeSearch() = 0;
    virtual void disableInspectorNodeSearch() = 0;
#endif
#endif

    // Auxiliary Client Creation
#if ENABLE(FULLSCREEN_API)
    virtual WebFullScreenManagerProxyClient& fullScreenManagerProxyClient() = 0;
#endif

    // Custom representations.
    virtual void didFinishLoadingDataForCustomContentProvider(const String& suggestedFilename, const IPC::DataReference&) = 0;

    virtual void navigationGestureDidBegin() = 0;
    virtual void navigationGestureWillEnd(bool willNavigate, WebBackForwardListItem&) = 0;
    virtual void navigationGestureDidEnd(bool willNavigate, WebBackForwardListItem&) = 0;
    virtual void willRecordNavigationSnapshot(WebBackForwardListItem&) = 0;

    virtual void didFirstVisuallyNonEmptyLayoutForMainFrame() = 0;
    virtual void didFinishLoadForMainFrame() = 0;
    virtual void didSameDocumentNavigationForMainFrame(SameDocumentNavigationType) = 0;
};

} // namespace WebKit

#endif // PageClient_h
