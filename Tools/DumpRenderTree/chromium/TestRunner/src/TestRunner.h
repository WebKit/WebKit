/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2010 Pawel Hajdan (phajdan.jr@chromium.org)
 * Copyright (C) 2012 Apple Inc. All Rights Reserved.
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

#ifndef TestRunner_h
#define TestRunner_h

#include "CppBoundClass.h"
#include "TestCommon.h"
#include "WebArrayBufferView.h"
#include "WebTask.h"
#include "WebTestRunner.h"
#include "WebTextDirection.h"
#include <deque>
#include <memory>
#include <public/WebURL.h>
#include <set>
#include <string>

namespace WebKit {
class WebArrayBufferView;
class WebNotificationPresenter;
class WebPermissionClient;
class WebView;
}

namespace WebTestRunner {

class NotificationPresenter;
class TestInterfaces;
class WebPermissions;
class WebTestDelegate;
class WebTestProxyBase;

class TestRunner : public WebTestRunner, public CppBoundClass {
public:
    explicit TestRunner(TestInterfaces*);
    virtual ~TestRunner();

    void setDelegate(WebTestDelegate*);
    void setWebView(WebKit::WebView*, WebTestProxyBase*);

    void reset();

    WebTaskList* taskList() { return &m_taskList; }

    void setTestIsRunning(bool);

    // WebTestRunner implementation.
    virtual bool shouldGeneratePixelResults() OVERRIDE;
    virtual bool shouldDumpAsAudio() const OVERRIDE;
    virtual const WebKit::WebArrayBufferView* audioData() const OVERRIDE;
    virtual bool shouldDumpBackForwardList() const OVERRIDE;
    virtual WebKit::WebPermissionClient* webPermissions() const OVERRIDE;

    // Methods used by WebTestProxyBase.
    bool shouldDumpSelectionRect() const;
    bool testRepaint() const;
    bool sweepHorizontally() const;
    bool isPrinting() const;
    bool shouldDumpAsText();
    bool shouldDumpChildFrameScrollPositions() const;
    bool shouldDumpChildFramesAsText() const;
    void showDevTools();
    void setShouldDumpAsText(bool);
    void setShouldGeneratePixelResults(bool);
    void setShouldDumpFrameLoadCallbacks(bool);
    bool shouldDumpEditingCallbacks() const;
    bool shouldDumpFrameLoadCallbacks() const;
    bool shouldDumpUserGestureInFrameLoadCallbacks() const;
    bool stopProvisionalFrameLoads() const;
    bool shouldDumpTitleChanges() const;
    bool shouldDumpCreateView() const;
    bool canOpenWindows() const;
    bool shouldDumpResourceLoadCallbacks() const;
    bool shouldDumpResourceRequestCallbacks() const;
    bool shouldDumpResourceResponseMIMETypes() const;
    bool shouldDumpStatusCallbacks() const;
    bool shouldDumpProgressFinishedCallback() const;
    bool deferMainResourceDataLoad() const;
    bool shouldStayOnPageAfterHandlingBeforeUnload() const;
    void setTitleTextDirection(WebKit::WebTextDirection);
    const std::set<std::string>* httpHeadersToClear() const;
    bool shouldBlockRedirects() const;
    bool willSendRequestShouldReturnNull() const;
    void setTopLoadingFrame(WebKit::WebFrame*, bool);
    WebKit::WebFrame* topLoadingFrame() const;
    void policyDelegateDone();
    bool policyDelegateEnabled() const;
    bool policyDelegateIsPermissive() const;
    bool policyDelegateShouldNotifyDone() const;
    bool shouldInterceptPostMessage() const;
    bool isSmartInsertDeleteEnabled() const;
    bool isSelectTrailingWhitespaceEnabled() const;
    bool shouldDumpResourcePriorities() const;
#if ENABLE_NOTIFICATIONS
    WebKit::WebNotificationPresenter* notificationPresenter() const;
#endif
    bool requestPointerLock();
    void requestPointerUnlock();
    bool isPointerLocked();

