/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2010 Pawel Hajdan (phajdan.jr@chromium.org)
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

/*
  LayoutTestController class:
  Bound to a JavaScript window.layoutTestController object using the
  CppBoundClass::bindToJavascript(), this allows layout tests that are run in
  the test_shell (or, in principle, any web page loaded into a client app built
  with this class) to control various aspects of how the tests are run and what
  sort of output they produce.
*/

#ifndef LayoutTestController_h
#define LayoutTestController_h

#include "CppBoundClass.h"
#include "Task.h"
#include "public/WebString.h"
#include "public/WebURL.h"
#include <wtf/Deque.h>
#include <wtf/OwnPtr.h>

namespace WebKit {
class WebDeviceOrientationClient;
class WebDeviceOrientationClientMock;
class WebSpeechInputController;
class WebSpeechInputControllerMock;
class WebSpeechInputListener;
}

class TestShell;

class LayoutTestController : public CppBoundClass {
public:
    // Builds the property and method lists needed to bind this class to a JS
    // object.
    LayoutTestController(TestShell*);

    ~LayoutTestController();

    // This function sets a flag that tells the test_shell to dump pages as
    // plain text, rather than as a text representation of the renderer's state.
    // It takes an optional argument, whether to dump pixels results or not.
    void dumpAsText(const CppArgumentList&, CppVariant*);

    // This function should set a flag that tells the test_shell to print a line
    // of descriptive text for each database command.  It should take no
    // arguments, and ignore any that may be present. However, at the moment, we
    // don't have any DB function that prints messages, so for now this function
    // doesn't do anything.
    void dumpDatabaseCallbacks(const CppArgumentList&, CppVariant*);

    // This function sets a flag that tells the test_shell to print a line of
    // descriptive text for each editing command.  It takes no arguments, and
    // ignores any that may be present.
    void dumpEditingCallbacks(const CppArgumentList&, CppVariant*);

    // This function sets a flag that tells the test_shell to print a line of
    // descriptive text for each frame load callback.  It takes no arguments, and
    // ignores any that may be present.
    void dumpFrameLoadCallbacks(const CppArgumentList&, CppVariant*);

    // This function sets a flag that tells the test_shell to print out a text
    // representation of the back/forward list.  It ignores all arguments.
    void dumpBackForwardList(const CppArgumentList&, CppVariant*);

    // This function sets a flag that tells the test_shell to print out the
    // scroll offsets of the child frames.  It ignores all.
    void dumpChildFrameScrollPositions(const CppArgumentList&, CppVariant*);

    // This function sets a flag that tells the test_shell to recursively
    // dump all frames as plain text if the dumpAsText flag is set.
    // It takes no arguments, and ignores any that may be present.
    void dumpChildFramesAsText(const CppArgumentList&, CppVariant*);
    
    // This function sets a flag that tells the test_shell to dump a descriptive
    // line for each resource load callback. It takes no arguments, and ignores
    // any that may be present.
    void dumpResourceLoadCallbacks(const CppArgumentList&, CppVariant*);    
    
    // This function sets a flag that tells the test_shell to dump the MIME type
    // for each resource that was loaded. It takes no arguments, and ignores any
    // that may be present.
    void dumpResourceResponseMIMETypes(const CppArgumentList&, CppVariant*);

    // This function sets a flag that tells the test_shell to dump all calls
    // to window.status().
    // It takes no arguments, and ignores any that may be present.
    void dumpWindowStatusChanges(const CppArgumentList&, CppVariant*);

    // When called with a boolean argument, this sets a flag that controls
    // whether content-editable elements accept editing focus when an editing
    // attempt is made. It ignores any additional arguments.
    void setAcceptsEditing(const CppArgumentList&, CppVariant*);

    // Functions for dealing with windows.  By default we block all new windows.
    void windowCount(const CppArgumentList&, CppVariant*);
    void setCanOpenWindows(const CppArgumentList&, CppVariant*);
    void setCloseRemainingWindowsWhenComplete(const CppArgumentList&, CppVariant*);

    // By default, tests end when page load is complete.  These methods are used
    // to delay the completion of the test until notifyDone is called.
    void waitUntilDone(const CppArgumentList&, CppVariant*);
    void notifyDone(const CppArgumentList&, CppVariant*);

