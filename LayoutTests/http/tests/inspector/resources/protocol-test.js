/*
 * Copyright (C) 2012 Samsung Electronics. All rights reserved.
 * Copyright (C) 2015 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// This namespace is injected into every test page. Its functions are invoked by
// ProtocolTest methods on the inspector page via a TestHarness subclass.
TestPage = {};
TestPage._initializers = [];

// Helper scripts like `console-test.js` must register their initialization
// function with this method so it will be marshalled to the inspector page.
TestPage.registerInitializer = function(initializer)
{
    if (typeof initializer === "function")
        this._initializers.push(initializer.toString());
};

let outputElement;

TestPage.debugLog = window.debugLog = function(text)
{
    alert(text);
}

TestPage.log = window.log = function(text)
{
    if (!outputElement) {
        let intermediate = document.createElement("div");
        document.body.appendChild(intermediate);

        let intermediate2 = document.createElement("div");
        intermediate.appendChild(intermediate2);

        outputElement = document.createElement("div");
        outputElement.className = "output";
        outputElement.id = "output";
        outputElement.style.whiteSpace = "pre";
        intermediate2.appendChild(outputElement);
    }
    outputElement.appendChild(document.createTextNode(text));
    outputElement.appendChild(document.createElement("br"));
};

TestPage.closeTest = window.closeTest = function()
{
    window.internals.closeDummyInspectorFrontend();

    // This code might be executed while the debugger is still running through a stack based EventLoop.
    // Use a setTimeout to defer to a clean stack before letting the testRunner load the next test.
    setTimeout(() => { testRunner.notifyDone(); }, 0);
};

TestPage.runTest = window.runTest = function()
{
    if (!window.testRunner) {
        console.error("This test must be run via DumpRenderTree or WebKitTestRunner.");
        return;
    }

    testRunner.dumpAsText();
    testRunner.waitUntilDone();
    testRunner.setCanOpenWindows(true);

    let testFunction = window.test;
    if (typeof testFunction !== "function") {
        alert("Failed to send test() because it is not a function.");
        testRunner.notifyDone();
        return;
    }

    let url = testRunner.inspectorTestStubURL;
    if (!url) {
        alert("Failed to obtain inspector test stub URL.");
        testRunner.notifyDone();
        return;
    }

    function runInitializationMethodsInFrontend(initializers)
    {
        for (let initializer of initializers) {
            try {
                initializer();
            } catch (e) {
                ProtocolTest.log("Exception in test initialization: " + e, e.stack || "(no stack trace)");
                ProtocolTest.completeTest();
            }
        }
    }

    function runTestMethodInFrontend(testFunction)
    {
        try {
            testFunction();
        } catch (e) {
            ProtocolTest.log("Exception during test execution: " + e, e.stack || "(no stack trace)");
            ProtocolTest.completeTest();
        }
    }

    TestPage.inspectorFrontend = window.internals.openDummyInspectorFrontend(url);
    TestPage.inspectorFrontend.addEventListener("load", (event) => {
        let initializationCodeString = `(${runInitializationMethodsInFrontend.toString()})([${TestPage._initializers}]);`;
        let testFunctionCodeString = `(${runTestMethodInFrontend.toString()})(${testFunction.toString()});`;

        TestPage.inspectorFrontend.postMessage(initializationCodeString, "*");
        TestPage.inspectorFrontend.postMessage(testFunctionCodeString, "*");
    });
};

TestPage.dispatchEventToFrontend = function(eventName, data)
{
    let dispatchEventCodeString = `ProtocolTest.dispatchEventToListeners(${JSON.stringify(eventName)}, ${JSON.stringify(data)});`;
    TestPage.inspectorFrontend.postMessage(dispatchEventCodeString, "*");
};
