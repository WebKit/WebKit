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

#if ENABLE(NOTIFICATIONS)
    // Grants permission for desktop notifications to an origin
    void grantWebNotificationPermission(const CppArgumentList&, CppVariant*);
    // Simulates a click on a desktop notification.
    void simulateLegacyWebNotificationClick(const CppArgumentList&, CppVariant*);
#endif

    void display(const CppArgumentList&, CppVariant*);
    void displayInvalidatedRegion(const CppArgumentList&, CppVariant*);

    // Gets the number of geolocation permissions requests pending.
    void numberOfPendingGeolocationPermissionRequests(const CppArgumentList&, CppVariant*);

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

public:
    // The following methods are not exposed to JavaScript.
    void setWorkQueueFrozen(bool frozen) { m_workQueue.setFrozen(frozen); }

    void setShowDebugLayerTree(bool value) { m_showDebugLayerTree = value; }

    bool shouldInterceptPostMessage()
    {
        return m_interceptPostMessage.isBool() && m_interceptPostMessage.toBoolean();
    }

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

    // Non-owning pointer. The DRTTestRunner is owned by the host.
    TestShell* m_shell;

    // When reset is called, go through and close all but the main test shell
    // window. By default, set to true but toggled to false using
    // setCloseRemainingWindowsWhenComplete().
    bool m_closeRemainingWindows;

    // If true, don't dump output until notifyDone is called.
    bool m_waitUntilDone;

    // If true, we will show extended information in the graphics layer tree.
    bool m_showDebugLayerTree;

    WorkQueue m_workQueue;

    // Bound variable counting the number of top URLs visited.
    CppVariant m_webHistoryItemCount;

    // Bound variable to set whether postMessages should be intercepted or not
    CppVariant m_interceptPostMessage;
};

#endif // DRTTestRunner_h
