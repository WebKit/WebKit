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

#ifndef WebViewClient_h
#define WebViewClient_h

#include "WebAccessibilityNotification.h"
#include "WebContentDetectionResult.h"
#include "WebDragOperation.h"
#include "WebEditingAction.h"
#include "WebFileChooserCompletion.h"
#include "WebFileChooserParams.h"
#include "WebPageVisibilityState.h"
#include "WebPopupType.h"
#include "WebTextAffinity.h"
#include "WebTextDirection.h"
#include "WebWidgetClient.h"
#include "platform/WebColor.h"
#include "platform/WebGraphicsContext3D.h"
#include "platform/WebString.h"

namespace WebKit {

class WebAccessibilityObject;
class WebBatteryStatusClient;
class WebColorChooser;
class WebColorChooserClient;
class WebCompositorOutputSurface;
class WebDateTimeChooserCompletion;
class WebDeviceOrientationClient;
class WebDragData;
class WebElement;
class WebExternalPopupMenu;
class WebExternalPopupMenuClient;
class WebFileChooserCompletion;
class WebFrame;
class WebGeolocationClient;
class WebGeolocationService;
class WebGestureEvent;
class WebHelperPlugin;
class WebHitTestResult;
class WebIconLoadingCompletion;
class WebImage;
class WebInputElement;
class WebKeyboardEvent;
class WebNode;
class WebNotificationPresenter;
class WebRange;
class WebSpeechInputController;
class WebSpeechInputListener;
class WebSpeechRecognizer;
class WebStorageNamespace;
class WebURL;
class WebURLRequest;
class WebUserMediaClient;
class WebView;
class WebWidget;
struct WebConsoleMessage;
struct WebContextMenuData;
struct WebDateTimeChooserParams;
struct WebPoint;
struct WebPopupMenuInfo;
struct WebRect;
struct WebSize;
struct WebWindowFeatures;

// Since a WebView is a WebWidget, a WebViewClient is a WebWidgetClient.
// Virtual inheritance allows an implementation of WebWidgetClient to be
// easily reused as part of an implementation of WebViewClient.
class WebViewClient : virtual public WebWidgetClient {
public:
    // Factory methods -----------------------------------------------------

    // Create a new related WebView.  This method must clone its session storage
    // so any subsequent calls to createSessionStorageNamespace conform to the
    // WebStorage specification.
    // The request parameter is only for the client to check if the request
    // could be fulfilled.  The client should not load the request.
    // The policy parameter indicates how the new view will be displayed in
    // WebWidgetClient::show.
    virtual WebView* createView(WebFrame* creator,
                                const WebURLRequest& request,
                                const WebWindowFeatures& features,
                                const WebString& name,
                                WebNavigationPolicy policy) {
        return 0;
    }

    // Create a new WebPopupMenu.  In the second form, the client is
    // responsible for rendering the contents of the popup menu.
    virtual WebWidget* createPopupMenu(WebPopupType) { return 0; }
    virtual WebWidget* createPopupMenu(const WebPopupMenuInfo&) { return 0; }
    virtual WebExternalPopupMenu* createExternalPopupMenu(
        const WebPopupMenuInfo&, WebExternalPopupMenuClient*) { return 0; }

    // Create a session storage namespace object associated with this WebView.
    virtual WebStorageNamespace* createSessionStorageNamespace(unsigned quota) { return 0; }

    // DEPRECATED: Creates a graphics context that renders to the client's WebView.
    virtual WebGraphicsContext3D* createGraphicsContext3D(const WebGraphicsContext3D::Attributes&) { return 0; }

    // Creates the output surface that renders to the client's WebView.
    virtual WebCompositorOutputSurface* createOutputSurface() { return 0; }

    // Misc ----------------------------------------------------------------

    // A new message was added to the console.
    virtual void didAddMessageToConsole(
        const WebConsoleMessage&, const WebString& sourceName, unsigned sourceLine) { }

    // Called when script in the page calls window.print().  If frame is
    // non-null, then it selects a particular frame, including its
    // children, to print.  Otherwise, the main frame and its children
    // should be printed.
    virtual void printPage(WebFrame*) { }

    // Called to retrieve the provider of desktop notifications.
    virtual WebNotificationPresenter* notificationPresenter() { return 0; }

    // Called to request an icon for the specified filenames.
    // The icon is shown in a file upload control.
    virtual bool queryIconForFiles(const WebVector<WebString>& filenames, WebIconLoadingCompletion*) { return false; }

    // This method enumerates all the files in the path. It returns immediately
    // and asynchronously invokes the WebFileChooserCompletion with all the
    // files in the directory. Returns false if the WebFileChooserCompletion
    // will never be called.
    virtual bool enumerateChosenDirectory(const WebString& path, WebFileChooserCompletion*) { return false; }

    // Creates the main WebFrame for the specified WebHelperPlugin.
    // Called by WebHelperPlugin to provide the WebFrameClient interface for the WebFrame.
    virtual void initializeHelperPluginWebFrame(WebHelperPlugin*) { }