    // Methods for adding actions to the work queue.  Used in conjunction with
    // waitUntilDone/notifyDone above.
    void queueBackNavigation(const CppArgumentList&, CppVariant*);
    void queueForwardNavigation(const CppArgumentList&, CppVariant*);
    void queueReload(const CppArgumentList&, CppVariant*);
    void queueLoadingScript(const CppArgumentList&, CppVariant*);
    void queueNonLoadingScript(const CppArgumentList&, CppVariant*);
    void queueLoad(const CppArgumentList&, CppVariant*);
    void queueLoadHTMLString(const CppArgumentList&, CppVariant*);

    // Although this is named "objC" to match the Mac version, it actually tests
    // the identity of its two arguments in C++.
    void objCIdentityIsEqual(const CppArgumentList&, CppVariant*);

    // Changes the cookie policy from the default to allow all cookies.
    void setAlwaysAcceptCookies(const CppArgumentList&, CppVariant*);

    // Shows DevTools window.
    void showWebInspector(const CppArgumentList&, CppVariant*);
    void closeWebInspector(const CppArgumentList&, CppVariant*);

    // Gives focus to the window.
    void setWindowIsKey(const CppArgumentList&, CppVariant*);

    // Method that controls whether pressing Tab key cycles through page elements
    // or inserts a '\t' char in text area
    void setTabKeyCyclesThroughElements(const CppArgumentList&, CppVariant*);

    // Passes through to WebPreferences which allows the user to have a custom
    // style sheet.
    void setUserStyleSheetEnabled(const CppArgumentList&, CppVariant*);
    void setUserStyleSheetLocation(const CppArgumentList&, CppVariant*);

    // Passes this preference through to WebSettings.
    void setAuthorAndUserStylesEnabled(const CppArgumentList&, CppVariant*);

    // Puts Webkit in "dashboard compatibility mode", which is used in obscure
    // Mac-only circumstances. It's not really necessary, and will most likely
    // never be used by Chrome, but some layout tests depend on its presence.
    void setUseDashboardCompatibilityMode(const CppArgumentList&, CppVariant*);

    void setScrollbarPolicy(const CppArgumentList&, CppVariant*);

    // Causes navigation actions just printout the intended navigation instead
    // of taking you to the page. This is used for cases like mailto, where you
    // don't actually want to open the mail program.
    void setCustomPolicyDelegate(const CppArgumentList&, CppVariant*);

    // Delays completion of the test until the policy delegate runs.
    void waitForPolicyDelegate(const CppArgumentList&, CppVariant*);

    // Causes WillSendRequest to clear certain headers.
    void setWillSendRequestClearHeader(const CppArgumentList&, CppVariant*);

    // Causes WillSendRequest to block redirects.
    void setWillSendRequestReturnsNullOnRedirect(const CppArgumentList&, CppVariant*);

    // Causes WillSendRequest to return an empty request.
    void setWillSendRequestReturnsNull(const CppArgumentList&, CppVariant*);

    // Converts a URL starting with file:///tmp/ to the local mapping.
    void pathToLocalResource(const CppArgumentList&, CppVariant*);

    // Sets a bool such that when a drag is started, we fill the drag clipboard
    // with a fake file object.
    void addFileToPasteboardOnDrag(const CppArgumentList&, CppVariant*);

    // Executes an internal command (superset of document.execCommand() commands).
    void execCommand(const CppArgumentList&, CppVariant*);

    // Checks if an internal command is currently available.
    void isCommandEnabled(const CppArgumentList&, CppVariant*);

    // Set the WebPreference that controls webkit's popup blocking.
    void setPopupBlockingEnabled(const CppArgumentList&, CppVariant*);

    // If true, causes provisional frame loads to be stopped for the remainder of
    // the test.
    void setStopProvisionalFrameLoads(const CppArgumentList&, CppVariant*);

    // Enable or disable smart insert/delete.  This is enabled by default.
    void setSmartInsertDeleteEnabled(const CppArgumentList&, CppVariant*);

    // Enable or disable trailing whitespace selection on double click.
    void setSelectTrailingWhitespaceEnabled(const CppArgumentList&, CppVariant*);

    void pauseAnimationAtTimeOnElementWithId(const CppArgumentList&, CppVariant*);
    void pauseTransitionAtTimeOnElementWithId(const CppArgumentList&, CppVariant*);
    void elementDoesAutoCompleteForElementWithId(const CppArgumentList&, CppVariant*);
    void numberOfActiveAnimations(const CppArgumentList&, CppVariant*);
    void suspendAnimations(const CppArgumentList&, CppVariant*);
    void resumeAnimations(const CppArgumentList&, CppVariant*);
    void sampleSVGAnimationForElementAtTime(const CppArgumentList&, CppVariant*);
    void disableImageLoading(const CppArgumentList&, CppVariant*);
    void setIconDatabaseEnabled(const CppArgumentList&, CppVariant*);
    void dumpSelectionRect(const CppArgumentList&, CppVariant*);

