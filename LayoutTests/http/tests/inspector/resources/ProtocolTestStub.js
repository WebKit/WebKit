/*
 * Copyright (C) 2012 Samsung Electronics. All rights reserved.
 * Copyright (C) 2014, 2015 Apple Inc. All rights reserved.
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
InspectorFrontendAPI = {};

InspectorTest = {};
InspectorTest._dispatchTable = [];
InspectorTest._requestId = -1;
InspectorTest.eventHandler = {};
InspectorTest.logCount = 0;

InspectorTest.dumpInspectorProtocolMessages = false;
InspectorTest.forceSyncDebugLogging = false;

InspectorTest.sendCommand = function(methodOrObject, params, handler)
{
    // Allow new-style arguments object, as in awaitCommand.
    var method = methodOrObject;
    if (typeof methodOrObject === "object")
        var {method, params, handler} = methodOrObject;

    this._dispatchTable[++this._requestId] = handler;
    var messageObject = {method, params, "id": this._requestId};
    this.sendMessage(messageObject);

    return this._requestId;
}

InspectorTest.awaitCommand = function(args)
{
    var {method, params} = args;
    return new Promise(function(resolve, reject) {
        this._dispatchTable[++this._requestId] = {resolve, reject};
        var messageObject = {method, params, "id": this._requestId};
        this.sendMessage(messageObject);
    }.bind(this));
}

InspectorTest.awaitEvent = function(args)
{
    var {event} = args;
    if (typeof event !== "string")
        throw new Error("Event must be a string.");

    return new Promise(function(resolve, reject) {
        InspectorTest.eventHandler[event] = function(message) {
            InspectorTest.eventHandler[event] = undefined;
            resolve(message);
        }
    });
}

InspectorTest.addEventListener = function(eventTypeOrObject, listener)
{
    var event = eventTypeOrObject;
    if (typeof eventTypeOrObject === "object")
        var {event, listener} = eventTypeOrObject;

    if (typeof event !== "string")
        throw new Error("Event name must be a string.");

    if (typeof listener !== "function")
        throw new Error("Event listener must be callable.");

    // Convert to an array of listeners.
    var listeners = InspectorTest.eventHandler[event];
    if (!listeners)
        listeners = InspectorTest.eventHandler[event] = [];
    else if (typeof listeners === "function")
        listeners = InspectorTest.eventHandler[event] = [listeners];

    // Prevent registering multiple times.
    if (listeners.includes(listener))
        throw new Error("Cannot register the same listener more than once.");

    listeners.push(listener);
}

InspectorTest.sendMessage = function(messageObject)
{
    // This matches the debug dumping in InspectorBackend, which is bypassed
    // by InspectorTest. Return messages should be dumped by InspectorBackend.
    if (this.dumpInspectorProtocolMessages)
        console.log("frontend: " + JSON.stringify(messageObject));

    InspectorFrontendHost.sendMessageToBackend(JSON.stringify(messageObject));
}

InspectorFrontendAPI.dispatchMessageAsync = function(messageObject)
{
    // If the message has an id, then it is a reply to a command.
    var messageId = messageObject["id"];
    if (typeof messageId === "number") {
        var handler = InspectorTest._dispatchTable[messageId];
        if (!handler)
            return;

        if (typeof handler === "function")
            handler(messageObject);
        else if (typeof handler === "object") {
            var {resolve, reject} = handler;
            if ("error" in messageObject)
                reject(messageObject.error.message);
            else
                resolve(messageObject.result);
        }
    // Otherwise, it is an event.
    } else {
        var eventName = messageObject["method"];
        var handler = InspectorTest.eventHandler[eventName];
        if (!handler)
            return;

        if (typeof handler === "function")
            handler(messageObject);
        else if (handler instanceof Array) {
            handler.map(function(listener) {
                listener.call(null, messageObject);
            });
        } else if (typeof handler === "object") {
            var {resolve, reject} = handler;
            if ("error" in messageObject)
                reject(messageObject.error.message);
            else
                resolve(messageObject.result);
        }
    }
}

InspectorTest.AsyncTestSuite = class AsyncTestSuite {
    constructor(name) {
        if (!name || typeof name !== "string")
            throw new Error("Tried to create AsyncTestSuite without string suite name.");

        this.name = name;

        this.testcases = [];
        this.runCount = 0;
        this.failCount = 0;
    }

    get passCount()
    {
        return this.runCount - this.failCount;
    }

    get skipCount()
    {
        if (this.failCount)
            return this.testcases.length - this.runCount;
        else
            return 0;
    }

    addTestCase(testcase)
    {
        if (!testcase || !(testcase instanceof Object))
            throw new Error("Tried to add non-object test case.");

        if (typeof testcase.name !== "string")
            throw new Error("Tried to add test case without a name.");

        if (typeof testcase.test !== "function")
            throw new Error("Tried to add test case without `test` function.");

        this.testcases.push(testcase);
    }

    // Use this if the test file only has one suite, and no handling
    // of the promise returned by runTestCases() is needed.
    runTestCasesAndFinish()
    {
        function finish() {
            InspectorTest.completeTest();
        }

        this.runTestCases()
            .then(finish)
            .catch(finish);
    }

    runTestCases()
    {
        if (!this.testcases.length)
            throw new Error("Tried to call runTestCases() for suite with no test cases");
        if (this._startedRunning)
            throw new Error("Tried to call runTestCases() more than once.");

        this._startedRunning = true;

        InspectorTest.log("");
        InspectorTest.log("== Running test suite: " + this.name);

        // Avoid adding newlines if nothing was logged.
        var priorLogCount = InspectorTest.logCount;
        var suite = this;
        var result = this.testcases.reduce(function(chain, testcase, i) {
            return chain.then(function() {
                if (i > 0 && priorLogCount + 1 < InspectorTest.logCount)
                    InspectorTest.log("");

                priorLogCount = InspectorTest.logCount;
                InspectorTest.log("-- Running test case: " + testcase.name);
                suite.runCount++;
                return new Promise(testcase.test);
            });
        }, Promise.resolve());

        return result.catch(function(e) {
            suite.failCount++;
            var message = e;
            if (e instanceof Error)
                message = e.message;

            if (typeof message !== "string")
                message = JSON.stringify(message);

            InspectorTest.log("!! EXCEPTION: " + message);
            throw e; // Reject this promise by re-throwing the error.
        });
    }
}

InspectorTest.SyncTestSuite = class SyncTestSuite {
    constructor(name) {
        if (!name || typeof name !== "string")
            throw new Error("Tried to create SyncTestSuite without string suite name.");

        this.name = name;

        this.testcases = [];
        this.runCount = 0;
        this.failCount = 0;
    }

    get passCount()
    {
        return this.runCount - this.failCount;
    }

    get skipCount()
    {
        if (this.failCount)
            return this.testcases.length - this.runCount;
        else
            return 0;
    }

    addTestCase(testcase)
    {
        if (!testcase || !(testcase instanceof Object))
            throw new Error("Tried to add non-object test case.");

        if (typeof testcase.name !== "string")
            throw new Error("Tried to add test case without a name.");

        if (typeof testcase.test !== "function")
            throw new Error("Tried to add test case without `test` function.");

        this.testcases.push(testcase);
    }

    // Use this if the test file only has one suite.
    runTestCasesAndFinish()
    {
        this.runTestCases();
        InspectorTest.completeTest();
    }

    runTestCases()
    {
        if (!this.testcases.length)
            throw new Error("Tried to call runTestCases() for suite with no test cases");
        if (this._startedRunning)
            throw new Error("Tried to call runTestCases() more than once.");

        this._startedRunning = true;

        InspectorTest.log("");
        InspectorTest.log("== Running test suite: " + this.name);

        var priorLogCount = InspectorTest.logCount;
        var suite = this;
        for (var i = 0; i < this.testcases.length; i++) {
            var testcase = this.testcases[i];
            if (i > 0 && priorLogCount + 1 < InspectorTest.logCount)
                InspectorTest.log("");

            priorLogCount = InspectorTest.logCount;

            InspectorTest.log("-- Running test case: " + testcase.name);
            suite.runCount++;
            try {
                var result = testcase.test.call(null);
                if (result === false) {
                    suite.failCount++;
                    return false;
                }
            } catch (e) {
                suite.failCount++;
                var message = e;
                if (e instanceof Error)
                    message = e.message;
                else
                    e = new Error(e);

                if (typeof message !== "string")
                    message = JSON.stringify(message);

                InspectorTest.log("!! EXCEPTION: " + message);
                return false;
            }
        }

        return true;
    }
}

// Logs a message to test document.
InspectorTest.log = function(message)
{
    ++InspectorTest.logCount;

    if (this.forceSyncDebugLogging)
        this.debugLog(message);
    else
        this.sendCommand("Runtime.evaluate", { "expression": "log(" + JSON.stringify(message) + ")" } );
}

// Logs an assertion result to the test document.
InspectorTest.assert = function(condition, message)
{
    var status = condition ? "PASS" : "FAIL";
    var message = typeof message !== "string" ? JSON.stringify(message) : message;
    var formattedMessage = status + ": " + message;
    this.log(formattedMessage);
}

// Logs message a directly to stdout of the test process via alert function.
// This message should survive process crash or kill by timeout.
InspectorTest.debugLog = function(message)
{
    this.sendCommand("Runtime.evaluate", { "expression": "debugLog(" + JSON.stringify(message) + ")" } );
}

InspectorTest.completeTest = function()
{
    this.sendCommand("Runtime.evaluate", { "expression": "closeTest();"} );
}

InspectorTest.checkForError = function(responseObject)
{
    if (responseObject.error) {
        InspectorTest.log("PROTOCOL ERROR: " + JSON.stringify(responseObject.error));
        InspectorTest.completeTest();
        throw "PROTOCOL ERROR";
    }
}

InspectorTest.importScript = function(scriptName)
{
    var xhr = new XMLHttpRequest();
    var isAsyncRequest = false;
    xhr.open("GET", scriptName, isAsyncRequest);
    xhr.send(null);
    if (xhr.status !== 0 && xhr.status !== 200)
        throw new Error("Invalid script URL: " + scriptName);
    var script = "try { " + xhr.responseText + "} catch (e) { alert(" + JSON.stringify("Error in: " + scriptName) + "); throw e; }";
    window.eval(script);
}

window.addEventListener("message", function(event) {
    try {
        eval(event.data);
    } catch (e) {
        alert(e.stack);
        InspectorTest.completeTest();
        throw e;
    }
});