    // A single item in the work queue.
    class WorkItem {
    public:
        virtual ~WorkItem() { }

        // Returns true if this started a load.
        virtual bool run(WebTestDelegate*, WebKit::WebView*) = 0;
    };

private:
    friend class WorkQueue;

    // Helper class for managing events queued by methods like queueLoad or
    // queueScript.
    class WorkQueue {
    public:
        WorkQueue(TestRunner* controller) : m_frozen(false), m_controller(controller) { }
        virtual ~WorkQueue();
        void processWorkSoon();

        // Reset the state of the class between tests.
        void reset();

        void addWork(WorkItem*);

        void setFrozen(bool frozen) { m_frozen = frozen; }
        bool isEmpty() { return m_queue.empty(); }
        WebTaskList* taskList() { return &m_taskList; }

    private:
        void processWork();
        class WorkQueueTask: public WebMethodTask<WorkQueue> {
        public:
            WorkQueueTask(WorkQueue* object): WebMethodTask<WorkQueue>(object) { }
            virtual void runIfValid() { m_object->processWork(); }
        };

        WebTaskList m_taskList;
        std::deque<WorkItem*> m_queue;
        bool m_frozen;
        TestRunner* m_controller;
    };
    ///////////////////////////////////////////////////////////////////////////
    // Methods dealing with the test logic

    // By default, tests end when page load is complete. These methods are used
    // to delay the completion of the test until notifyDone is called.
    void waitUntilDone(const CppArgumentList&, CppVariant*);
    void notifyDone(const CppArgumentList&, CppVariant*);

    // Methods for adding actions to the work queue. Used in conjunction with
    // waitUntilDone/notifyDone above.
    void queueBackNavigation(const CppArgumentList&, CppVariant*);
    void queueForwardNavigation(const CppArgumentList&, CppVariant*);
    void queueReload(const CppArgumentList&, CppVariant*);
    void queueLoadingScript(const CppArgumentList&, CppVariant*);
    void queueNonLoadingScript(const CppArgumentList&, CppVariant*);
    void queueLoad(const CppArgumentList&, CppVariant*);
    void queueLoadHTMLString(const CppArgumentList&, CppVariant*);


    // Causes navigation actions just printout the intended navigation instead
    // of taking you to the page. This is used for cases like mailto, where you
    // don't actually want to open the mail program.
    void setCustomPolicyDelegate(const CppArgumentList&, CppVariant*);

    // Delays completion of the test until the policy delegate runs.
    void waitForPolicyDelegate(const CppArgumentList&, CppVariant*);

    // Functions for dealing with windows. By default we block all new windows.
    void windowCount(const CppArgumentList&, CppVariant*);
    void setCloseRemainingWindowsWhenComplete(const CppArgumentList&, CppVariant*);

    ///////////////////////////////////////////////////////////////////////////
    // Methods implemented entirely in terms of chromium's public WebKit API

    // Method that controls whether pressing Tab key cycles through page elements
    // or inserts a '\t' char in text area
    void setTabKeyCyclesThroughElements(const CppArgumentList&, CppVariant*);

    // Executes an internal command (superset of document.execCommand() commands).
    void execCommand(const CppArgumentList&, CppVariant*);

    // Checks if an internal command is currently available.
    void isCommandEnabled(const CppArgumentList&, CppVariant*);

    void elementDoesAutoCompleteForElementWithId(const CppArgumentList&, CppVariant*);
    void callShouldCloseOnWebView(const CppArgumentList&, CppVariant*);
    void setDomainRelaxationForbiddenForURLScheme(const CppArgumentList&, CppVariant*);
    void evaluateScriptInIsolatedWorldAndReturnValue(const CppArgumentList&, CppVariant*);
    void evaluateScriptInIsolatedWorld(const CppArgumentList&, CppVariant*);
    void setIsolatedWorldSecurityOrigin(const CppArgumentList&, CppVariant*);
    void setIsolatedWorldContentSecurityPolicy(const CppArgumentList&, CppVariant*);

