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

/*
  DRTTestRunner class:
  Bound to a JavaScript window.testRunner object using the
  CppBoundClass::bindToJavascript(), this allows layout tests that are run in
  the test_shell (or, in principle, any web page loaded into a client app built
  with this class) to control various aspects of how the tests are run and what
  sort of output they produce.
*/

#ifndef DRTTestRunner_h
#define DRTTestRunner_h

#include "TestRunner/src/TestRunner.h"
#include "WebTask.h"
#include "WebTextDirection.h"
#include "platform/WebArrayBufferView.h"
#include "platform/WebString.h"
#include "platform/WebURL.h"
#include <wtf/Deque.h>
#include <wtf/OwnPtr.h>

namespace WebKit {
class WebGeolocationClientMock;
}

namespace webkit_support {
class ScopedTempDirectory;
}


class TestShell;

using WebTestRunner::CppArgumentList;
using WebTestRunner::CppVariant;
using WebTestRunner::WebMethodTask;
using WebTestRunner::WebTaskList;

class DRTTestRunner : public WebTestRunner::TestRunner {
public:
    // Builds the property and method lists needed to bind this class to a JS
    // object.
    DRTTestRunner(TestShell*);

    virtual ~DRTTestRunner();

    // This function sets a flag that tells the test_shell to print a line of
    // descriptive text for the progress finished callback. It takes no
    // arguments, and ignores any that may be present.
    void dumpProgressFinishedCallback(const CppArgumentList&, CppVariant*);

    // This function sets a flag that tells the test_shell to print out a text
    // representation of the back/forward list. It ignores all arguments.
    void dumpBackForwardList(const CppArgumentList&, CppVariant*);

    // This function sets a flag that tells the test_shell to dump all calls
    // to window.status().
    // It takes no arguments, and ignores any that may be present.
    void dumpWindowStatusChanges(const CppArgumentList&, CppVariant*);

    // Functions for dealing with windows. By default we block all new windows.
    void windowCount(const CppArgumentList&, CppVariant*);
    void setCloseRemainingWindowsWhenComplete(const CppArgumentList&, CppVariant*);

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

    // Changes the cookie policy from the default to allow all cookies.
    void setAlwaysAcceptCookies(const CppArgumentList&, CppVariant*);

    // Gives focus to the window.
    void setWindowIsKey(const CppArgumentList&, CppVariant*);


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

    void dumpSelectionRect(const CppArgumentList&, CppVariant*);

#if ENABLE(NOTIFICATIONS)
    // Grants permission for desktop notifications to an origin
    void grantWebNotificationPermission(const CppArgumentList&, CppVariant*);
    // Simulates a click on a desktop notification.
    void simulateLegacyWebNotificationClick(const CppArgumentList&, CppVariant*);
#endif

    void setDeferMainResourceDataLoad(const CppArgumentList&, CppVariant*);

    void display(const CppArgumentList&, CppVariant*);
    void displayInvalidatedRegion(const CppArgumentList&, CppVariant*);
    void testRepaint(const CppArgumentList&, CppVariant*);
    void repaintSweepHorizontally(const CppArgumentList&, CppVariant*);

    // Clears all databases.
    void clearAllDatabases(const CppArgumentList&, CppVariant*);
    // Sets the default quota for all origins
    void setDatabaseQuota(const CppArgumentList&, CppVariant*);

    // Calls setlocale(LC_ALL, ...) for a specified locale.
    // Resets between tests.
    void setPOSIXLocale(const CppArgumentList&, CppVariant*);

    // Causes layout to happen as if targetted to printed pages.
    void setPrinting(const CppArgumentList&, CppVariant*);

    // Gets the number of geolocation permissions requests pending.
    void numberOfPendingGeolocationPermissionRequests(const CppArgumentList&, CppVariant*);

    // DeviceOrientation related functions
    void setMockDeviceOrientation(const CppArgumentList&, CppVariant*);

    // Geolocation related functions.
    void setGeolocationPermission(const CppArgumentList&, CppVariant*);
    void setMockGeolocationPosition(const CppArgumentList&, CppVariant*);
    void setMockGeolocationPositionUnavailableError(const CppArgumentList&, CppVariant*);

    // Speech input related functions.
#if ENABLE(INPUT_SPEECH)
    void addMockSpeechInputResult(const CppArgumentList&, CppVariant*);
    void setMockSpeechInputDumpRect(const CppArgumentList&, CppVariant*);
#endif
#if ENABLE(SCRIPTED_SPEECH)
    void addMockSpeechRecognitionResult(const CppArgumentList&, CppVariant*);
    void setMockSpeechRecognitionError(const CppArgumentList&, CppVariant*);
    void wasMockSpeechRecognitionAborted(const CppArgumentList&, CppVariant*);
#endif

    void setShouldStayOnPageAfterHandlingBeforeUnload(const CppArgumentList&, CppVariant*);

#if ENABLE(POINTER_LOCK)
    void didAcquirePointerLock(const CppArgumentList&, CppVariant*);
    void didNotAcquirePointerLock(const CppArgumentList&, CppVariant*);
    void didLosePointerLock(const CppArgumentList&, CppVariant*);
    void setPointerLockWillFailSynchronously(const CppArgumentList&, CppVariant*);
    void setPointerLockWillRespondAsynchronously(const CppArgumentList&, CppVariant*);
#endif

    // Used to set the device scale factor.
    void setBackingScaleFactor(const CppArgumentList&, CppVariant*);

public:
    // The following methods are not exposed to JavaScript.
    void setWorkQueueFrozen(bool frozen) { m_workQueue.setFrozen(frozen); }

