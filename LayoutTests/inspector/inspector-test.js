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

// This namespace is injected into every test page. Its functions are invoked by
// InspectorTest methods on the inspector page via RuntimeAgent.evaluate() calls.
InspectorTestProxy = {};
InspectorTestProxy._initializers = [];

// Helper scripts like `debugger-test.js` must register their initialization
// function with this method so it will be marshalled to the inspector page.
InspectorTestProxy.register = function(initializer)
{
    if (typeof initializer === "function")
        this._initializers.push(initializer.toString());
}

// This function is called by the test document's body onload handler.

// It initializes the inspector and loads any `*-test.js` helper scripts
// into the inspector page context.
function runTest()
{
    // Set up the test page before the load event fires.
    testRunner.dumpAsText();
    testRunner.waitUntilDone();

    if (window.internals)
        internals.setInspectorIsUnderTest(true);
    testRunner.showWebInspector();

    // This function runs the initializer functions in the Inspector page.
    function initializeFrontend(initializersArray)
    {
        for (var initializer of initializersArray) {
            try {
                initializer();
            } catch (e) {
                console.error("Exception in test initialization: " + e, e.stack || "(no stack trace)");
                InspectorTest.completeTest();
            }
        }
    }

    // This function runs the test() method in the Inspector page.
    function runTestInFrontend(testFunction)
    {
        try {
            testFunction();
        } catch (e) {
            console.error("Exception during test execution: " + e, e.stack || "(no stack trace)");
            InspectorTest.completeTest();
        }
    }

    var codeStringToEvaluate = "(" + initializeFrontend.toString() + ")([" + InspectorTestProxy._initializers + "]);";
    testRunner.evaluateInWebInspector(0, codeStringToEvaluate);

    // `test` refers to a function defined in global scope in the test HTML page.
    codeStringToEvaluate = "(" + runTestInFrontend.toString() + ")(" + test.toString() + ");";
    testRunner.evaluateInWebInspector(0, codeStringToEvaluate);
}

InspectorTestProxy.completeTest = function()
{
    // Close inspector asynchrously in case we want to test tear-down behavior.
    setTimeout(function() {
        testRunner.closeWebInspector();
        setTimeout(function() {
            testRunner.notifyDone();
        }, 0);
    }, 0);
}

// Logs message to unbuffered process stdout, avoiding timeouts.
// only be used to debug tests and not to produce normal test output.
InspectorTestProxy.debugLog = function(message)
{
    window.alert(message);
}

// Add and clear test output from the results window.
InspectorTestProxy.addResult = function(text)
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
    this._resultElement.appendChild(document.createTextNode(text));
    this._resultElement.appendChild(document.createElement("br"));
}

InspectorTestProxy.clearResults = function()
{
    if (this._resultElement) {
        this._resultElement.parentNode.removeChild(this._resultElement);
        delete this._resultElement;
    }
}

InspectorTestProxy.reportUncaughtException = function(message, url, lineNumber)
{
    var result = "Uncaught exception in test page: " + message + " [" + url + ":" + lineNumber + "]";
    InspectorTestProxy.addResult(result);
    InspectorTestProxy.completeTest();
}

// Catch syntax errors, type errors, and other exceptions. Run this before loading other files.
window.onerror = InspectorTestProxy.reportUncaughtException;