    // Allows layout tests to manage origins' whitelisting.
    void addOriginAccessWhitelistEntry(const CppArgumentList&, CppVariant*);
    void removeOriginAccessWhitelistEntry(const CppArgumentList&, CppVariant*);

    // Returns true if the current page box has custom page size style for
    // printing.
    void hasCustomPageSizeStyle(const CppArgumentList&, CppVariant*);

    // Forces the selection colors for testing under Linux.
    void forceRedSelectionColors(const CppArgumentList&, CppVariant*);

    // Adds a user script or user style sheet to be injected into new documents.
    void addUserScript(const CppArgumentList&, CppVariant*);
    void addUserStyleSheet(const CppArgumentList&, CppVariant*);

    void startSpeechInput(const CppArgumentList&, CppVariant*);

    void findString(const CppArgumentList&, CppVariant*);

    // Expects the first argument to be an input element and the second argument to be a string value.
    // Forwards the setValueForUser() call to the element.
    void setValueForUser(const CppArgumentList&, CppVariant*);

    void enableFixedLayoutMode(const CppArgumentList&, CppVariant*);
    void setFixedLayoutSize(const CppArgumentList&, CppVariant*);

    void selectionAsMarkup(const CppArgumentList&, CppVariant*);

    // Enables or disables subpixel positioning (i.e. fractional X positions for
    // glyphs) in text rendering on Linux. Since this method changes global
    // settings, tests that call it must use their own custom font family for
    // all text that they render. If not, an already-cached style will be used,
    // resulting in the changed setting being ignored.
    void setTextSubpixelPositioning(const CppArgumentList&, CppVariant*);

    // Switch the visibility of the page.
    void setPageVisibility(const CppArgumentList&, CppVariant*);
    void resetPageVisibility(const CppArgumentList&, CppVariant*);

    // Changes the direction of the focused element.
    void setTextDirection(const CppArgumentList&, CppVariant*);

    // Retrieves the text surrounding a position in a text node.
    // Expects the first argument to be a text node, the second and third to be
    // point coordinates relative to the node and the fourth the maximum text
    // length to retrieve.
    void textSurroundingNode(const CppArgumentList&, CppVariant*);

    // Enable or disable smart insert/delete. This is enabled by default.
    void setSmartInsertDeleteEnabled(const CppArgumentList&, CppVariant*);

    // Enable or disable trailing whitespace selection on double click.
    void setSelectTrailingWhitespaceEnabled(const CppArgumentList&, CppVariant*);
    void enableAutoResizeMode(const CppArgumentList&, CppVariant*);
    void disableAutoResizeMode(const CppArgumentList&, CppVariant*);

    // DeviceOrientation related functions
    void setMockDeviceOrientation(const CppArgumentList&, CppVariant*);

    void didAcquirePointerLock(const CppArgumentList&, CppVariant*);
    void didNotAcquirePointerLock(const CppArgumentList&, CppVariant*);
    void didLosePointerLock(const CppArgumentList&, CppVariant*);
    void setPointerLockWillFailSynchronously(const CppArgumentList&, CppVariant*);
    void setPointerLockWillRespondAsynchronously(const CppArgumentList&, CppVariant*);

    ///////////////////////////////////////////////////////////////////////////
    // Methods modifying WebPreferences.

    // Passes through to WebPreferences which allows the user to have a custom
    // style sheet.
    void setUserStyleSheetEnabled(const CppArgumentList&, CppVariant*);
    void setUserStyleSheetLocation(const CppArgumentList&, CppVariant*);

    // Passes this preference through to WebSettings.
    void setAuthorAndUserStylesEnabled(const CppArgumentList&, CppVariant*);

    // Set the WebPreference that controls webkit's popup blocking.
    void setPopupBlockingEnabled(const CppArgumentList&, CppVariant*);

    void setJavaScriptCanAccessClipboard(const CppArgumentList&, CppVariant*);
    void setXSSAuditorEnabled(const CppArgumentList&, CppVariant*);
    void setAllowUniversalAccessFromFileURLs(const CppArgumentList&, CppVariant*);
    void setAllowFileAccessFromFileURLs(const CppArgumentList&, CppVariant*);
    void overridePreference(const CppArgumentList&, CppVariant*);