    // Grants permission for desktop notifications to an origin
    void grantDesktopNotificationPermission(const CppArgumentList&, CppVariant*);
    // Simulates a click on a desktop notification.
    void simulateDesktopNotificationClick(const CppArgumentList&, CppVariant*);

    void setDomainRelaxationForbiddenForURLScheme(const CppArgumentList&, CppVariant*);
    void setDeferMainResourceDataLoad(const CppArgumentList&, CppVariant*);
    void setEditingBehavior(const CppArgumentList&, CppVariant*);

    // The following are only stubs.  TODO(pamg): Implement any of these that
    // are needed to pass the layout tests.
    void dumpAsWebArchive(const CppArgumentList&, CppVariant*);
    void dumpTitleChanges(const CppArgumentList&, CppVariant*);
    void setMainFrameIsFirstResponder(const CppArgumentList&, CppVariant*);
    void display(const CppArgumentList&, CppVariant*);
    void testRepaint(const CppArgumentList&, CppVariant*);
    void repaintSweepHorizontally(const CppArgumentList&, CppVariant*);
    void clearBackForwardList(const CppArgumentList&, CppVariant*);
    void keepWebHistory(const CppArgumentList&, CppVariant*);
    void storeWebScriptObject(const CppArgumentList&, CppVariant*);
    void accessStoredWebScriptObject(const CppArgumentList&, CppVariant*);
    void objCClassNameOf(const CppArgumentList&, CppVariant*);
    void addDisallowedURL(const CppArgumentList&, CppVariant*);
    void callShouldCloseOnWebView(const CppArgumentList&, CppVariant*);
    void setCallCloseOnWebViews(const CppArgumentList&, CppVariant*);
    void setPrivateBrowsingEnabled(const CppArgumentList&, CppVariant*);

    void setJavaScriptCanAccessClipboard(const CppArgumentList&, CppVariant*);
    void setXSSAuditorEnabled(const CppArgumentList&, CppVariant*);
    void evaluateScriptInIsolatedWorld(const CppArgumentList&, CppVariant*);
    void overridePreference(const CppArgumentList&, CppVariant*);
    void setAllowUniversalAccessFromFileURLs(const CppArgumentList&, CppVariant*);
    void setAllowFileAccessFromFileURLs(const CppArgumentList&, CppVariant*);


    // The fallback method is called when a nonexistent method is called on
    // the layout test controller object.
    // It is usefull to catch typos in the JavaScript code (a few layout tests
    // do have typos in them) and it allows the script to continue running in
    // that case (as the Mac does).
    void fallbackMethod(const CppArgumentList&, CppVariant*);

    // Allows layout tests to manage origins' whitelisting.
    void addOriginAccessWhitelistEntry(const CppArgumentList&, CppVariant*);
    void removeOriginAccessWhitelistEntry(const CppArgumentList&, CppVariant*);

    // Clears all Application Caches.
    void clearAllApplicationCaches(const CppArgumentList&, CppVariant*);
    // Sets the Application Quota for the localhost origin.
    void setApplicationCacheOriginQuota(const CppArgumentList&, CppVariant*);

    // Clears all databases.
    void clearAllDatabases(const CppArgumentList&, CppVariant*);
    // Sets the default quota for all origins
    void setDatabaseQuota(const CppArgumentList&, CppVariant*);

    // Calls setlocale(LC_ALL, ...) for a specified locale.
    // Resets between tests.
    void setPOSIXLocale(const CppArgumentList&, CppVariant*);

    // Gets the value of the counter in the element specified by its ID.
    void counterValueForElementById(const CppArgumentList&, CppVariant*);

    // Gets the number of page where the specified element will be put.
    void pageNumberForElementById(const CppArgumentList&, CppVariant*);

    // Gets the number of pages to be printed.
    void numberOfPages(const CppArgumentList&, CppVariant*);


    // Allows layout tests to start Timeline profiling.
    void setTimelineProfilingEnabled(const CppArgumentList&, CppVariant*);

    // Allows layout tests to exec scripts at WebInspector side.
    void evaluateInWebInspector(const CppArgumentList&, CppVariant*);

    // Forces the selection colors for testing under Linux.
    void forceRedSelectionColors(const CppArgumentList&, CppVariant*);

    // Adds a user script or user style sheet to be injected into new documents.
    void addUserScript(const CppArgumentList&, CppVariant*);
    void addUserStyleSheet(const CppArgumentList&, CppVariant*);