    // Navigational --------------------------------------------------------

    // These notifications bracket any loading that occurs in the WebView.
    virtual void didStartLoading() { }
    virtual void didStopLoading() { }

    // Notification that some progress was made loading the current page.
    // loadProgress is a value between 0 (nothing loaded) and 1.0 (frame fully
    // loaded).
    virtual void didChangeLoadProgress(WebFrame*, double loadProgress) { }

    // Editing -------------------------------------------------------------

    // These methods allow the client to intercept and overrule editing
    // operations.
    virtual bool shouldBeginEditing(const WebRange&) { return true; }
    virtual bool shouldEndEditing(const WebRange&) { return true; }
    virtual bool shouldInsertNode(
        const WebNode&, const WebRange&, WebEditingAction) { return true; }
    virtual bool shouldInsertText(
        const WebString&, const WebRange&, WebEditingAction) { return true; }
    virtual bool shouldChangeSelectedRange(
        const WebRange& from, const WebRange& to, WebTextAffinity,
        bool stillSelecting) { return true; }
    virtual bool shouldDeleteRange(const WebRange&) { return true; }
    virtual bool shouldApplyStyle(const WebString& style, const WebRange&) { return true; }

    virtual bool isSmartInsertDeleteEnabled() { return true; }
    virtual bool isSelectTrailingWhitespaceEnabled() { return true; }

    virtual void didBeginEditing() { }
    virtual void didChangeSelection(bool isSelectionEmpty) { }
    virtual void didChangeContents() { }
    virtual void didExecuteCommand(const WebString& commandName) { }
    virtual void didEndEditing() { }
    virtual void didChangeFormState(const WebNode&) { }

    // This method is called in response to WebView's handleInputEvent()
    // when the default action for the current keyboard event is not
    // suppressed by the page, to give the embedder a chance to handle
    // the keyboard event specially.
    //
    // Returns true if the keyboard event was handled by the embedder,
    // indicating that the default action should be suppressed.
    virtual bool handleCurrentKeyboardEvent() { return false; }

    // Dialogs -------------------------------------------------------------

    // This method opens the color chooser and returns a new WebColorChooser
    // instance. If there is a WebColorChooser already from the last time this
    // was called, it ends the color chooser by calling endChooser, and replaces
    // it with the new one.
    virtual WebColorChooser* createColorChooser(WebColorChooserClient*,
                                                const WebColor&) { return 0; }

    // This method returns immediately after showing the dialog. When the
    // dialog is closed, it should call the WebFileChooserCompletion to
    // pass the results of the dialog. Returns false if
    // WebFileChooseCompletion will never be called.
    virtual bool runFileChooser(const WebFileChooserParams&,
                                WebFileChooserCompletion*) { return false; }

    // Ask users to choose date/time for the specified parameters. When a user
    // chooses a value, an implementation of this function should call
    // WebDateTimeChooserCompletion::didChooseValue or didCancelChooser. If the
    // implementation opened date/time chooser UI successfully, it should return
    // true. This function is used only if ExternalDateTimeChooser is used.
    virtual bool openDateTimeChooser(const WebDateTimeChooserParams&, WebDateTimeChooserCompletion*) { return false; }

    // Displays a modal alert dialog containing the given message.  Returns
    // once the user dismisses the dialog.
    virtual void runModalAlertDialog(
        WebFrame*, const WebString& message) { }

    // Displays a modal confirmation dialog with the given message as
    // description and OK/Cancel choices.  Returns true if the user selects
    // 'OK' or false otherwise.
    virtual bool runModalConfirmDialog(
        WebFrame*, const WebString& message) { return false; }

    // Displays a modal input dialog with the given message as description
    // and OK/Cancel choices.  The input field is pre-filled with
    // defaultValue.  Returns true if the user selects 'OK' or false
    // otherwise.  Upon returning true, actualValue contains the value of
    // the input field.
    virtual bool runModalPromptDialog(
        WebFrame*, const WebString& message, const WebString& defaultValue,
        WebString* actualValue) { return false; }

    // Displays a modal confirmation dialog containing the given message as
    // description and OK/Cancel choices, where 'OK' means that it is okay
    // to proceed with closing the view.  Returns true if the user selects
    // 'OK' or false otherwise.
    virtual bool runModalBeforeUnloadDialog(
        WebFrame*, const WebString& message) { return true; }


    // UI ------------------------------------------------------------------

    // Called when script modifies window.status
    virtual void setStatusText(const WebString&) { }

    // Called when hovering over an anchor with the given URL.
    virtual void setMouseOverURL(const WebURL&) { }

    // Called when keyboard focus switches to an anchor with the given URL.
    virtual void setKeyboardFocusURL(const WebURL&) { }

    // Shows a context menu with commands relevant to a specific element on
    // the given frame. Additional context data is supplied.
    virtual void showContextMenu(WebFrame*, const WebContextMenuData&) { }

    // Called when a drag-n-drop operation should begin.
    virtual void startDragging(WebFrame*, const WebDragData&, WebDragOperationsMask, const WebImage&, const WebPoint& dragImageOffset) { }