    // Enable or disable plugins.
    void setPluginsEnabled(const CppArgumentList&, CppVariant*);

    // Changes asynchronous spellchecking flag on the settings.
    void setAsynchronousSpellCheckingEnabled(const CppArgumentList&, CppVariant*);

    void setTouchDragDropEnabled(const CppArgumentList&, CppVariant*);

    ///////////////////////////////////////////////////////////////////////////
    // Methods that modify the state of TestRunner

    // This function sets a flag that tells the test_shell to print a line of
    // descriptive text for each editing command. It takes no arguments, and
    // ignores any that may be present.
    void dumpEditingCallbacks(const CppArgumentList&, CppVariant*);

    // This function sets a flag that tells the test_shell to dump pages as
    // plain text, rather than as a text representation of the renderer's state.
    // It takes an optional argument, whether to dump pixels results or not.
    void dumpAsText(const CppArgumentList&, CppVariant*);

    // This function sets a flag that tells the test_shell to print out the
    // scroll offsets of the child frames. It ignores all.
    void dumpChildFrameScrollPositions(const CppArgumentList&, CppVariant*);

    // This function sets a flag that tells the test_shell to recursively
    // dump all frames as plain text if the dumpAsText flag is set.
    // It takes no arguments, and ignores any that may be present.
    void dumpChildFramesAsText(const CppArgumentList&, CppVariant*);

    // Deals with Web Audio WAV file data.
    void setAudioData(const CppArgumentList&, CppVariant*);

    // This function sets a flag that tells the test_shell to print a line of
    // descriptive text for each frame load callback. It takes no arguments, and
    // ignores any that may be present.
    void dumpFrameLoadCallbacks(const CppArgumentList&, CppVariant*);

    // This function sets a flag that tells the test_shell to print a line of
    // user gesture status text for some frame load callbacks. It takes no
    // arguments, and ignores any that may be present.
    void dumpUserGestureInFrameLoadCallbacks(const CppArgumentList&, CppVariant*);

    // If true, causes provisional frame loads to be stopped for the remainder of
    // the test.
    void setStopProvisionalFrameLoads(const CppArgumentList&, CppVariant*);

    void dumpTitleChanges(const CppArgumentList&, CppVariant*);

    // This function sets a flag that tells the test_shell to dump all calls to
    // WebViewClient::createView().
    // It takes no arguments, and ignores any that may be present.
    void dumpCreateView(const CppArgumentList&, CppVariant*);

    void setCanOpenWindows(const CppArgumentList&, CppVariant*);

    // This function sets a flag that tells the test_shell to dump a descriptive
    // line for each resource load callback. It takes no arguments, and ignores
    // any that may be present.
    void dumpResourceLoadCallbacks(const CppArgumentList&, CppVariant*);

    // This function sets a flag that tells the test_shell to print a line of
    // descriptive text for each element that requested a resource. It takes no
    // arguments, and ignores any that may be present.
    void dumpResourceRequestCallbacks(const CppArgumentList&, CppVariant*);

    // This function sets a flag that tells the test_shell to dump the MIME type
    // for each resource that was loaded. It takes no arguments, and ignores any
    // that may be present.
    void dumpResourceResponseMIMETypes(const CppArgumentList&, CppVariant*);

    // WebPermissionClient related.
    void setImagesAllowed(const CppArgumentList&, CppVariant*);
    void setScriptsAllowed(const CppArgumentList&, CppVariant*);
    void setStorageAllowed(const CppArgumentList&, CppVariant*);
    void setPluginsAllowed(const CppArgumentList&, CppVariant*);
    void setAllowDisplayOfInsecureContent(const CppArgumentList&, CppVariant*);
    void setAllowRunningOfInsecureContent(const CppArgumentList&, CppVariant*);
    void dumpPermissionClientCallbacks(const CppArgumentList&, CppVariant*);

