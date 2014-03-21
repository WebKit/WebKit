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

WebInspector.loaded = function()
{
    // Register observers for events from the InspectorBackend.
    // The initialization order should match the same in Main.js.
    InspectorBackend.registerInspectorDispatcher(new WebInspector.InspectorObserver);
    InspectorBackend.registerPageDispatcher(new WebInspector.PageObserver);
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
    this.runtimeManager = new WebInspector.RuntimeManager;
    this.timelineManager = new WebInspector.TimelineManager;
    this.debuggerManager = new WebInspector.DebuggerManager;
    this.probeManager = new WebInspector.ProbeManager;
    this.replayManager = new WebInspector.ReplayManager;

    document.addEventListener("DOMContentLoaded", this.contentLoaded.bind(this));

    // Enable agents.
    InspectorAgent.enable();

    // Establish communication with the InspectorBackend.
    InspectorFrontendHost.loaded();
}

WebInspector.contentLoaded = function() {
    // Signal that the frontend is now ready to receive messages.
    InspectorFrontendAPI.loadCompleted();
}

// Add stubs that are called by the frontend API.
WebInspector.updateDockedState = function()
{
}

// InspectorTest contains extra methods that are only available to test code running
// in the Web Inspector page. They rely on equivalents in the actual test page
// which are provided by `inspector-test.js`.
InspectorTest = {};

// This is a workaround for the fact that it would be hard to set up a constructor,
// prototype, and prototype chain for the singleton InspectorTest.
InspectorTest.EventDispatcher = function()
{
    WebInspector.Object.call(this);
};

InspectorTest.EventDispatcher.Event = {
    TestPageDidLoad: "inspector-test-test-page-did-load"
};

InspectorTest.EventDispatcher.prototype = {
    __proto__: WebInspector.Object.prototype,
    constructor: InspectorTest.EventDispatcher,

    dispatchEvent: function(event)
    {
        this.dispatchEventToListeners(event);
    }
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
    this.evaluateInPage("InspectorTestProxy.debugLog(unescape('" + escape(JSON.stringify(message)) + "'))");
}

InspectorTest.completeTest = function()
{
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
    if (!RuntimeAgent) {
        this._originalConsoleMethods["error"]("Tried to evaluate in test page, but connection not yet established:", codeString);
        return;
    }

    RuntimeAgent.evaluate.invoke({expression: codeString, objectGroup: "test", includeCommandLineAPI: false}, callback);
}

InspectorTest.addResult = function(text)
{
    this._results.push(text);

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
    return PageAgent.reload.promise(!!shouldIgnoreCache)
        .then(function() {
            this._shouldResendResults = true;
            this._testPageIsReloading = true;

            return Promise.resolve(null);
        });
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

for (var logType of ["log", "error", "info"]) {
    // Redirect console methods to log messages into the test page's DOM.
    InspectorTest._originalConsoleMethods[logType] = console[logType].bind(console);
    console[logType] = function() {
        InspectorTest.addResult(logType.toUpperCase() + ": " + Array.prototype.slice.call(arguments).toString());
    };
}
