var initialize_InspectorTest = function() {

var results = [];
var resultsSynchronized = false;

InspectorTest.completeTest = function()
{
    InspectorBackend.didEvaluateForTestInFrontend(InspectorTest.completeTestCallId, "");
}

InspectorTest.evaluateInConsole = function(code, callback)
{
    WebInspector.console.visible = true;
    WebInspector.console.prompt.text = code;
    var event = document.createEvent("KeyboardEvent");
    event.initKeyboardEvent("keydown", true, true, null, "Enter", "");
    WebInspector.console.promptElement.dispatchEvent(event);
    InspectorTest._addSniffer(WebInspector.ConsoleView.prototype, "addMessage",
        function(commandResult) {
            if (callback)
                callback(commandResult.toMessageElement().textContent);
        });
}

InspectorTest.evaluateInConsoleAndDump = function(code, callback)
{
    function mycallback(text)
    {
        InspectorTest.addResult(code + " = " + text);
        if (callback)
            callback(text);
    }
    InspectorTest.evaluateInConsole(code, mycallback);
}

InspectorTest.evaluateInPage = function(code, callback)
{
    function mycallback(result)
    {
        if (callback)
            callback(WebInspector.RemoteObject.fromPayload(result));
    }
    InspectorBackend.evaluate(code, "console", false, mycallback);
}

InspectorTest.evaluateInPageWithTimeout = function(code, callback)
{
    InspectorTest.evaluateInPage("setTimeout(unescape('" + escape(code) + "'))", callback);
}

InspectorTest.addResult = function(text)
{
    results.push(text);
    if (resultsSynchronized)
        addResultToPage(text);
    else {
        clearResults();
        for (var i = 0; i < results.length; ++i)
            addResultToPage(results[i]);
        resultsSynchronized = true;
    }

    function clearResults()
    {
        InspectorTest.evaluateInPage("clearOutput()");
    }

    function addResultToPage(text)
    {
        InspectorTest.evaluateInPage("output(unescape('" + escape(text) + "'))");
    }
}

InspectorTest.addResults = function(textArray)
{
    if (!textArray)
        return;
    for (var i = 0, size = textArray.length; i < size; ++i)
        InspectorTest.addResult(textArray[i]);
}

InspectorTest.addObject = function(object, nondeterministicProps, prefix, firstLinePrefix)
{
    prefix = prefix || "";
    firstLinePrefix = firstLinePrefix || prefix;
    InspectorTest.addResult(firstLinePrefix + "{");
    for (var prop in object) {
        if (!object.hasOwnProperty(prop))
            continue;
        var prefixWithName = prefix + "    " + prop + " : ";
        var propValue = object[prop];
        if (nondeterministicProps && prop in nondeterministicProps)
            InspectorTest.addResult(prefixWithName + "<" + typeof propValue + ">");
        else if (propValue === null)
            InspectorTest.addResult(prefixWithName + "null");
        else if (typeof propValue === "object")
            InspectorTest.addObject(propValue, nondeterministicProps, prefix + "    ", prefixWithName);
        else if (typeof propValue === "string")
            InspectorTest.addResult(prefixWithName + "\"" + propValue + "\"");
        else
            InspectorTest.addResult(prefixWithName + propValue);
    }
    InspectorTest.addResult(prefix + "}");
}

InspectorTest.reloadPage = function(callback)
{
    InspectorTest._reloadPageCallback = callback;

    if (WebInspector.panels.network)
        WebInspector.panels.network._reset();
    InspectorBackend.reloadPage(false);
}

InspectorTest.pageReloaded = function()
{
    resultsSynchronized = false;
    InspectorTest.addResult("Page reloaded.");
    if (InspectorTest._reloadPageCallback) {
        var callback = InspectorTest._reloadPageCallback;
        delete InspectorTest._reloadPageCallback;
        callback();
    }
}

InspectorTest.runAfterPendingDispatches = function(callback)
{
    WebInspector.TestController.prototype.runAfterPendingDispatches(callback);
}

InspectorTest.createKeyEvent = function(keyIdentifier)
{
    var evt = document.createEvent("KeyboardEvent");
    evt.initKeyboardEvent("keydown", true /* can bubble */, true /* can cancel */, null /* view */, keyIdentifier, "");
    return evt;
}

InspectorTest.runTestSuite = function(testSuite)
{
    var testSuiteTests = testSuite.slice();

    function runner()
    {
        if (!testSuiteTests.length) {
            InspectorTest.completeTest();
            return;
        }
        var nextTestFunction = testSuiteTests.shift();
        nextTestFunction(runner);
    }

    runner();
}

InspectorTest._addSniffer = function(receiver, methodName, override, opt_sticky)
{
    var original = receiver[methodName];
    if (typeof original !== "function")
        throw ("Cannot find method to override: " + methodName);

    receiver[methodName] = function(var_args) {
        try {
            var result = original.apply(this, arguments);
        } finally {
            if (!opt_sticky)
                receiver[methodName] = original;
        }
        // In case of exception the override won't be called.
        try {
            override.apply(this, arguments);
        } catch (e) {
            throw ("Exception in overriden method '" + methodName + "': " + e);
        }
        return result;
    };
}

};

var runTestCallId = 0;
var completeTestCallId = 1;

function runAfterIframeIsLoaded()
{
    if (window.layoutTestController)
        layoutTestController.waitUntilDone();
    function step()
    {
        if (!window.iframeLoaded)
            setTimeout(step, 100);
        else
            runTest();
    }
    setTimeout(step, 100);
}

function runTest()
{
    if (!window.layoutTestController)
        return;

    layoutTestController.dumpAsText();
    layoutTestController.waitUntilDone();

    function runTestInFrontend(initializationFunctions, testFunction, completeTestCallId)
    {
        if (window.InspectorTest) {
            InspectorTest.pageReloaded();
            return;
        }

        InspectorTest = {};
        InspectorTest.completeTestCallId = completeTestCallId;

        for (var i = 0; i < initializationFunctions.length; ++i) {
            try {
                initializationFunctions[i]();
            } catch (e) {
                console.error("Exception in test initialization: " + e);
            }
        }

        WebInspector.showPanel("elements");
        testFunction();
    }

    var initializationFunctions = [];
    for (var name in window) {
        if (name.indexOf("initialize_") === 0 && typeof window[name] === "function")
            initializationFunctions.push(window[name].toString());
    }
    var parameters = ["[" + initializationFunctions + "]", test, completeTestCallId];
    var toEvaluate = "(" + runTestInFrontend + ")(" + parameters.join(", ") + ");";
    layoutTestController.evaluateInWebInspector(runTestCallId, toEvaluate);
}

function didEvaluateForTestInFrontend(callId)
{
    if (callId !== completeTestCallId)
        return;
    layoutTestController.closeWebInspector();
    setTimeout(function() {
        layoutTestController.notifyDone();
    }, 0);
}

var outputElement;

function output(text)
{
    if (!outputElement) {
        outputElement = document.createElement("div");
        outputElement.className = "output";
        outputElement.style.whiteSpace = "pre";
        document.body.appendChild(outputElement);
    }
    outputElement.appendChild(document.createTextNode(text));
    outputElement.appendChild(document.createElement("br"));
}

function clearOutput()
{
    if (outputElement) {
        outputElement.parentNode.removeChild(outputElement);
        outputElement = null;
    }
}
