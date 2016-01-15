/*
 * Copyright (C) 2013-2015 Apple Inc. All rights reserved.
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

// This namespace is injected into every test page. Its functions are invoked by
// InspectorTest methods on the inspector page via a TestHarness subclass.
TestPage = {};
TestPage._initializers = [];

// Helper scripts like `debugger-test.js` must register their initialization
// function with this method so it will be marshalled to the inspector page.
TestPage.registerInitializer = function(initializer)
{
    if (typeof initializer === "function")
        this._initializers.push(initializer.toString());
}

// This function is called by the test document's body onload handler.

// It initializes the inspector and loads any `*-test.js` helper scripts
// into the inspector page context.
function runTest()
{
    // Don't try to use testRunner if running through the browser.
    if (!window.testRunner)
        return;

    // Set up the test page before the load event fires.
    testRunner.dumpAsText();
    testRunner.waitUntilDone();

    window.internals.setInspectorIsUnderTest(true);
    testRunner.showWebInspector();

    let testFunction = window.test;
    if (typeof testFunction !== "function") {
        alert("Failed to send test() because it is not a function.");
        testRunner.notifyDone();
    }

    function runInitializationMethodsInFrontend(initializersArray)
    {
        InspectorTest.testPageDidLoad();

        // If the test page reloaded but we started running the test in a previous
        // navigation, then don't initialize the inspector frontend again.
        if (InspectorTest.didInjectTestCode)
            return;

        for (let initializer of initializersArray) {
            try {
                initializer();
            } catch (e) {
                console.error("Exception in test initialization: " + e, e.stack || "(no stack trace)");
                InspectorTest.completeTest();
            }
        }
    }

    function runTestMethodInFrontend(testFunction)
    {
        if (InspectorTest.didInjectTestCode)
            return;

        InspectorTest.didInjectTestCode = true;

        try {
            testFunction();
        } catch (e) {
            // FIXME: the redirected console methods do not forward additional arguments.
            console.error("Exception during test execution: " + e, e.stack || "(no stack trace)");
            InspectorTest.completeTest();
        }
    }

    let initializationCodeString = `(${runInitializationMethodsInFrontend.toString()})([${TestPage._initializers}]);`;
    let testFunctionCodeString = `(${runTestMethodInFrontend.toString()})(${testFunction.toString()});`;

    testRunner.evaluateInWebInspector(initializationCodeString);
    testRunner.evaluateInWebInspector(testFunctionCodeString);
}

TestPage.completeTest = function()
{
    // Don't try to use testRunner if running through the browser.
    if (!window.testRunner)
        return;

    // Close inspector asynchrously in case we want to test tear-down behavior.
    setTimeout(() => {
        testRunner.closeWebInspector();
        setTimeout(() => { testRunner.notifyDone(); }, 0);
    }, 0);
}

// Logs message to unbuffered process stdout, avoiding timeouts.
// only be used to debug tests and not to produce normal test output.
TestPage.debugLog = function(message)
{
    window.alert(message);
}

// Add and clear test output from the results window.
TestPage.addResult = function(text)
{
    // For early errors triggered when loading the test page, write to stderr.
    if (!document.body) {
        this.debugLog(text);
        this.completeTest();
    }

    if (!this._resultElement) {
        this._resultElement = document.createElement("pre");
        this._resultElement.id = "output";
        document.body.appendChild(this._resultElement);
    }

    this._resultElement.append(text, document.createElement("br"));
}

TestPage.dispatchEventToFrontend = function(eventName, data)
{
    let dispatchEventCodeString = `InspectorTest.dispatchEventToListeners(${JSON.stringify(eventName)}, ${JSON.stringify(data)});`;
    testRunner.evaluateInWebInspector(dispatchEventCodeString);
};

TestPage.allowUncaughtExceptions = false;
TestPage.needToSanitizeUncaughtExceptionURLs = false;

TestPage.reportUncaughtException = function(message, url, lineNumber)
{
    if (TestPage.needToSanitizeUncaughtExceptionURLs) {
        if (typeof url === "string") {
            let lastSlash = url.lastIndexOf("/");
            let lastBackSlash = url.lastIndexOf("\\");
            let lastPathSeparator = Math.max(lastSlash, lastBackSlash);
            if (lastPathSeparator > 0)
                url = url.substr(lastPathSeparator + 1);
        }
    }

    let result = `Uncaught exception in test page: ${message} [${url}:${lineNumber}]`;
    TestPage.addResult(result);

    if (!TestPage.allowUncaughtExceptions)
        TestPage.completeTest();
}

// Catch syntax errors, type errors, and other exceptions. Run this before loading other files.
window.onerror = TestPage.reportUncaughtException.bind(TestPage);