    // This function sets a flag that tells the test_shell to dump all calls
    // to window.status().
    // It takes no arguments, and ignores any that may be present.
    void dumpWindowStatusChanges(const CppArgumentList&, CppVariant*);

    // This function sets a flag that tells the test_shell to print a line of
    // descriptive text for the progress finished callback. It takes no
    // arguments, and ignores any that may be present.
    void dumpProgressFinishedCallback(const CppArgumentList&, CppVariant*);

    // This function sets a flag that tells the test_shell to print out a text
    // representation of the back/forward list. It ignores all arguments.
    void dumpBackForwardList(const CppArgumentList&, CppVariant*);

    void setDeferMainResourceDataLoad(const CppArgumentList&, CppVariant*);
    void dumpSelectionRect(const CppArgumentList&, CppVariant*);
    void testRepaint(const CppArgumentList&, CppVariant*);
    void repaintSweepHorizontally(const CppArgumentList&, CppVariant*);

    // Causes layout to happen as if targetted to printed pages.
    void setPrinting(const CppArgumentList&, CppVariant*);

    void setShouldStayOnPageAfterHandlingBeforeUnload(const CppArgumentList&, CppVariant*);

    // Causes WillSendRequest to clear certain headers.
    void setWillSendRequestClearHeader(const CppArgumentList&, CppVariant*);

    // Causes WillSendRequest to block redirects.
    void setWillSendRequestReturnsNullOnRedirect(const CppArgumentList&, CppVariant*);

    // Causes WillSendRequest to return an empty request.
    void setWillSendRequestReturnsNull(const CppArgumentList&, CppVariant*);

    // This function sets a flag that tells the test_shell to dump a descriptive
    // line for each resource load's priority and any time that priority
    // changes. It takes no arguments, and ignores any that may be present.
    void dumpResourceRequestPriorities(const CppArgumentList&, CppVariant*);

    ///////////////////////////////////////////////////////////////////////////
    // Methods interacting with the WebTestProxy

    ///////////////////////////////////////////////////////////////////////////
    // Methods forwarding to the WebTestDelegate

    // Shows DevTools window.
    void showWebInspector(const CppArgumentList&, CppVariant*);
    void closeWebInspector(const CppArgumentList&, CppVariant*);

    // Allows layout tests to exec scripts at WebInspector side.
    void evaluateInWebInspector(const CppArgumentList&, CppVariant*);

    // Clears all databases.
    void clearAllDatabases(const CppArgumentList&, CppVariant*);
    // Sets the default quota for all origins
    void setDatabaseQuota(const CppArgumentList&, CppVariant*);

    // Changes the cookie policy from the default to allow all cookies.
    void setAlwaysAcceptCookies(const CppArgumentList&, CppVariant*);

    // Gives focus to the window.
    void setWindowIsKey(const CppArgumentList&, CppVariant*);

    // Converts a URL starting with file:///tmp/ to the local mapping.
    void pathToLocalResource(const CppArgumentList&, CppVariant*);

    // Used to set the device scale factor.
    void setBackingScaleFactor(const CppArgumentList&, CppVariant*);

    // Calls setlocale(LC_ALL, ...) for a specified locale.
    // Resets between tests.
    void setPOSIXLocale(const CppArgumentList&, CppVariant*);

    // Gets the number of geolocation permissions requests pending.
    void numberOfPendingGeolocationPermissionRequests(const CppArgumentList&, CppVariant*);

    // Geolocation related functions.
    void setGeolocationPermission(const CppArgumentList&, CppVariant*);
    void setMockGeolocationPosition(const CppArgumentList&, CppVariant*);
    void setMockGeolocationPositionUnavailableError(const CppArgumentList&, CppVariant*);

#if ENABLE_NOTIFICATIONS
    // Grants permission for desktop notifications to an origin
    void grantWebNotificationPermission(const CppArgumentList&, CppVariant*);
    // Simulates a click on a desktop notification.
    void simulateLegacyWebNotificationClick(const CppArgumentList&, CppVariant*);
#endif

