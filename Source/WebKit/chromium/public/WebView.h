/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#include "WebDragOperation.h"
#include "WebPageVisibilityState.h"
#include "WebWidget.h"
#include "platform/WebString.h"
#include "platform/WebVector.h"

namespace WebKit {

class WebAccessibilityObject;
class WebAutofillClient;
class WebDevToolsAgent;
class WebDevToolsAgentClient;
class WebDragData;
class WebFrame;
class WebFrameClient;
class WebGraphicsContext3D;
class WebNode;
class WebPageOverlay;
class WebPermissionClient;
class WebSettings;
class WebSpellCheckClient;
class WebString;
class WebViewClient;
struct WebMediaPlayerAction;
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

    // Initializes the various client interfaces.
    virtual void setAutofillClient(WebAutofillClient*) = 0;
    virtual void setDevToolsAgentClient(WebDevToolsAgentClient*) = 0;
    virtual void setPermissionClient(WebPermissionClient*) = 0;
    virtual void setSpellCheckClient(WebSpellCheckClient*) = 0;


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

    // Gets the scale factor of the page, where 1.0 is the normal size, > 1.0
    // is scaled up, < 1.0 is scaled down.
    virtual float pageScaleFactor() const = 0;

    // Indicates wehther the page scale factor has been set since navigating
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
        bool enable,
        const WebSize& minSize,
        const WebSize& maxSize) = 0;


    // Media ---------------------------------------------------------------

    // Performs the specified action on the node at the given location.
    virtual void performMediaPlayerAction(
        const WebMediaPlayerAction&, const WebPoint& location) = 0;


    // Data exchange -------------------------------------------------------

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
        WebDragOperationsMask operationsAllowed) = 0;
    virtual WebDragOperation dragTargetDragOver(
        const WebPoint& clientPoint, const WebPoint& screenPoint,
        WebDragOperationsMask operationsAllowed) = 0;
    virtual void dragTargetDragLeave() = 0;
    virtual void dragTargetDrop(
        const WebPoint& clientPoint, const WebPoint& screenPoint) = 0;


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
    // |uniqueIDs| is a vector of IDs that represent the unique ID of each
    // Autofill profile in the suggestions popup. If a unique ID is 0, then the
    // corresponding suggestion comes from Autocomplete rather than Autofill.
    // If a unique ID is negative, then the corresponding "suggestion" is
    // actually a user-facing warning, e.g. explaining why Autofill is
    // unavailable for the current form.
    virtual void applyAutofillSuggestions(
        const WebNode&,
        const WebVector<WebString>& names,
        const WebVector<WebString>& labels,
        const WebVector<WebString>& icons,
        const WebVector<int>& uniqueIDs,
        int separatorIndex) = 0;

    // Hides any popup (suggestions, selects...) that might be showing.
    virtual void hidePopups() = 0;


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

    // Returns the (on-screen) WebGraphicsContext3D associated with
    // this WebView. One will be created if it doesn't already exist.
    // This is used to set up sharing between this context (which is
    // that used by the compositor) and contexts for WebGL and other
    // APIs.
    virtual WebGraphicsContext3D* graphicsContext3D() = 0;

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

    // Testing functionality for LayoutTestController -----------------------

    // Simulates a compositor lost context.
    virtual void loseCompositorContext(int numTimes) = 0;


protected:
    ~WebView() {}
};

} // namespace WebKit

#endif