    // Called to determine if drag-n-drop operations may initiate a page
    // navigation.
    virtual bool acceptsLoadDrops() { return true; }

    // Take focus away from the WebView by focusing an adjacent UI element
    // in the containing window.
    virtual void focusNext() { }
    virtual void focusPrevious() { }

    // Called when a new node gets focused.
    virtual void focusedNodeChanged(const WebNode&) { }

    virtual void numberOfWheelEventHandlersChanged(unsigned) { }
    virtual void hasTouchEventHandlers(bool) { }

    // Indicates two things:
    //   1) This view may have a new layout now.
    //   2) Calling layout() is a no-op.
    // After calling WebWidget::layout(), expect to get this notification
    // unless the view did not need a layout.
    virtual void didUpdateLayout() { }

    // Return true to swallow the input event if the embedder will start a disambiguation popup
    virtual bool didTapMultipleTargets(const WebGestureEvent&, const WebVector<WebRect>& targetRects) { return false; }

    // Show a notification popup for the specified form vaidation messages
    // besides the anchor rectangle. An implementation of this function should
    // not hide the popup until hideValidationMessage call.
    virtual void showValidationMessage(const WebRect& anchorInScreen, const WebString& mainText, const WebString& supplementalText, WebTextDirection hint) { }
    // Hide notifation popup for form validation messages.
    virtual void hideValidationMessage() { }

    // Session history -----------------------------------------------------

    // Tells the embedder to navigate back or forward in session history by
    // the given offset (relative to the current position in session
    // history).
    virtual void navigateBackForwardSoon(int offset) { }

    // Returns the number of history items before/after the current
    // history item.
    virtual int historyBackListCount() { return 0; }
    virtual int historyForwardListCount() { return 0; }

    // Called to notify the embedder when a new history item is added.
    virtual void didAddHistoryItem() { }


    // Accessibility -------------------------------------------------------

    // Notifies embedder about an accessibility notification.
    virtual void postAccessibilityNotification(const WebAccessibilityObject&, WebAccessibilityNotification) { }


    // Developer tools -----------------------------------------------------

    // Called to notify the client that the inspector's settings were
    // changed and should be saved.  See WebView::inspectorSettings.
    virtual void didUpdateInspectorSettings() { }

    virtual void didUpdateInspectorSetting(const WebString& key, const WebString& value) { }

    // Geolocation ---------------------------------------------------------

    // Access the embedder API for (client-based) geolocation client .
    virtual WebGeolocationClient* geolocationClient() { return 0; }
    // Access the embedder API for (non-client-based) geolocation services.
    virtual WebGeolocationService* geolocationService() { return 0; }

    // Speech --------------------------------------------------------------

    // Access the embedder API for speech input services.
    virtual WebSpeechInputController* speechInputController(
        WebSpeechInputListener*) { return 0; }

    // Access the embedder API for speech recognition services.
    virtual WebSpeechRecognizer* speechRecognizer() { return 0; }

    // Device Orientation --------------------------------------------------

    // Access the embedder API for device orientation services.
    virtual WebDeviceOrientationClient* deviceOrientationClient() { return 0; }

    // Battery Status ------------------------------------------------------

    // Access the embedder API for battery status services.
    virtual WebBatteryStatusClient* batteryStatusClient() { return 0; }

    // Zoom ----------------------------------------------------------------

    // Informs the browser that the zoom levels for this frame have changed from
    // the default values.
    virtual void zoomLimitsChanged(double minimumLevel, double maximumLevel) { }

    // Informs the browser that the zoom level has changed as a result of an
    // action that wasn't initiated by the client.
    virtual void zoomLevelChanged() { }

    // Registers a new URL handler for the given protocol.
    virtual void registerProtocolHandler(const WebString& scheme,
                                         const WebString& baseUrl,
                                         const WebString& url,
                                         const WebString& title) { }

    // Visibility -----------------------------------------------------------

    // Returns the current visibility of the WebView.
    virtual WebPageVisibilityState visibilityState() const
    {
        return WebPageVisibilityStateVisible;
    }

    // Media Streams -------------------------------------------------------

    virtual WebUserMediaClient* userMediaClient() { return 0; }


    // Content detection ----------------------------------------------------

    // Retrieves detectable content (e.g., email addresses, phone numbers)
    // around a hit test result. The embedder should use platform-specific
    // content detectors to analyze the region around the hit test result.
    virtual WebContentDetectionResult detectContentAround(const WebHitTestResult&) { return WebContentDetectionResult(); }

    // Schedules a new content intent with the provided url.
    virtual void scheduleContentIntent(const WebURL&) { }

    // Cancels any previously scheduled content intents that have not yet launched.
    virtual void cancelScheduledContentIntents() { }

    // Draggable regions ----------------------------------------------------

    // Informs the browser that the draggable regions have been updated.
    virtual void draggableRegionsChanged() { }

protected:
    ~WebViewClient() { }
};

} // namespace WebKit

#endif