    // DeviceOrientation related functions
    void setMockDeviceOrientation(const CppArgumentList&, CppVariant*);

    // Geolocation related functions.
    void setGeolocationPermission(const CppArgumentList&, CppVariant*);
    void setMockGeolocationPosition(const CppArgumentList&, CppVariant*);
    void setMockGeolocationError(const CppArgumentList&, CppVariant*);

    // Empty stub method to keep parity with object model exposed by global LayoutTestController.
    void abortModal(const CppArgumentList&, CppVariant*);

    // Speech input related functions.
    void setMockSpeechInputResult(const CppArgumentList&, CppVariant*);

    void markerTextForListItem(const CppArgumentList&, CppVariant*);

public:
    // The following methods are not exposed to JavaScript.
    void setWorkQueueFrozen(bool frozen) { m_workQueue.setFrozen(frozen); }

    WebKit::WebSpeechInputController* speechInputController(WebKit::WebSpeechInputListener*);
    WebKit::WebDeviceOrientationClient* deviceOrientationClient();
    bool shouldDumpAsText() { return m_dumpAsText; }
    bool shouldDumpEditingCallbacks() { return m_dumpEditingCallbacks; }
    bool shouldDumpFrameLoadCallbacks() { return m_dumpFrameLoadCallbacks; }
    void setShouldDumpFrameLoadCallbacks(bool value) { m_dumpFrameLoadCallbacks = value; }
    bool shouldDumpResourceLoadCallbacks() {return m_dumpResourceLoadCallbacks; }
    void setShouldDumpResourceResponseMIMETypes(bool value) { m_dumpResourceResponseMIMETypes = value; }
    bool shouldDumpResourceResponseMIMETypes() {return m_dumpResourceResponseMIMETypes; }
    bool shouldDumpStatusCallbacks() { return m_dumpWindowStatusChanges; }
    bool shouldDumpSelectionRect() { return m_dumpSelectionRect; }
    bool shouldDumpBackForwardList() { return m_dumpBackForwardList; }
    bool shouldDumpTitleChanges() { return m_dumpTitleChanges; }
    bool shouldDumpChildFrameScrollPositions() { return m_dumpChildFrameScrollPositions; }
    bool shouldDumpChildFramesAsText() { return m_dumpChildFramesAsText; }
    bool shouldGeneratePixelResults() { return m_generatePixelResults; }
    bool acceptsEditing() { return m_acceptsEditing; }
    bool canOpenWindows() { return m_canOpenWindows; }
    bool shouldAddFileToPasteboard() { return m_shouldAddFileToPasteboard; }
    bool stopProvisionalFrameLoads() { return m_stopProvisionalFrameLoads; }
    bool deferMainResourceDataLoad() { return m_deferMainResourceDataLoad; }

    bool testRepaint() const { return m_testRepaint; }
    bool sweepHorizontally() const { return m_sweepHorizontally; }

    // Called by the webview delegate when the toplevel frame load is done.
    void locationChangeDone();

    // Called by the webview delegate when the policy delegate runs if the
    // waitForPolicyDelegate was called.
    void policyDelegateDone();

    // Reinitializes all static values.  The reset() method should be called
    // before the start of each test (currently from
    // TestShell::runFileTest).
    void reset();

    // A single item in the work queue.
    class WorkItem {
    public:
        virtual ~WorkItem() {}

        // Returns true if this started a load.
        virtual bool run(TestShell* shell) = 0;
    };

    TaskList* taskList() { return &m_taskList; }

private:
    friend class WorkItem;
    friend class WorkQueue;

    // Helper class for managing events queued by methods like queueLoad or
    // queueScript.
    class WorkQueue {
    public:
        WorkQueue(LayoutTestController* controller) : m_frozen(false), m_controller(controller) {}
        virtual ~WorkQueue();
        void processWorkSoon();

        // Reset the state of the class between tests.
        void reset();

        void addWork(WorkItem* work);

        void setFrozen(bool frozen) { m_frozen = frozen; }
        bool isEmpty() { return m_queue.isEmpty(); }
        TaskList* taskList() { return &m_taskList; }

    private:
        void processWork();
        class WorkQueueTask: public MethodTask<WorkQueue> {
        public:
            WorkQueueTask(WorkQueue* object): MethodTask<WorkQueue>(object) {}
            virtual void runIfValid() { m_object->processWork(); }
        };

        TaskList m_taskList;
        Deque<WorkItem*> m_queue;
        bool m_frozen;
        LayoutTestController* m_controller;
    };

