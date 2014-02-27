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
    InspectorBackend.registerDebuggerDispatcher(new WebInspector.DebuggerObserver);
    InspectorBackend.registerCSSDispatcher(new WebInspector.CSSObserver);
    InspectorBackend.registerRuntimeDispatcher(new WebInspector.RuntimeObserver);

    // Instantiate controllers used by tests.
    this.frameResourceManager = new WebInspector.FrameResourceManager;
    this.domTreeManager = new WebInspector.DOMTreeManager;
    this.cssStyleManager = new WebInspector.CSSStyleManager;
    this.runtimeManager = new WebInspector.RuntimeManager;
    this.debuggerManager = new WebInspector.DebuggerManager;
    this.probeManager = new WebInspector.ProbeManager;

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

// Note: Additional InspectorTest methods are included on a per-test basis from
// files like `debugger-test.js`.

// Appends a log message in the test document.
InspectorTest.log = function(message)
{
    this.evaluateInPage("InspectorTestProxy.addResult(" + JSON.stringify(message) + ")");
}

// Appends an assert message in the test document.
InspectorTest.assert = function(condition, message)
{
    var status = condition ? "PASS" : "FAIL";
    this.evaluateInPage("InspectorTestProxy.addResult(" + JSON.stringify(status + ": " + message) + ")");
}

// This function should only be used to debug tests and not to produce normal test output.
InspectorTest.debugLog = function(message)
{
    this.evaluateInPage("InspectorTestProxy.debugLog(" + JSON.stringify(message) + ")");
}

InspectorTest.completeTest = function()
{
    InspectorBackend.runAfterPendingDispatches(this.evaluateInPage.bind(this, "InspectorTestProxy.completeTest()"));
}

InspectorTest.evaluateInPage = function(codeString)
{
    // If we load this page outside of the inspector, or hit an early error when loading
    // the test frontend, then defer evaluating the commands (indefinitely in the former case).
    if (!RuntimeAgent) {
        InspectorTest._originalConsoleMethods["error"]("Tried to evaluate in test page, but connection not yet established:", codeString);
        return;
    }

    RuntimeAgent.evaluate(codeString, "test", false);
}

InspectorTest.addResult = function(text)
{
    this._results.push(text);
    // If the test page reloaded, then we need to re-add the results from before the navigation.
    if (this._shouldRebuildResults) {
        delete this._shouldRebuildResults;

        this.clearResults();
        for (var result of this._results)
            InspectorTest.evaluateInPage("InspectorTestProxy.addResult(unescape('" + escape(text) + "'))");
    }

    InspectorTest.evaluateInPage("InspectorTestProxy.addResult(unescape('" + escape(text) + "'))");
}

InspectorTest.clearResults = function(text)
{
    InspectorTest.evaluateInPage("InspectorTestProxy.clearResults()");
}

InspectorTest.pageLoaded = function()
{
    InspectorTest._shouldRebuildResults = true;
    InspectorTest.addResult("Page reloaded.");
}

InspectorTest.reportUncaughtException = function(message, url, lineNumber)
{
    var result = "Uncaught exception in inspector page: " + message + " [" + url + ":" + lineNumber + "]";

    // If the connection to the test page is not set up, then just dump to console and give up.
    // Errors encountered this early can be debugged by loading Test.html in a normal browser page.
    if (!InspectorFrontendHost || !InspectorBackend) {
        InspectorTest._originalConsoleMethods["error"](result);
        return;
    }

    InspectorTest.addResult(result);
    InspectorTest.completeTest();
}

// Initialize reporting mechanisms before loading the rest of the inspector page.
InspectorTest._results = [];
InspectorTest._shouldRebuildResults = true;
InspectorTest._originalConsoleMethods = {};

// Catch syntax errors, type errors, and other exceptions. Run this before loading other files.
window.onerror = InspectorTest.reportUncaughtException;

for (var logType of ["log", "error", "info"]) {
    // Redirect console methods to log messages into the test page's DOM.
    InspectorTest._originalConsoleMethods[logType] = console[logType].bind(console);
    console[logType] = function() {
        InspectorTest.addResult(logType.toUpperCase() + ": " + Array.prototype.slice.call(arguments).toString());
    };
}
