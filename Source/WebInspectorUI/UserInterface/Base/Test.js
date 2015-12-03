/*
 * Copyright (C) 2013, 2014 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

WebInspector.DebuggableType = {
    Web: "web",
    JavaScript: "javascript"
};

WebInspector.loaded = function()
{
    this.debuggableType = WebInspector.DebuggableType.Web;
    this.hasExtraDomains = false;

    // Register observers for events from the InspectorBackend.
    // The initialization order should match the same in Main.js.
    InspectorBackend.registerInspectorDispatcher(new WebInspector.InspectorObserver);
    InspectorBackend.registerPageDispatcher(new WebInspector.PageObserver);
    InspectorBackend.registerConsoleDispatcher(new WebInspector.ConsoleObserver);
    InspectorBackend.registerDOMDispatcher(new WebInspector.DOMObserver);
    InspectorBackend.registerNetworkDispatcher(new WebInspector.NetworkObserver);
    InspectorBackend.registerDebuggerDispatcher(new WebInspector.DebuggerObserver);
    InspectorBackend.registerTimelineDispatcher(new WebInspector.TimelineObserver);
    InspectorBackend.registerCSSDispatcher(new WebInspector.CSSObserver);
    InspectorBackend.registerRuntimeDispatcher(new WebInspector.RuntimeObserver);
    if (InspectorBackend.registerReplayDispatcher)
        InspectorBackend.registerReplayDispatcher(new WebInspector.ReplayObserver);

    // Instantiate controllers used by tests.
    this.frameResourceManager = new WebInspector.FrameResourceManager;
    this.domTreeManager = new WebInspector.DOMTreeManager;
    this.cssStyleManager = new WebInspector.CSSStyleManager;
    this.logManager = new WebInspector.LogManager;
    this.issueManager = new WebInspector.IssueManager;
    this.runtimeManager = new WebInspector.RuntimeManager;
    this.timelineManager = new WebInspector.TimelineManager;
    this.debuggerManager = new WebInspector.DebuggerManager;
    this.probeManager = new WebInspector.ProbeManager;
    this.replayManager = new WebInspector.ReplayManager;

    // Global controllers.
    this.quickConsole = {executionContextIdentifier: undefined};

    document.addEventListener("DOMContentLoaded", this.contentLoaded.bind(this));

    // Enable agents.
    InspectorAgent.enable();
    ConsoleAgent.enable();

    // Perform one-time tasks.
    WebInspector.CSSCompletions.requestCSSCompletions();

    // Global settings.
    this.showShadowDOMSetting = new WebInspector.Setting("show-shadow-dom", true);
}

WebInspector.contentLoaded = function()
{
    // Signal that the frontend is now ready to receive messages.
    InspectorFrontendAPI.loadCompleted();

    // Tell the InspectorFrontendHost we loaded, which causes the window to display
    // and pending InspectorFrontendAPI commands to be sent.
    InspectorFrontendHost.loaded();
}

WebInspector.UIString = function(string)
{
    return string;
}

// Add stubs that are called by the frontend API.
WebInspector.updateDockedState = function() {};
WebInspector.updateDockingAvailability = function() {};

// InspectorTest contains extra methods that are only available to test code running
// in the Web Inspector page. They rely on equivalents in the actual test page
// which are provided by `inspector-test.js`.
InspectorTest = {};

// This is useful for debugging Inspector tests by synchronously logging messages.
InspectorTest.dumpMessagesToConsole = false;

// This is a workaround for the fact that it would be hard to set up a constructor,
// prototype, and prototype chain for the singleton InspectorTest.
InspectorTest.EventDispatcher = class EventDispatcher extends WebInspector.Object
{
    dispatchEvent(event)
    {
        this.dispatchEventToListeners(event);
    }
};

InspectorTest.EventDispatcher.Event = {
    TestPageDidLoad: "inspector-test-test-page-did-load"
};

InspectorTest.eventDispatcher = new InspectorTest.EventDispatcher;

// Note: Additional InspectorTest methods are included on a per-test basis from
// files like `debugger-test.js`.

// Appends a log message in the test document.
InspectorTest.log = function(message)
{
    var stringifiedMessage = typeof message !== "object" ? message : JSON.stringify(message);
    InspectorTest.addResult(stringifiedMessage);
}

// Appends a message in the test document only if the condition is false.
InspectorTest.assert = function(condition, message)
{
    if (condition)
        return;

    var stringifiedMessage = typeof message !== "object" ? message : JSON.stringify(message);
    InspectorTest.addResult("ASSERT: " + stringifiedMessage);
}

// Appends a message in the test document whether the condition is true or not.
InspectorTest.expectThat = function(condition, message)
{
    var prefix = condition ? "PASS" : "FAIL";
    var stringifiedMessage = typeof message !== "object" ? message : JSON.stringify(message);
    InspectorTest.addResult(prefix + ": " + stringifiedMessage);
}

// This function should only be used to debug tests and not to produce normal test output.
InspectorTest.debugLog = function(message)
{
    if (InspectorTest.dumpMessagesToConsole)
        InspectorFrontendHost.unbufferedLog("debugLog: " + message);

    this.evaluateInPage("InspectorTestProxy.debugLog(unescape('" + escape(JSON.stringify(message)) + "'))");
}

// Appends a message in the test document if there was an error, and attempts to complete the test.
InspectorTest.expectNoError = function(error)
{
    if (error) {
        InspectorTest.log("PROTOCOL ERROR: " + error);
        InspectorTest.completeTest();
        throw "PROTOCOL ERROR";
    }
}

InspectorTest.completeTest = function()
{
    if (InspectorTest.dumpMessagesToConsole)
        InspectorFrontendHost.unbufferedLog("InspectorTest.completeTest()");

    function signalCompletionToTestPage() {
        InspectorBackend.runAfterPendingDispatches(this.evaluateInPage.bind(this, "InspectorTestProxy.completeTest()"));
    }

    // Wait for results to be resent before requesting completeTest(). Otherwise, messages will be
    // queued after pending dispatches run to zero and the test page will quit before processing them.
    if (this._shouldResendResults)
        this._resendResults(signalCompletionToTestPage.bind(this));
    else
        signalCompletionToTestPage.call(this);
}

InspectorTest.evaluateInPage = function(codeString, callback)
{
    // If we load this page outside of the inspector, or hit an early error when loading
    // the test frontend, then defer evaluating the commands (indefinitely in the former case).
    if (!window.RuntimeAgent) {
        this._originalConsoleMethods["error"]("Tried to evaluate in test page, but connection not yet established:", codeString);
        return;
    }

    RuntimeAgent.evaluate.invoke({expression: codeString, objectGroup: "test", includeCommandLineAPI: false}, callback);
}

InspectorTest.addResult = function(text)
{
    this._results.push(text);

    if (InspectorTest.dumpMessagesToConsole)
        InspectorFrontendHost.unbufferedLog("addResult: " + text);

    if (!this._testPageIsReloading)
        this.evaluateInPage("InspectorTestProxy.addResult(unescape('" + escape(text) + "'))");
}

InspectorTest._resendResults = function(callback)
{
    console.assert(this._shouldResendResults);
    delete this._shouldResendResults;

    var pendingResponseCount = 1 + this._results.length;
    function decrementPendingResponseCount() {
        pendingResponseCount--;
        if (!pendingResponseCount && typeof callback === "function")
            callback();
    }

    this.evaluateInPage("InspectorTestProxy.clearResults()", decrementPendingResponseCount);
    for (var result of this._results)
        this.evaluateInPage("InspectorTestProxy.addResult(unescape('" + escape(result) + "'))", decrementPendingResponseCount);
}

InspectorTest.testPageDidLoad = function()
{
    this._testPageIsReloading = false;
    this._resendResults();

    this.eventDispatcher.dispatchEvent(InspectorTest.EventDispatcher.Event.TestPageDidLoad);
}

InspectorTest.reloadPage = function(shouldIgnoreCache)
{
    console.assert(!this._testPageIsReloading);
    console.assert(!this._testPageReloadedOnce);

    this._testPageIsReloading = true;

    return PageAgent.reload(!!shouldIgnoreCache)
        .then(function() {
            this._shouldResendResults = true;
            this._testPageReloadedOnce = true;

            return Promise.resolve(null);
        }.bind(this));
}

InspectorTest.reportUncaughtException = function(message, url, lineNumber)
{
    var result = "Uncaught exception in inspector page: " + message + " [" + url + ":" + lineNumber + "]";

    // If the connection to the test page is not set up, then just dump to console and give up.
    // Errors encountered this early can be debugged by loading Test.html in a normal browser page.
    if (!InspectorFrontendHost || !InspectorBackend) {
        this._originalConsoleMethods["error"](result);
        return false;
    }

    this.addResult(result);
    this.completeTest();
    // Stop default handler so we can empty InspectorBackend's message queue.
    return true;
}

// Initialize reporting mechanisms before loading the rest of the inspector page.
InspectorTest._results = [];
InspectorTest._shouldResendResults = true;
InspectorTest._originalConsoleMethods = {};

// Catch syntax errors, type errors, and other exceptions.
window.onerror = InspectorTest.reportUncaughtException.bind(InspectorTest);

// Redirect frontend console methods to log messages into the test result.
(function() {
    function createProxyConsoleHandler(type) {
        return function() {
            InspectorTest.addResult(type + ": " + Array.from(arguments).join(" "));
        };
    }

    for (var type of ["log", "error", "info"]) {
        InspectorTest._originalConsoleMethods[type] = console[type].bind(console);
        console[type] = createProxyConsoleHandler(type.toUpperCase());
    }
})();