    // Speech input related functions.
    void addMockSpeechInputResult(const CppArgumentList&, CppVariant*);
    void setMockSpeechInputDumpRect(const CppArgumentList&, CppVariant*);
    void addMockSpeechRecognitionResult(const CppArgumentList&, CppVariant*);
    void setMockSpeechRecognitionError(const CppArgumentList&, CppVariant*);
    void wasMockSpeechRecognitionAborted(const CppArgumentList&, CppVariant*);

    void display(const CppArgumentList&, CppVariant*);
    void displayInvalidatedRegion(const CppArgumentList&, CppVariant*);

    //////////////////////////////////////////////////////////////////////////
    // Fallback and stub methods

    // The fallback method is called when a nonexistent method is called on
    // the layout test controller object.
    // It is usefull to catch typos in the JavaScript code (a few layout tests
    // do have typos in them) and it allows the script to continue running in
    // that case (as the Mac does).
    void fallbackMethod(const CppArgumentList&, CppVariant*);

    // Stub for not implemented methods.
    void notImplemented(const CppArgumentList&, CppVariant*);

    ///////////////////////////////////////////////////////////////////////////
    // Internal helpers
    void checkResponseMimeType();
    void completeNotifyDone(bool isTimeout);
    class NotifyDoneTimedOutTask: public WebMethodTask<TestRunner> {
    public:
        NotifyDoneTimedOutTask(TestRunner* object): WebMethodTask<TestRunner>(object) { }
        virtual void runIfValid() { m_object->completeNotifyDone(true); }
    };

    class HostMethodTask : public WebMethodTask<TestRunner> {
    public:
        typedef void (TestRunner::*CallbackMethodType)();
        HostMethodTask(TestRunner* object, CallbackMethodType callback)
            : WebMethodTask<TestRunner>(object)
            , m_callback(callback)
        { }

        virtual void runIfValid() { (m_object->*m_callback)(); }

    private:
        CallbackMethodType m_callback;
    };
    void didAcquirePointerLockInternal();
    void didNotAcquirePointerLockInternal();
    void didLosePointerLockInternal();

    bool elementDoesAutoCompleteForElementWithId(const WebKit::WebString&);
    bool cppVariantToBool(const CppVariant&);
    int32_t cppVariantToInt32(const CppVariant&);
    WebKit::WebString cppVariantToWebString(const CppVariant&);

    void printErrorMessage(const std::string&);

    // In the Mac code, this is called to trigger the end of a test after the
    // page has finished loading. From here, we can generate the dump for the
    // test.
    void locationChangeDone();

    bool m_testIsRunning;

    // When reset is called, go through and close all but the main test shell
    // window. By default, set to true but toggled to false using
    // setCloseRemainingWindowsWhenComplete().
    bool m_closeRemainingWindows;

    // If true, don't dump output until notifyDone is called.
    bool m_waitUntilDone;

    // Causes navigation actions just printout the intended navigation instead
    // of taking you to the page. This is used for cases like mailto, where you
    // don't actually want to open the mail program.
    bool m_policyDelegateEnabled;

    // Toggles the behavior of the policy delegate. If true, then navigations
    // will be allowed. Otherwise, they will be ignored (dropped).
    bool m_policyDelegateIsPermissive;

    // If true, the policy delegate will signal layout test completion.
    bool m_policyDelegateShouldNotifyDone;

    WorkQueue m_workQueue;

    WebKit::WebURL m_userStyleSheetLocation;

    // globalFlag is used by a number of layout tests in http/tests/security/dataURL.
    CppVariant m_globalFlag;

    // Bound variable to return the name of this platform (chromium).
    CppVariant m_platformName;

    // Bound variable tracking the directionality of the <title> tag.
    CppVariant m_titleTextDirection;

    // Bound variable counting the number of top URLs visited.
    CppVariant m_webHistoryItemCount;

    // Bound variable to set whether postMessages should be intercepted or not
    CppVariant m_interceptPostMessage;

    // If true, the test_shell will write a descriptive line for each editing
    // command.
    bool m_dumpEditingCallbacks;

