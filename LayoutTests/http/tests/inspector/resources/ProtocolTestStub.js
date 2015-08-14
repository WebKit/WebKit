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

InjectedTestHarness = class InjectedTestHarness
{
    constructor()
    {
        this._logCount = 0;
        this.forceSyncDebugLogging = false;
    }

    completeTest()
    {
        throw new Error("Must be implemented by subclasses.");
    }

    addResult()
    {
        throw new Error("Must be implemented by subclasses.");
    }

    debugLog()
    {
        throw new Error("Must be implemented by subclasses.");
    }

    evaluateInPage(string, callback)
    {
        throw new Error("Must be implemented by subclasses.");
    }

    createAsyncSuite(name)
    {
        return new InjectedTestHarness.AsyncTestSuite(this, name);
    }

    createSyncSuite(name)
    {
        return new InjectedTestHarness.SyncTestSuite(this, name);
    }

    importScript(scriptName)
    {
        var xhr = new XMLHttpRequest();
        var isAsyncRequest = false;
        xhr.open("GET", scriptName, isAsyncRequest);
        xhr.send(null);
        if (xhr.status !== 0 && xhr.status !== 200)
            throw new Error("Invalid script URL: " + scriptName);
        var script = `// From InjectedTestHarness.importScript(${scriptName})
try {
    ${xhr.responseText}
} catch (e) {
    alert(${JSON.stringify("Error in: " + scriptName)});
    throw e;
}`;
        window.eval(script);
    }

    get logCount()
    {
        return this._logCount;
    }

    log(message)
    {
        ++this._logCount;

        if (this.forceSyncDebugLogging)
            this.debugLog(message);
        else
            this.addResult(message);
    }

    assert(condition, message)
    {
        if (condition)
            return;

        let stringifiedMessage = typeof message !== "object" ? message : JSON.stringify(message);
        this.addResult("ASSERT: " + stringifiedMessage);
    }

    expectThat(condition, message)
    {
        let prefix = condition ? "PASS" : "FAIL";
        let stringifiedMessage = typeof message !== "object" ? message : JSON.stringify(message);
        this.addResult(prefix + ": " + stringifiedMessage);
    }
}

InjectedTestHarness.AsyncTestSuite = class AsyncTestSuite
{
    constructor(harness, name) {
        if (!(harness instanceof InjectedTestHarness))
            throw new Error("Must pass the test's harness as the first argument.");

        if (!name || typeof name !== "string")
            throw new Error("Tried to create AsyncTestSuite without string suite name.");

        this.name = name;
        this._harness = harness;

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
            this._harness.completeTest();
        }

        this.runTestCases()
            .then(finish.bind(this))
            .catch(finish.bind(this));
    }

    runTestCases()
    {
        if (!this.testcases.length)
            throw new Error("Tried to call runTestCases() for suite with no test cases");
        if (this._startedRunning)
            throw new Error("Tried to call runTestCases() more than once.");

        this._startedRunning = true;

        this._harness.log("");
        this._harness.log("== Running test suite: " + this.name);

        // Avoid adding newlines if nothing was logged.
        var priorLogCount = this._harness.logCount;
        var suite = this;
        var result = this.testcases.reduce(function(chain, testcase, i) {
            return chain.then(function() {
                if (i > 0 && priorLogCount + 1 < suite._harness.logCount)
                    suite._harness.log("");

                priorLogCount = suite._harness.logCount;
                suite._harness.log("-- Running test case: " + testcase.name);
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

            suite._harness.log("!! EXCEPTION: " + message);
            throw e; // Reject this promise by re-throwing the error.
        });
    }
}

InjectedTestHarness.SyncTestSuite = class SyncTestSuite
{
    constructor(harness, name) {
        if (!(harness instanceof InjectedTestHarness))
            throw new Error("Must pass the test's harness as the first argument.");

        if (!name || typeof name !== "string")
            throw new Error("Tried to create SyncTestSuite without string suite name.");

        this.name = name;
        this._harness = harness;

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
        this._harness.completeTest();
    }

    runTestCases()
    {
        if (!this.testcases.length)
            throw new Error("Tried to call runTestCases() for suite with no test cases");
        if (this._startedRunning)
            throw new Error("Tried to call runTestCases() more than once.");

        this._startedRunning = true;

        this._harness.log("");
        this._harness.log("== Running test suite: " + this.name);

        var priorLogCount = this._harness.logCount;
        var suite = this;
        for (var i = 0; i < this.testcases.length; i++) {
            var testcase = this.testcases[i];
            if (i > 0 && priorLogCount + 1 < this._harness.logCount)
                this._harness.log("");

            priorLogCount = this._harness.logCount;

            this._harness.log("-- Running test case: " + testcase.name);
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

                this._harness.log("!! EXCEPTION: " + message);
                return false;
            }
        }

        return true;
    }
}