    bool shouldDumpProgressFinishedCallback() { return m_dumpProgressFinishedCallback; }
    void setShouldDumpProgressFinishedCallback(bool value) { m_dumpProgressFinishedCallback = value; }
    bool shouldDumpStatusCallbacks() { return m_dumpWindowStatusChanges; }
    bool shouldDumpSelectionRect() { return m_dumpSelectionRect; }
    bool shouldDumpBackForwardList() { return m_dumpBackForwardList; }
    bool deferMainResourceDataLoad() { return m_deferMainResourceDataLoad; }
    void setShowDebugLayerTree(bool value) { m_showDebugLayerTree = value; }
    void setTitleTextDirection(WebKit::WebTextDirection dir)
    {
        m_titleTextDirection.set(dir == WebKit::WebTextDirectionLeftToRight ? "ltr" : "rtl");
    }

    bool shouldInterceptPostMessage()
    {
        return m_interceptPostMessage.isBool() && m_interceptPostMessage.toBoolean();
    }

    void setIsPrinting(bool value) { m_isPrinting = value; }
    bool isPrinting() { return m_isPrinting; }

    bool testRepaint() const { return m_testRepaint; }
    bool sweepHorizontally() const { return m_sweepHorizontally; }

    // Called by the webview delegate when the toplevel frame load is done.
    void locationChangeDone();

    // Called by the webview delegate when the policy delegate runs if the
    // waitForPolicyDelegate was called.
    void policyDelegateDone();

    // Reinitializes all static values. The reset() method should be called
    // before the start of each test (currently from TestShell::runFileTest).
    void reset();

    // A single item in the work queue.
    class WorkItem {
    public:
        virtual ~WorkItem() { }

        // Returns true if this started a load.
        virtual bool run(TestShell*) = 0;
    };

    WebTaskList* taskList() { return &m_taskList; }

    bool shouldStayOnPageAfterHandlingBeforeUnload() const { return m_shouldStayOnPageAfterHandlingBeforeUnload; }

private:
    friend class WorkItem;
    friend class WorkQueue;

    // Helper class for managing events queued by methods like queueLoad or
    // queueScript.
    class WorkQueue {
    public:
        WorkQueue(DRTTestRunner* controller) : m_frozen(false), m_controller(controller) { }
        virtual ~WorkQueue();
        void processWorkSoon();

        // Reset the state of the class between tests.
        void reset();

        void addWork(WorkItem*);

        void setFrozen(bool frozen) { m_frozen = frozen; }
        bool isEmpty() { return m_queue.isEmpty(); }
        WebTaskList* taskList() { return &m_taskList; }

    private:
        void processWork();
        class WorkQueueTask: public WebMethodTask<WorkQueue> {
        public:
            WorkQueueTask(WorkQueue* object): WebMethodTask<WorkQueue>(object) { }
            virtual void runIfValid() { m_object->processWork(); }
        };

        WebTaskList m_taskList;
        Deque<WorkItem*> m_queue;
        bool m_frozen;
        DRTTestRunner* m_controller;
    };
    void completeNotifyDone(bool isTimeout);
    class NotifyDoneTimedOutTask: public WebMethodTask<DRTTestRunner> {
    public:
        NotifyDoneTimedOutTask(DRTTestRunner* object): WebMethodTask<DRTTestRunner>(object) { }
        virtual void runIfValid() { m_object->completeNotifyDone(true); }
    };

    // Used for test timeouts.
    WebTaskList m_taskList;

    // Non-owning pointer. The DRTTestRunner is owned by the host.
    TestShell* m_shell;

    // If true, the test_shell will draw the bounds of the current selection rect
    // taking possible transforms of the selection rect into account.
    bool m_dumpSelectionRect;

    // If true, the test_shell will output a descriptive line for the progress
    // finished callback.
    bool m_dumpProgressFinishedCallback;

    // If true, the test_shell will produce a dump of the back forward list as
    // well.
    bool m_dumpBackForwardList;

    // If true, the test_shell will dump all changes to window.status.
    bool m_dumpWindowStatusChanges;

    // When reset is called, go through and close all but the main test shell
    // window. By default, set to true but toggled to false using
    // setCloseRemainingWindowsWhenComplete().
    bool m_closeRemainingWindows;

    // If true, pixel dump will be produced as a series of 1px-tall, view-wide
    // individual paints over the height of the view.
    bool m_testRepaint;
    // If true and test_repaint_ is true as well, pixel dump will be produced as
    // a series of 1px-wide, view-tall paints across the width of the view.
    bool m_sweepHorizontally;

    // If true, don't dump output until notifyDone is called.
    bool m_waitUntilDone;

    // If false, all new requests will not defer the main resource data load.
    bool m_deferMainResourceDataLoad;

    // If true, we will show extended information in the graphics layer tree.
    bool m_showDebugLayerTree;

    // If true, layout is to target printed pages.
    bool m_isPrinting;

    WorkQueue m_workQueue;

    // Bound variable counting the number of top URLs visited.
    CppVariant m_webHistoryItemCount;

    // Bound variable tracking the directionality of the <title> tag.
    CppVariant m_titleTextDirection;

    // Bound variable to set whether postMessages should be intercepted or not
    CppVariant m_interceptPostMessage;

    bool m_shouldStayOnPageAfterHandlingBeforeUnload;
};

#endif // DRTTestRunner_h
