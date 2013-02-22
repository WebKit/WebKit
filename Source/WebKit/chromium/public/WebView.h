/*
 * Copyright (C) 2009, 2010, 2011, 2012 Google Inc. All rights reserved.
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

#ifndef WebView_h
#define WebView_h

#include "../../../Platform/chromium/public/WebString.h"
#include "../../../Platform/chromium/public/WebVector.h"
#include "WebDragOperation.h"
#include "WebPageVisibilityState.h"
#include "WebWidget.h"

namespace WebKit {

class WebAccessibilityObject;
class WebAutofillClient;
class WebBatteryStatus;
class WebDevToolsAgent;
class WebDevToolsAgentClient;
class WebDragData;
class WebFrame;
class WebFrameClient;
class WebGraphicsContext3D;
class WebHitTestResult;
class WebNode;
class WebPageOverlay;
class WebPermissionClient;
class WebPrerendererClient;
class WebRange;
class WebSettings;
class WebSpellCheckClient;
class WebString;
class WebTextFieldDecoratorClient;
class WebViewBenchmarkSupport;
class WebViewClient;
struct WebActiveWheelFlingParameters;
struct WebMediaPlayerAction;
struct WebPluginAction;
struct WebPoint;

class WebView : public WebWidget {
public:
    WEBKIT_EXPORT static const double textSizeMultiplierRatio;
    WEBKIT_EXPORT static const double minTextSizeMultiplier;
    WEBKIT_EXPORT static const double maxTextSizeMultiplier;
    WEBKIT_EXPORT static const float minPageScaleFactor;
    WEBKIT_EXPORT static const float maxPageScaleFactor;

    // Controls the time that user scripts injected into the document run.
    enum UserScriptInjectAt {
        UserScriptInjectAtDocumentStart,
        UserScriptInjectAtDocumentEnd
    };

    // Controls which frames user content is injected into.
    enum UserContentInjectIn {
        UserContentInjectInAllFrames,
        UserContentInjectInTopFrameOnly
    };

    // Controls which documents user styles are injected into.
    enum UserStyleInjectionTime {
        UserStyleInjectInExistingDocuments,
        UserStyleInjectInSubsequentDocuments
    };


    // Initialization ------------------------------------------------------

    // Creates a WebView that is NOT yet initialized.  You will need to
    // call initializeMainFrame to finish the initialization.  It is valid
    // to pass null client pointers.
    WEBKIT_EXPORT static WebView* create(WebViewClient*);

    // After creating a WebView, you should immediately call this method.
    // You can optionally modify the settings before calling this method.
    // The WebFrameClient will receive events for the main frame and any
    // child frames.  It is valid to pass a null WebFrameClient pointer.
    virtual void initializeMainFrame(WebFrameClient*) = 0;

    virtual void initializeHelperPluginFrame(WebFrameClient*) = 0;

    // Initializes the various client interfaces.
    virtual void setAutofillClient(WebAutofillClient*) = 0;
    virtual void setDevToolsAgentClient(WebDevToolsAgentClient*) = 0;
    virtual void setPermissionClient(WebPermissionClient*) = 0;
    virtual void setPrerendererClient(WebPrerendererClient*) = 0;
    virtual void setSpellCheckClient(WebSpellCheckClient*) = 0;
    virtual void addTextFieldDecoratorClient(WebTextFieldDecoratorClient*) = 0;


    // Options -------------------------------------------------------------

    // The returned pointer is valid for the lifetime of the WebView.
    virtual WebSettings* settings() = 0;

    // Corresponds to the encoding of the main frame.  Setting the page
    // encoding may cause the main frame to reload.
    virtual WebString pageEncoding() const = 0;
    virtual void setPageEncoding(const WebString&) = 0;

    // Makes the WebView transparent.  This is useful if you want to have
    // some custom background rendered behind it.
    virtual bool isTransparent() const = 0;
    virtual void setIsTransparent(bool) = 0;

    // Controls whether pressing Tab key advances focus to links.
    virtual bool tabsToLinks() const = 0;
    virtual void setTabsToLinks(bool) = 0;

    // Method that controls whether pressing Tab key cycles through page
    // elements or inserts a '\t' char in the focused text area.
    virtual bool tabKeyCyclesThroughElements() const = 0;
    virtual void setTabKeyCyclesThroughElements(bool) = 0;

    // Controls the WebView's active state, which may affect the rendering
    // of elements on the page (i.e., tinting of input elements).
    virtual bool isActive() const = 0;
    virtual void setIsActive(bool) = 0;

    // Allows disabling domain relaxation.
    virtual void setDomainRelaxationForbidden(bool, const WebString& scheme) = 0;


    // Closing -------------------------------------------------------------

    // Runs beforeunload handlers for the current page, returning false if
    // any handler suppressed unloading.
    virtual bool dispatchBeforeUnloadEvent() = 0;

    // Runs unload handlers for the current page.
    virtual void dispatchUnloadEvent() = 0;


    // Frames --------------------------------------------------------------

    virtual WebFrame* mainFrame() = 0;

    // Returns the frame identified by the given name.  This method
    // supports pseudo-names like _self, _top, and _blank.  It traverses
    // the entire frame tree containing this tree looking for a frame that
    // matches the given name.  If the optional relativeToFrame parameter
    // is specified, then the search begins with the given frame and its
    // children.
    virtual WebFrame* findFrameByName(
        const WebString& name, WebFrame* relativeToFrame = 0) = 0;


    // Focus ---------------------------------------------------------------

    virtual WebFrame* focusedFrame() = 0;
    virtual void setFocusedFrame(WebFrame*) = 0;

    // Focus the first (last if reverse is true) focusable node.
    virtual void setInitialFocus(bool reverse) = 0;

    // Clears the focused node (and selection if a text field is focused)
    // to ensure that a text field on the page is not eating keystrokes we
    // send it.
    virtual void clearFocusedNode() = 0;

    // Scrolls the node currently in focus into view.
    virtual void scrollFocusedNodeIntoView() = 0;

    // Scrolls the node currently in focus into |rect|, where |rect| is in
    // window space.
    virtual void scrollFocusedNodeIntoRect(const WebRect&) { }

    // Advance the focus of the WebView forward to the next element or to the
    // previous element in the tab sequence (if reverse is true).
    virtual void advanceFocus(bool reverse) { }

    // Animate a scale into the specified find-in-page rect.
    virtual void zoomToFindInPageRect(const WebRect&) = 0;


    // Zoom ----------------------------------------------------------------

    // Returns the current zoom level.  0 is "original size", and each increment
    // above or below represents zooming 20% larger or smaller to default limits
    // of 300% and 50% of original size, respectively.  Only plugins use
    // non whole-numbers, since they might choose to have specific zoom level so
    // that fixed-width content is fit-to-page-width, for example.
    virtual double zoomLevel() = 0;

    // Changes the zoom level to the specified level, clamping at the limits
    // noted above, and returns the current zoom level after applying the
    // change.
    //
    // If |textOnly| is set, only the text will be zoomed; otherwise the entire
    // page will be zoomed. You can only have either text zoom or full page zoom
    // at one time.  Changing the mode while the page is zoomed will have odd
    // effects.
    virtual double setZoomLevel(bool textOnly, double zoomLevel) = 0;

    // Updates the zoom limits for this view.
    virtual void zoomLimitsChanged(double minimumZoomLevel,
                                   double maximumZoomLevel) = 0;

    // Helper functions to convert between zoom level and zoom factor.  zoom
    // factor is zoom percent / 100, so 300% = 3.0.
    WEBKIT_EXPORT static double zoomLevelToZoomFactor(double zoomLevel);
    WEBKIT_EXPORT static double zoomFactorToZoomLevel(double factor);

    // Sets the initial page scale to the given factor. This scale setting overrides
    // page scale set in the page's viewport meta tag.
    virtual void setInitialPageScaleOverride(float) = 0;

    // Gets the scale factor of the page, where 1.0 is the normal size, > 1.0
    // is scaled up, < 1.0 is scaled down.
    virtual float pageScaleFactor() const = 0;

    // Indicates whether the page scale factor has been set since navigating
    // to a new page.
    virtual bool isPageScaleFactorSet() const = 0;

    // Scales the page and the scroll offset by a given factor, while ensuring
    // that the new scroll position does not go beyond the edge of the page.
    virtual void setPageScaleFactorPreservingScrollOffset(float) = 0;

    // Scales a page by a factor of scaleFactor and then sets a scroll position to (x, y).
    // setPageScaleFactor() magnifies and shrinks a page without affecting layout.
    // On the other hand, zooming affects layout of the page.
    virtual void setPageScaleFactor(float scaleFactor, const WebPoint& origin) = 0;

    // PageScaleFactor will be force-clamped between minPageScale and maxPageScale
    // (and these values will persist until setPageScaleFactorLimits is called
    // again).
    virtual void setPageScaleFactorLimits(float minPageScale, float maxPageScale) = 0;

    virtual float minimumPageScaleFactor() const = 0;
    virtual float maximumPageScaleFactor() const = 0;

    // Save the WebView's current scroll and scale state. Each call to this function
    // overwrites the previously saved scroll and scale state.
    virtual void saveScrollAndScaleState() = 0;

    // Restore the previously saved scroll and scale state. After restoring the
    // state, this function deletes any saved scroll and scale state.
    virtual void restoreScrollAndScaleState() = 0;

    // Reset any saved values for the scroll and scale state.
    virtual void resetScrollAndScaleState() = 0;

    // Prevent the web page from setting a maximum scale via the viewport meta
    // tag. This is an accessibility feature that lets folks zoom in to web
    // pages even if the web page tries to block scaling.
    virtual void setIgnoreViewportTagMaximumScale(bool) = 0;

    // The ratio of the current device's screen DPI to the target device's screen DPI.
    virtual float deviceScaleFactor() const = 0;

    // Sets the ratio as computed by computeViewportAttributes.
    virtual void setDeviceScaleFactor(float) = 0;


    // Fixed Layout --------------------------------------------------------

    // In fixed layout mode, the layout of the page is independent of the
    // view port size, given by WebWidget::size().

    virtual bool isFixedLayoutModeEnabled() const = 0;
    virtual void enableFixedLayoutMode(bool enable) = 0;

    virtual WebSize fixedLayoutSize() const = 0;
    virtual void setFixedLayoutSize(const WebSize&) = 0;


    // Auto-Resize -----------------------------------------------------------

    // In auto-resize mode, the view is automatically adjusted to fit the html
    // content within the given bounds.
    virtual void enableAutoResizeMode(
        const WebSize& minSize,
        const WebSize& maxSize) = 0;

    // Turn off auto-resize.
    virtual void disableAutoResizeMode() = 0;

    // Media ---------------------------------------------------------------

    // Performs the specified media player action on the node at the given location.
    virtual void performMediaPlayerAction(
        const WebMediaPlayerAction&, const WebPoint& location) = 0;

    // Performs the specified plugin action on the node at the given location.
    virtual void performPluginAction(
        const WebPluginAction&, const WebPoint& location) = 0;


    // Data exchange -------------------------------------------------------

    // Do a hit test at given point and return the HitTestResult.
    virtual WebHitTestResult hitTestResultAt(const WebPoint&) = 0;

    // Copy to the clipboard the image located at a particular point in the
    // WebView (if there is such an image)
    virtual void copyImageAt(const WebPoint&) = 0;

    // Notifies the WebView that a drag has terminated.
    virtual void dragSourceEndedAt(
        const WebPoint& clientPoint, const WebPoint& screenPoint,
        WebDragOperation operation) = 0;

    // Notifies the WebView that a drag is going on.
    virtual void dragSourceMovedTo(
        const WebPoint& clientPoint, const WebPoint& screenPoint,
        WebDragOperation operation) = 0;

    // Notfies the WebView that the system drag and drop operation has ended.
    virtual void dragSourceSystemDragEnded() = 0;

    // Callback methods when a drag-and-drop operation is trying to drop
    // something on the WebView.
    virtual WebDragOperation dragTargetDragEnter(
        const WebDragData&,
        const WebPoint& clientPoint, const WebPoint& screenPoint,
        WebDragOperationsMask operationsAllowed,
        int keyModifiers) = 0;
    virtual WebDragOperation dragTargetDragOver(
        const WebPoint& clientPoint, const WebPoint& screenPoint,
        WebDragOperationsMask operationsAllowed,
        int keyModifiers) = 0;
    virtual void dragTargetDragLeave() = 0;
    virtual void dragTargetDrop(
        const WebPoint& clientPoint, const WebPoint& screenPoint,
        int keyModifiers) = 0;


    // Support for resource loading initiated by plugins -------------------

    // Returns next unused request identifier which is unique within the
    // parent Page.
    virtual unsigned long createUniqueIdentifierForRequest() = 0;


    // Developer tools -----------------------------------------------------

    // Inspect a particular point in the WebView.  (x = -1 || y = -1) is a
    // special case, meaning inspect the current page and not a specific
    // point.
    virtual void inspectElementAt(const WebPoint&) = 0;

    // Settings used by the inspector.
    virtual WebString inspectorSettings() const = 0;
    virtual void setInspectorSettings(const WebString&) = 0;
    virtual bool inspectorSetting(const WebString& key,
                                  WebString* value) const = 0;
    virtual void setInspectorSetting(const WebString& key,
                                     const WebString& value) = 0;

    // The embedder may optionally engage a WebDevToolsAgent.  This may only
    // be set once per WebView.
    virtual WebDevToolsAgent* devToolsAgent() = 0;


    // Accessibility -------------------------------------------------------

    // Returns the accessibility object for this view.
    virtual WebAccessibilityObject accessibilityObject() = 0;


    // Autofill  -----------------------------------------------------------

    // Notifies the WebView that Autofill suggestions are available for a node.
    // |itemIDs| is a vector of IDs for the menu items. A positive itemID is a
    // unique ID for the Autofill entries. Other MenuItemIDs are defined in
    // WebAutofillClient.h
    virtual void applyAutofillSuggestions(
        const WebNode&,
        const WebVector<WebString>& names,
        const WebVector<WebString>& labels,
        const WebVector<WebString>& icons,
        const WebVector<int>& itemIDs,
        int separatorIndex = -1) = 0;

    // Hides any popup (suggestions, selects...) that might be showing.
    virtual void hidePopups() = 0;

    virtual void selectAutofillSuggestionAtIndex(unsigned listIndex) = 0;


    // Context menu --------------------------------------------------------

    virtual void performCustomContextMenuAction(unsigned action) = 0;


    // Popup menu ----------------------------------------------------------

    // Sets whether select popup menus should be rendered by the browser.
    WEBKIT_EXPORT static void setUseExternalPopupMenus(bool);


    // Visited link state --------------------------------------------------

    // Tells all WebView instances to update the visited link state for the
    // specified hash.
    WEBKIT_EXPORT static void updateVisitedLinkState(unsigned long long hash);

    // Tells all WebView instances to update the visited state for all
    // their links.
    WEBKIT_EXPORT static void resetVisitedLinkState();


    // Custom colors -------------------------------------------------------

    virtual void setScrollbarColors(unsigned inactiveColor,
                                    unsigned activeColor,
                                    unsigned trackColor) = 0;

    virtual void setSelectionColors(unsigned activeBackgroundColor,
                                    unsigned activeForegroundColor,
                                    unsigned inactiveBackgroundColor,
                                    unsigned inactiveForegroundColor) = 0;

    // User scripts --------------------------------------------------------
    WEBKIT_EXPORT static void addUserScript(const WebString& sourceCode,
                                            const WebVector<WebString>& patterns,
                                            UserScriptInjectAt injectAt,
                                            UserContentInjectIn injectIn);
    WEBKIT_EXPORT static void addUserStyleSheet(const WebString& sourceCode,
                                                const WebVector<WebString>& patterns,
                                                UserContentInjectIn injectIn,
                                                UserStyleInjectionTime injectionTime = UserStyleInjectInSubsequentDocuments);
    WEBKIT_EXPORT static void removeAllUserContent();

    // Modal dialog support ------------------------------------------------

    // Call these methods before and after running a nested, modal event loop
    // to suspend script callbacks and resource loads.
    WEBKIT_EXPORT static void willEnterModalLoop();
    WEBKIT_EXPORT static void didExitModalLoop();

    // GPU acceleration support --------------------------------------------

    // Context that's in the compositor's share group, but is not the compositor context itself.
    // Can be used for allocating resources that the compositor will later access.
    virtual WebGraphicsContext3D* sharedGraphicsContext3D() = 0;

    // Called to inform the WebView that a wheel fling animation was started externally (for instance
    // by the compositor) but must be completed by the WebView.
    virtual void transferActiveWheelFlingAnimation(const WebActiveWheelFlingParameters&) = 0;

    virtual bool setEditableSelectionOffsets(int start, int end) = 0;
    virtual bool setCompositionFromExistingText(int compositionStart, int compositionEnd, const WebVector<WebCompositionUnderline>& underlines) = 0;
    virtual void extendSelectionAndDelete(int before, int after) = 0;

    virtual bool isSelectionEditable() const = 0;

    virtual void setShowPaintRects(bool) = 0;
    virtual void setShowFPSCounter(bool) = 0;
    virtual void setContinuousPaintingEnabled(bool) = 0;

    // Benchmarking support -------------------------------------------------

    virtual WebViewBenchmarkSupport* benchmarkSupport() { return 0; }

    // Visibility -----------------------------------------------------------

    // Sets the visibility of the WebView.
    virtual void setVisibilityState(WebPageVisibilityState visibilityState,
                                    bool isInitialState) { }

    // PageOverlay ----------------------------------------------------------

    // Adds/removes page overlay to this WebView. These functions change the
    // graphical appearance of the WebView. WebPageOverlay paints the
    // contents of the page overlay. It also provides an z-order number for
    // the page overlay. The z-order number defines the paint order the page
    // overlays. Page overlays with larger z-order number will be painted after
    // page overlays with smaller z-order number. That is, they appear above
    // the page overlays with smaller z-order number. If two page overlays have
    // the same z-order number, the later added one will be on top.
    virtual void addPageOverlay(WebPageOverlay*, int /*z-order*/) = 0;
    virtual void removePageOverlay(WebPageOverlay*) = 0;

    // Battery status API support -------------------------------------------

    // Updates the battery status in the BatteryClient. This also triggers the
    // appropriate JS events (e.g. sends a 'levelchange' event to JS if the
    // level is changed in this update from the previous update).
    virtual void updateBatteryStatus(const WebBatteryStatus&) { }

    // Testing functionality for TestRunner ---------------------------------

protected:
    ~WebView() {}
};

} // namespace WebKit

#endif