    // Support for overridePreference.
    bool cppVariantToBool(const CppVariant&);
    int32_t cppVariantToInt32(const CppVariant&);
    WebKit::WebString cppVariantToWebString(const CppVariant&);

    void logErrorToConsole(const std::string&);
    void completeNotifyDone(bool isTimeout);
    class NotifyDoneTimedOutTask: public MethodTask<LayoutTestController> {
    public:
        NotifyDoneTimedOutTask(LayoutTestController* object): MethodTask<LayoutTestController>(object) {}
        virtual void runIfValid() { m_object->completeNotifyDone(true); }
    };


    bool pauseAnimationAtTimeOnElementWithId(const WebKit::WebString& animationName, double time, const WebKit::WebString& elementId);
    bool pauseTransitionAtTimeOnElementWithId(const WebKit::WebString& propertyName, double time, const WebKit::WebString& elementId);
    bool elementDoesAutoCompleteForElementWithId(const WebKit::WebString&);
    int numberOfActiveAnimations();
    void suspendAnimations();
    void resumeAnimations();

    // Used for test timeouts.
    TaskList m_taskList;

    // Non-owning pointer.  The LayoutTestController is owned by the host.
    TestShell* m_shell;

    // If true, the test_shell will produce a plain text dump rather than a
    // text representation of the renderer.
    bool m_dumpAsText;

    // If true, the test_shell will write a descriptive line for each editing
    // command.
    bool m_dumpEditingCallbacks;

    // If true, the test_shell will draw the bounds of the current selection rect
    // taking possible transforms of the selection rect into account.
    bool m_dumpSelectionRect;

    // If true, the test_shell will output a descriptive line for each frame
    // load callback.
    bool m_dumpFrameLoadCallbacks;

    // If true, the test_shell will output a descriptive line for each resource
    // load callback.
    bool m_dumpResourceLoadCallbacks;
    
    // If true, the test_shell will output the MIME type for each resource that 
    // was loaded.
    bool m_dumpResourceResponseMIMETypes;

    // If true, the test_shell will produce a dump of the back forward list as
    // well.
    bool m_dumpBackForwardList;

    // If true, the test_shell will print out the child frame scroll offsets as
    // well.
    bool m_dumpChildFrameScrollPositions;

    // If true and if dump_as_text_ is true, the test_shell will recursively
    // dump all frames as plain text.
    bool m_dumpChildFramesAsText;

    // If true, the test_shell will dump all changes to window.status.
    bool m_dumpWindowStatusChanges;

    // If true, output a message when the page title is changed.
    bool m_dumpTitleChanges;

    // If true, the test_shell will generate pixel results in dumpAsText mode
    bool m_generatePixelResults;

    // If true, the element will be treated as editable.  This value is returned
    // from various editing callbacks that are called just before edit operations
    // are allowed.
    bool m_acceptsEditing;

    // If true, new windows can be opened via javascript or by plugins.  By
    // default, set to false and can be toggled to true using
    // setCanOpenWindows().
    bool m_canOpenWindows;

    // When reset is called, go through and close all but the main test shell
    // window.  By default, set to true but toggled to false using
    // setCloseRemainingWindowsWhenComplete().
    bool m_closeRemainingWindows;

    // If true, pixel dump will be produced as a series of 1px-tall, view-wide
    // individual paints over the height of the view.
    bool m_testRepaint;
    // If true and test_repaint_ is true as well, pixel dump will be produced as
    // a series of 1px-wide, view-tall paints across the width of the view.
    bool m_sweepHorizontally;

    // If true and a drag starts, adds a file to the drag&drop clipboard.
    bool m_shouldAddFileToPasteboard;

    // If true, stops provisional frame loads during the
    // DidStartProvisionalLoadForFrame callback.
    bool m_stopProvisionalFrameLoads;

    // If true, don't dump output until notifyDone is called.
    bool m_waitUntilDone;

    // If false, all new requests will not defer the main resource data load.
    bool m_deferMainResourceDataLoad;

    WorkQueue m_workQueue;

    CppVariant m_globalFlag;

    // Bound variable counting the number of top URLs visited.
    CppVariant m_webHistoryItemCount;

    WebKit::WebURL m_userStyleSheetLocation;

    OwnPtr<WebKit::WebSpeechInputControllerMock> m_speechInputControllerMock;

    OwnPtr<WebKit::WebDeviceOrientationClientMock> m_deviceOrientationClientMock;
};

#endif // LayoutTestController_h
