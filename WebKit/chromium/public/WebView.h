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
#include "WebString.h"
#include "WebVector.h"
#include "WebWidget.h"

namespace WebKit {

class WebAccessibilityObject;
class WebDevToolsAgent;
class WebDevToolsAgentClient;
class WebDragData;
class WebFrame;
class WebFrameClient;
class WebGLES2Context;
class WebGraphicsContext3D;
class WebNode;
class WebSettings;
class WebString;
class WebViewClient;
struct WebMediaPlayerAction;
struct WebPoint;

class WebView : public WebWidget {
public:
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

    // Initialization ------------------------------------------------------

    // Creates a WebView that is NOT yet initialized.  You will need to
    // call initializeMainFrame to finish the initialization.  It is valid
    // to pass null WebViewClient and WebDevToolsAgentClient pointers.
    WEBKIT_API static WebView* create(WebViewClient*, WebDevToolsAgentClient*);

    // After creating a WebView, you should immediately call this method.
    // You can optionally modify the settings before calling this method.
    // The WebFrameClient will receive events for the main frame and any
    // child frames.  It is valid to pass a null WebFrameClient pointer.
    virtual void initializeMainFrame(WebFrameClient*) = 0;


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


    // Zoom ----------------------------------------------------------------

    // Returns the current zoom level.  0 is "original size", and each increment
    // above or below represents zooming 20% larger or smaller to limits of 300%
    // and 50% of original size, respectively.
    virtual int zoomLevel() = 0;

    // Changes the zoom level to the specified level, clamping at the limits
    // noted above, and returns the current zoom level after applying the
    // change.
    //
    // If |textOnly| is set, only the text will be zoomed; otherwise the entire
    // page will be zoomed. You can only have either text zoom or full page zoom
    // at one time.  Changing the mode while the page is zoomed will have odd
    // effects.
    virtual int setZoomLevel(bool textOnly, int zoomLevel) = 0;


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
        const WebDragData&, int identity,
        const WebPoint& clientPoint, const WebPoint& screenPoint,
        WebDragOperationsMask operationsAllowed) = 0;
    virtual WebDragOperation dragTargetDragOver(
        const WebPoint& clientPoint, const WebPoint& screenPoint,
        WebDragOperationsMask operationsAllowed) = 0;
    virtual void dragTargetDragLeave() = 0;
    virtual void dragTargetDrop(
        const WebPoint& clientPoint, const WebPoint& screenPoint) = 0;

    virtual int dragIdentity() = 0;

    // Helper method for drag and drop target operations: override the
    // default drop effect with either a "copy" (accept true) or "none"
    // (accept false) effect.  Return true on success.
    virtual bool setDropEffect(bool accept) = 0;


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


    // AutoFill  -----------------------------------------------------------

    // DEPRECATED.
    virtual void applyAutoFillSuggestions(
        const WebNode&,
        const WebVector<WebString>& names,
        const WebVector<WebString>& labels,
        const WebVector<int>& uniqueIDs,
        int separatorIndex) = 0;

    // Notifies the WebView that AutoFill suggestions are available for a node.
    // |uniqueIDs| is a vector of IDs that represent the unique ID of each
    // AutoFill profile in the suggestions popup.
    virtual void applyAutoFillSuggestions(
        const WebNode&,
        const WebVector<WebString>& names,
        const WebVector<WebString>& labels,
        const WebVector<WebString>& icons,
        const WebVector<int>& uniqueIDs,
        int separatorIndex) = 0;

    // Notifies the WebView that Autocomplete suggestions are available for a
    // node.
    // DEPRECATED: merging with applyAutoFillSuggestions.
    virtual void applyAutocompleteSuggestions(
        const WebNode&,
        const WebVector<WebString>& suggestions,
        int defaultSuggestionIndex) = 0;

    // Hides any popup (suggestions, selects...) that might be showing.
    virtual void hidePopups() = 0;


    // Context menu --------------------------------------------------------

    virtual void performCustomContextMenuAction(unsigned action) = 0;


    // Visited link state --------------------------------------------------

    // Tells all WebView instances to update the visited link state for the
    // specified hash.
    WEBKIT_API static void updateVisitedLinkState(unsigned long long hash);

    // Tells all WebView instances to update the visited state for all
    // their links.
    WEBKIT_API static void resetVisitedLinkState();


    // Custom colors -------------------------------------------------------

    virtual void setScrollbarColors(unsigned inactiveColor,
                                    unsigned activeColor,
                                    unsigned trackColor) = 0;

    virtual void setSelectionColors(unsigned activeBackgroundColor,
                                    unsigned activeForegroundColor,
                                    unsigned inactiveBackgroundColor,
                                    unsigned inactiveForegroundColor) = 0;

    // User scripts --------------------------------------------------------
    // FIXME: These two methods are DEPRECATED. Remove once Chromium has been rolled.
    virtual void addUserScript(const WebString& sourceCode, bool runAtStart)
    {
        addUserScript(sourceCode, WebVector<WebString>(),
                      runAtStart ? UserScriptInjectAtDocumentStart : UserScriptInjectAtDocumentEnd,
                      UserContentInjectInAllFrames);
    }
    virtual void addUserStyleSheet(const WebString& sourceCode)
    {
        addUserStyleSheet(sourceCode, WebVector<WebString>(), UserContentInjectInAllFrames);
    }

    WEBKIT_API static void addUserScript(const WebString& sourceCode,
                                         const WebVector<WebString>& patterns,
                                         UserScriptInjectAt injectAt,
                                         UserContentInjectIn injectIn);
    WEBKIT_API static void addUserStyleSheet(const WebString& sourceCode,
                                             const WebVector<WebString>& patterns,
                                             UserContentInjectIn injectIn);
    WEBKIT_API static void removeAllUserContent();

    // Modal dialog support ------------------------------------------------

    // Call these methods before and after running a nested, modal event loop
    // to suspend script callbacks and resource loads.
    WEBKIT_API static void willEnterModalLoop();
    WEBKIT_API static void didExitModalLoop();

    // GPU acceleration support --------------------------------------------

    // Returns the GLES2Context associated with this WebView. One will be
    // created if it doesn't already exist.
    // FIXME: remove this method once the compositor is fully switched
    // over to GraphicsContext3D.
    virtual WebGLES2Context* gles2Context() = 0;

    // Returns the (on-screen) WebGraphicsContext3D associated with
    // this WebView. One will be created if it doesn't already exist.
    // This is used to set up sharing between this context (which is
    // that used by the compositor) and contexts for WebGL and other
    // APIs.
    virtual WebGraphicsContext3D* graphicsContext3D() = 0;

protected:
    ~WebView() {}
};

} // namespace WebKit

#endif
