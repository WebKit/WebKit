/*
 * Copyright (C) 2012 Samsung Electronics. All rights reserved.
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
InspectorTest.dumpInspectorProtocolMessages = false;
InspectorTest._dispatchTable = [];
InspectorTest._requestId = -1;
InspectorTest.eventHandler = {};

/**
 * @param {string} method
 * @param {object} params
 * @param {function({object} messageObject)=} handler
 */
InspectorTest.sendCommand = function(method, params, handler)
{
    this._dispatchTable[++this._requestId] = handler;

    var messageObject = { "method": method,
                          "params": params,
                          "id": this._requestId };

    // This matches the debug dumping in InspectorBackend, which is bypassed
    // by InspectorTest. Return messages should be dumped by InspectorBackend.
    if (this.dumpInspectorProtocolMessages)
        console.log("frontend: " + JSON.stringify(messageObject));

    InspectorFrontendHost.sendMessageToBackend(JSON.stringify(messageObject));

    return this._requestId;
}

/**
 * @param {object} messageObject
 */
InspectorFrontendAPI.dispatchMessageAsync = function(messageObject)
{
    var messageId = messageObject["id"];
    if (typeof messageId === "number") {
        var handler = InspectorTest._dispatchTable[messageId];
        if (handler && typeof handler === "function")
            handler(messageObject);
    } else {
        var eventName = messageObject["method"];
        var eventHandler = InspectorTest.eventHandler[eventName];
        if (eventHandler)
            eventHandler(messageObject);
        else if (InspectorTest.defaultEventHandler)
            InspectorTest.defaultEventHandler(messageObject);
    }
}

/**
* Registers an event handler for messages coming from the InspectorBackend.
* If multiple callbacks are registered for the same event, it will chain the execution.
* @param {string} event name
* @param {function} handler to be executed
* @param {boolean} execute the handler before all other handlers
*/
InspectorTest.addEventListener = function(eventName, callback, capture)
{
    if (!InspectorTest.eventHandler[eventName]) {
        InspectorTest.eventHandler[eventName] = callback;
        return;
    }
    var firstHandler = InspectorTest.eventHandler[eventName];
    var secondHandler = callback;
    if (capture) {
        // Swap firstHandler with the new callback, so that we execute the callback first.
        [firstHandler, secondHandler] = [secondHandler, firstHandler];
    }
    InspectorTest.eventHandler[eventName] = function(messageObject)
    {
        firstHandler(messageObject);
        secondHandler(messageObject);
    };
}

/**
* Logs message to document.
* @param {string} message
*/
InspectorTest.log = function(message)
{
    this.sendCommand("Runtime.evaluate", { "expression": "log(" + JSON.stringify(message) + ")" } );
}

/**
* Logs an assert message to document.
* @param {boolean} condition
* @param {string} message
*/
InspectorTest.assert = function(condition, message)
{
    var status = condition ? "PASS" : "FAIL";
    this.sendCommand("Runtime.evaluate", { "expression": "log(" + JSON.stringify(status + ": " + message) + ")" } );
}

/**
* Logs message directly to process stdout via alert function (hopefully followed by flush call).
* This message should survive process crash or kill by timeout.
* @param {string} message
*/
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

/**
 * @param {string} scriptName
 */
InspectorTest.importScript = function(scriptName)
{
    var xhr = new XMLHttpRequest();
    xhr.open("GET", scriptName, false);
    xhr.send(null);
    if (xhr.status !== 0 && xhr.status !== 200)
        throw new Error("Invalid script URL: " + scriptName);
    var script = "try { " + xhr.responseText + "} catch (e) { alert(" + JSON.stringify("Error in: " + scriptName) + "); throw e; }";
    window.eval(script);
}