class ProtocolTestHarness extends InjectedTestHarness
{
    // InjectedTestHarness Overrides

    completeTest()
    {
        this.evaluateInPage("closeTest();");
    }

    addResult(message)
    {
        // Unfortunately, every string argument must be escaped because tests are not consistent
        // with respect to escaping with single or double quotes. Some exceptions use single quotes.
        var stringifiedMessage = typeof message !== "string" ? JSON.stringify(message) : message;
        this.evaluateInPage("log(unescape('" + escape(stringifiedMessage) + "'));");
    }

    debugLog(message)
    {
        var stringifiedMessage = typeof message !== "string" ? JSON.stringify(message) : message;
        this.evaluateInPage("debugLog(unescape('" + escape(stringifiedMessage) + "'));")
    }

    evaluateInPage(expression, callback)
    {
        let args = {
            method: "Runtime.evaluate",
            params: {expression}
        }

        if (typeof callback === "function")
            InspectorProtocol.sendCommand(args, callback);
        else
            return InspectorProtocol.awaitCommand(args);
    }
}

window.ProtocolTest = new ProtocolTestHarness();

InspectorProtocol = {};
InspectorProtocol._dispatchTable = [];
InspectorProtocol._requestId = -1;
InspectorProtocol.eventHandler = {};

InspectorProtocol.dumpInspectorProtocolMessages = false;

InspectorProtocol.sendCommand = function(methodOrObject, params, handler)
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

InspectorProtocol.awaitCommand = function(args)
{
    var {method, params} = args;
    return new Promise(function(resolve, reject) {
        this._dispatchTable[++this._requestId] = {resolve, reject};
        var messageObject = {method, params, "id": this._requestId};
        this.sendMessage(messageObject);
    }.bind(this));
}

InspectorProtocol.awaitEvent = function(args)
{
    var {event} = args;
    if (typeof event !== "string")
        throw new Error("Event must be a string.");

    return new Promise(function(resolve, reject) {
        InspectorProtocol.eventHandler[event] = function(message) {
            InspectorProtocol.eventHandler[event] = undefined;
            resolve(message);
        }
    });
}

InspectorProtocol.addEventListener = function(eventTypeOrObject, listener)
{
    var event = eventTypeOrObject;
    if (typeof eventTypeOrObject === "object")
        var {event, listener} = eventTypeOrObject;

    if (typeof event !== "string")
        throw new Error("Event name must be a string.");

    if (typeof listener !== "function")
        throw new Error("Event listener must be callable.");

    // Convert to an array of listeners.
    var listeners = InspectorProtocol.eventHandler[event];
    if (!listeners)
        listeners = InspectorProtocol.eventHandler[event] = [];
    else if (typeof listeners === "function")
        listeners = InspectorProtocol.eventHandler[event] = [listeners];

    // Prevent registering multiple times.
    if (listeners.includes(listener))
        throw new Error("Cannot register the same listener more than once.");

    listeners.push(listener);
}

InspectorProtocol.sendMessage = function(messageObject)
{
    // This matches the debug dumping in InspectorBackend, which is bypassed
    // by InspectorProtocol. Return messages should be dumped by InspectorBackend.
    if (InspectorProtocol.dumpInspectorProtocolMessages)
        console.log("frontend: " + JSON.stringify(messageObject));

    InspectorFrontendHost.sendMessageToBackend(JSON.stringify(messageObject));
}

InspectorProtocol.checkForError = function(responseObject)
{
    if (responseObject.error) {
        ProtocolTest.log("PROTOCOL ERROR: " + JSON.stringify(responseObject.error));
        ProtocolTest.completeTest();
        throw "PROTOCOL ERROR";
    }
}

InspectorFrontendAPI.dispatchMessageAsync = function(messageObject)
{
    // This matches the debug dumping in InspectorBackend, which is bypassed
    // by InspectorProtocol. Return messages should be dumped by InspectorBackend.
    if (InspectorProtocol.dumpInspectorProtocolMessages)
        console.log("backend: " + JSON.stringify(messageObject));

    // If the message has an id, then it is a reply to a command.
    var messageId = messageObject["id"];
    if (typeof messageId === "number") {
        var handler = InspectorProtocol._dispatchTable[messageId];
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
        var handler = InspectorProtocol.eventHandler[eventName];
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

window.addEventListener("message", function(event) {
    try {
        eval(event.data);
    } catch (e) {
        alert(e.stack);
        ProtocolTest.completeTest();
        throw e;
    }
});