    // If true, the test_shell will generate pixel results in dumpAsText mode
    bool m_generatePixelResults;

    // If true, the test_shell will produce a plain text dump rather than a
    // text representation of the renderer.
    bool m_dumpAsText;

    // If true and if dump_as_text_ is true, the test_shell will recursively
    // dump all frames as plain text.
    bool m_dumpChildFramesAsText;

    // If true, the test_shell will print out the child frame scroll offsets as
    // well.
    bool m_dumpChildFrameScrollPositions;

    // If true, the test_shell will output a base64 encoded WAVE file.
    bool m_dumpAsAudio;

    // If true, the test_shell will output a descriptive line for each frame
    // load callback.
    bool m_dumpFrameLoadCallbacks;

    // If true, the test_shell will output a line of the user gesture status
    // text for some frame load callbacks.
    bool m_dumpUserGestureInFrameLoadCallbacks;

    // If true, stops provisional frame loads during the
    // DidStartProvisionalLoadForFrame callback.
    bool m_stopProvisionalFrameLoads;

    // If true, output a message when the page title is changed.
    bool m_dumpTitleChanges;

    // If true, output a descriptive line each time WebViewClient::createView
    // is invoked.
    bool m_dumpCreateView;

    // If true, new windows can be opened via javascript or by plugins. By
    // default, set to false and can be toggled to true using
    // setCanOpenWindows().
    bool m_canOpenWindows;

    // If true, the test_shell will output a descriptive line for each resource
    // load callback.
    bool m_dumpResourceLoadCallbacks;

    // If true, the test_shell will output a descriptive line for each resource
    // request callback.
    bool m_dumpResourceRequestCallbacks;

    // If true, the test_shell will output the MIME type for each resource that 
    // was loaded.
    bool m_dumpResourceResponseMIMETypes;

    // If true, the test_shell will dump all changes to window.status.
    bool m_dumpWindowStatusChanges;

    // If true, the test_shell will output a descriptive line for the progress
    // finished callback.
    bool m_dumpProgressFinishedCallback;

    // If true, the test_shell will produce a dump of the back forward list as
    // well.
    bool m_dumpBackForwardList;

    // If false, all new requests will not defer the main resource data load.
    bool m_deferMainResourceDataLoad;

    // If true, the test_shell will draw the bounds of the current selection rect
    // taking possible transforms of the selection rect into account.
    bool m_dumpSelectionRect;

    // If true, pixel dump will be produced as a series of 1px-tall, view-wide
    // individual paints over the height of the view.
    bool m_testRepaint;

    // If true and test_repaint_ is true as well, pixel dump will be produced as
    // a series of 1px-wide, view-tall paints across the width of the view.
    bool m_sweepHorizontally;

    // If true, layout is to target printed pages.
    bool m_isPrinting;

    bool m_shouldStayOnPageAfterHandlingBeforeUnload;

    bool m_shouldBlockRedirects;

    bool m_willSendRequestShouldReturnNull;

    bool m_smartInsertDeleteEnabled;

    bool m_selectTrailingWhitespaceEnabled;

    bool m_shouldDumpResourcePriorities;

    std::set<std::string> m_httpHeadersToClear;

    // WAV audio data is stored here.
    WebKit::WebArrayBufferView m_audioData;

    // Used for test timeouts.
    WebTaskList m_taskList;

    TestInterfaces* m_testInterfaces;
    WebTestDelegate* m_delegate;
    WebKit::WebView* m_webView;
    WebTestProxyBase* m_proxy;

    // This is non-0 IFF a load is in progress.
    WebKit::WebFrame* m_topLoadingFrame;

    // WebPermissionClient mock object.
    std::auto_ptr<WebPermissions> m_webPermissions;

#if ENABLE_NOTIFICATIONS
    std::auto_ptr<NotificationPresenter> m_notificationPresenter;
#endif

    bool m_pointerLocked;
    enum {
        PointerLockWillSucceed,
        PointerLockWillRespondAsync,
        PointerLockWillFailSync,
    } m_pointerLockPlannedResult;
};

}

#endif // TestRunner_h