InspectorTest.initializeInspectorModels = function()
{
    // Catch any errors and finish the test early.
    console.error = window.onerror = function()
    {
        InspectorTest.log(Array.prototype.join.call(arguments, ', '));
        InspectorTest.completeTest();
    };

    console.assert = function(assertion, message)
    {
        if (assertion)
            return;

        InspectorTest.log("ASSERT:" + message);
        InspectorTest.completeTest();
    };

    // Note: This function overwrites the InspectorFrontendAPI, so there's currently no
    // way to intercept the messages from the backend.

    var inspectorScripts = [
        "Base/WebInspector",
        "Base/Object",

        "Base/DOMUtilities",
        "Base/Setting",
        "Base/URLUtilities",
        "Base/Utilities",

        "Protocol/CSSObserver",
        "Protocol/DOMObserver",
        "Protocol/DebuggerObserver",
        "Protocol/InspectorBackend",
        "Protocol/InspectorFrontendAPI",
        "Protocol/InspectorFrontendHostStub",
        "Protocol/InspectorJSBackendCommands",
        "Protocol/InspectorObserver",
        "Protocol/InspectorWebBackendCommands",
        "Protocol/MessageDispatcher",
        "Protocol/PageObserver",
        "Protocol/RemoteObject",
        "Protocol/RuntimeObserver",

        "Models/BreakpointAction",
        "Models/SourceCode",

        "Models/Breakpoint",
        "Models/Color",
        "Models/ContentFlow",
        "Models/DOMNode",
        "Models/DOMStorageObject",
        "Models/DOMTree",
        "Models/ExecutionContext",
        "Models/ExecutionContextList",
        "Models/Frame",
        "Models/IndexedDatabase",
        "Models/IndexedDatabaseObjectStore",
        "Models/IndexedDatabaseObjectStoreIndex",
        "Models/Probe",
        "Models/ProbeSet",
        "Models/ProbeSetDataFrame",
        "Models/ProbeSetDataTable",
        "Models/Resource",
        "Models/ResourceCollection",
        "Models/Revision",
        "Models/Script",
        "Models/SourceCodeLocation",
        "Models/SourceCodeRevision",
        "Models/SourceMapResource",
        "Models/TextRange",

        "Controllers/CSSStyleManager",
        "Controllers/DOMTreeManager",
        "Controllers/DebuggerManager",
        "Controllers/FrameResourceManager",
        "Controllers/ProbeManager",
        "Controllers/RuntimeManager",
        "Controllers/RuntimeManager",
        "Controllers/StorageManager"
    ];

    // This corresponds to loading the scripts in Main.hml.
    for (var i = 0; i < inspectorScripts.length; ++i)
        InspectorTest.importScript("../../../../../Source/WebInspectorUI/UserInterface/" + inspectorScripts[i] + ".js");

    // The initialization should be in sync with WebInspector.loaded in Main.js.
    // FIXME: As soon as we can support all the observers and managers we should remove UI related tasks
    // from WebInspector.loaded, so that it can be used from the LayoutTests.

    InspectorFrontendHost.loaded();

    // Enable agents.
    InspectorAgent.enable();

    InspectorBackend.registerInspectorDispatcher(new WebInspector.InspectorObserver);
    InspectorBackend.registerPageDispatcher(new WebInspector.PageObserver);
    InspectorBackend.registerDOMDispatcher(new WebInspector.DOMObserver);
    InspectorBackend.registerDebuggerDispatcher(new WebInspector.DebuggerObserver);
    InspectorBackend.registerCSSDispatcher(new WebInspector.CSSObserver);
    if (InspectorBackend.registerRuntimeDispatcher)
        InspectorBackend.registerRuntimeDispatcher(new WebInspector.RuntimeObserver);

    WebInspector.frameResourceManager = new WebInspector.FrameResourceManager;
    WebInspector.storageManager = new WebInspector.StorageManager;
    WebInspector.domTreeManager = new WebInspector.DOMTreeManager;
    WebInspector.cssStyleManager = new WebInspector.CSSStyleManager;
    WebInspector.debuggerManager = new WebInspector.DebuggerManager;
    WebInspector.runtimeManager = new WebInspector.RuntimeManager;
    WebInspector.probeManager = new WebInspector.ProbeManager;
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
