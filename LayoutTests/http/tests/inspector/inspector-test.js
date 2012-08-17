var initialize_InspectorTest = function() {

var results = [];
var resultsSynchronized = false;

function consoleOutputHook(messageType)
{
    InspectorTest.addResult(messageType + ": " + Array.prototype.slice.call(arguments, 1));
}

console.log = consoleOutputHook.bind(InspectorTest, "log");
console.error = consoleOutputHook.bind(InspectorTest, "error");
console.info = consoleOutputHook.bind(InspectorTest, "info");

InspectorTest.completeTest = function()
{
    function testDispatchQueueIsEmpty() {
        if (!WebInspector.dispatchQueueIsEmpty()) {
            // Wait for unprocessed messages.
            setTimeout(testDispatchQueueIsEmpty, 10);
            return;
        }
        RuntimeAgent.evaluate("didEvaluateForTestInFrontend(" + InspectorTest.completeTestCallId + ", \"\")", "test");
    }
    testDispatchQueueIsEmpty();
}

InspectorTest.evaluateInConsole = function(code, callback)
{
    callback = InspectorTest.safeWrap(callback);

    WebInspector.consoleView.visible = true;
    WebInspector.consoleView.prompt.text = code;
    var event = document.createEvent("KeyboardEvent");
    event.initKeyboardEvent("keydown", true, true, null, "Enter", "");
    WebInspector.consoleView.prompt.proxyElement.dispatchEvent(event);
    InspectorTest.addConsoleSniffer(
        function(commandResult) {
            callback(commandResult.toMessageElement().textContent);
        });
}

InspectorTest.evaluateInConsoleAndDump = function(code, callback)
{
    callback = InspectorTest.safeWrap(callback);

    function mycallback(text)
    {
        InspectorTest.addResult(code + " = " + text);
        callback(text);
    }
    InspectorTest.evaluateInConsole(code, mycallback);
}

InspectorTest.evaluateInPage = function(code, callback)
{
    callback = InspectorTest.safeWrap(callback);

    function mycallback(error, result, wasThrown)
    {
        if (!error)
            callback(WebInspector.RemoteObject.fromPayload(result), wasThrown);
    }
    RuntimeAgent.evaluate(code, "console", false, mycallback);
}

InspectorTest.evaluateInPageWithTimeout = function(code)
{
    InspectorTest.evaluateInPage("setTimeout(unescape('" + escape(code) + "'))");
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

function onError(event)
{
    window.removeEventListener("error", onError);
    InspectorTest.addResult("Uncaught exception in inspector front-end: " + event.message + " [" + event.filename + ":" + event.lineno + "]");
    InspectorTest.completeTest();
}

window.addEventListener("error", onError);

InspectorTest.formatters = {};

InspectorTest.formatters.formatAsTypeName = function(value)
{
    return "<" + typeof value + ">";
}

InspectorTest.formatters.formatAsRecentTime = function(value)
{
    if (typeof value !== "object" || !(value instanceof Date))
        return InspectorTest.formatAsTypeName(value);
    var delta = Date.now() - value;
    return 0 <= delta && delta < 30 * 60 * 1000 ? "<plausible>" : value;
}

InspectorTest.addObject = function(object, customFormatters, prefix, firstLinePrefix)
{
    prefix = prefix || "";
    firstLinePrefix = firstLinePrefix || prefix;
    InspectorTest.addResult(firstLinePrefix + "{");
    for (var prop in object) {
        if (typeof object.hasOwnProperty === "function" && !object.hasOwnProperty(prop))
            continue;
        var prefixWithName = "    " + prefix + prop + " : ";
        var propValue = object[prop];
        if (customFormatters && customFormatters[prop]) {
            var formatterName = customFormatters[prop];
            var formatter = InspectorTest.formatters[formatterName];
            InspectorTest.addResult(prefixWithName + formatter(propValue));
        } else
            InspectorTest.dump(propValue, customFormatters, "    " + prefix, prefixWithName);
    }
    InspectorTest.addResult(prefix + "}");
}

InspectorTest.addArray = function(array, customFormatters, prefix, firstLinePrefix)
{
    prefix = prefix || "";
    firstLinePrefix = firstLinePrefix || prefix;
    InspectorTest.addResult(firstLinePrefix + "[");
    for (var i = 0; i < array.length; ++i)
        InspectorTest.dump(array[i], customFormatters, prefix + "    ");
    InspectorTest.addResult(prefix + "]");
}

InspectorTest.dump = function(value, customFormatters, prefix, prefixWithName)
{
    prefixWithName = prefixWithName || prefix;
    if (prefixWithName && prefixWithName.length > 80) {
        InspectorTest.addResult(prefixWithName + "was skipped due to prefix length limit");
        return;
    }
    if (value === null)
        InspectorTest.addResult(prefixWithName + "null");
    else if (value instanceof Array)
        InspectorTest.addArray(value, customFormatters, prefix, prefixWithName);
    else if (typeof value === "object")
        InspectorTest.addObject(value, customFormatters, prefix, prefixWithName);
    else if (typeof value === "string")
        InspectorTest.addResult(prefixWithName + "\"" + value + "\"");
    else
        InspectorTest.addResult(prefixWithName + value);
}

InspectorTest.assertGreaterOrEqual = function(a, b, message)
{
    if (a < b)
        InspectorTest.addResult("FAILED: " + (message ? message + ": " : "") + a + " < " + b);
}

InspectorTest.navigate = function(url, callback)
{
    InspectorTest._pageLoadedCallback = InspectorTest.safeWrap(callback);

    if (WebInspector.panels.network)
        WebInspector.panels.network._reset();
    InspectorTest.evaluateInConsole("window.location = '" + url + "'");
}

InspectorTest.hardReloadPage = function(callback)
{
    InspectorTest._innerReloadPage(true, callback);
}

InspectorTest.reloadPage = function(callback)
{
    InspectorTest._innerReloadPage(false, callback);
}

InspectorTest._innerReloadPage = function(hardReload, callback)
{
    InspectorTest._pageLoadedCallback = InspectorTest.safeWrap(callback);

    if (WebInspector.panels.network)
        WebInspector.panels.network._reset();
    PageAgent.reload(hardReload);
}

InspectorTest.pageLoaded = function()
{
    resultsSynchronized = false;
    InspectorTest.addResult("Page reloaded.");
    if (InspectorTest._pageLoadedCallback) {
        var callback = InspectorTest._pageLoadedCallback;
        delete InspectorTest._pageLoadedCallback;
        callback();
    }
}

InspectorTest.runWhenPageLoads = function(callback)
{
    var oldCallback = InspectorTest._pageLoadedCallback;
    function chainedCallback()
    {
        if (oldCallback)
            oldCallback();
        callback();
    }
    InspectorTest._pageLoadedCallback = InspectorTest.safeWrap(chainedCallback);
}

InspectorTest.runAfterPendingDispatches = function(callback)
{
    callback = InspectorTest.safeWrap(callback);
    InspectorBackend.runAfterPendingDispatches(callback);
}

InspectorTest.createKeyEvent = function(keyIdentifier, ctrlKey, altKey, shiftKey, metaKey)
{
    var evt = document.createEvent("KeyboardEvent");
    evt.initKeyboardEvent("keydown", true /* can bubble */, true /* can cancel */, null /* view */, keyIdentifier, "", ctrlKey, altKey, shiftKey, metaKey);
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
        var nextTest = testSuiteTests.shift();
        InspectorTest.addResult("");
        InspectorTest.addResult("Running: " + /function\s([^(]*)/.exec(nextTest)[1]);
        InspectorTest.safeWrap(nextTest)(runner, runner);
    }
    runner();
}

InspectorTest.assertEquals = function(expected, found, message)
{
    if (expected === found)
        return;

    var error;
    if (message)
        error = "Failure (" + message + "):";
    else
        error = "Failure:";
    throw new Error(error + " expected <" + expected + "> found <" + found + ">");
}

InspectorTest.assertTrue = function(found, message)
{
    InspectorTest.assertEquals(true, !!found, message);
}

InspectorTest.safeWrap = function(func, onexception)
{
    function result()
    {
        if (!func)
            return;
        var wrapThis = this;
        try {
            return func.apply(wrapThis, arguments);
        } catch(e) {
            InspectorTest.addResult("Exception while running: " + func + "\n" + (e.stack || e));
            if (onexception)
                InspectorTest.safeWrap(onexception)();
            else
                InspectorTest.completeTest();
        }
    }
    return result;
}

InspectorTest.addSniffer = function(receiver, methodName, override, opt_sticky)
{
    override = InspectorTest.safeWrap(override);

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

InspectorTest.addConsoleSniffer = function(override, opt_sticky)
{
    InspectorTest.addSniffer(WebInspector.ConsoleView.prototype, "_appendConsoleMessage", override, opt_sticky);
}

InspectorTest.override = function(receiver, methodName, override, opt_sticky)
{
    override = InspectorTest.safeWrap(override);

    var original = receiver[methodName];
    if (typeof original !== "function")
        throw ("Cannot find method to override: " + methodName);

    receiver[methodName] = function(var_args) {
        try {
            try {
                var result = override.apply(this, arguments);
            } catch (e) {
                throw ("Exception in overriden method '" + methodName + "': " + e);
            }
        } finally {
            if (!opt_sticky)
                receiver[methodName] = original;
        }
        return result;
    };

    return original;
}

InspectorTest.textContentWithLineBreaks = function(node)
{
    var buffer = "";
    var currentNode = node;
    while (currentNode = currentNode.traverseNextNode(node)) {
        if (currentNode.nodeType === Node.TEXT_NODE)
            buffer += currentNode.nodeValue;
        else if (currentNode.nodeName === "LI")
            buffer += "\n    ";
        else if (currentNode.classList.contains("console-message"))
            buffer += "\n\n";
    }
    return buffer;
}

};

var initializeCallId = 0;
var runTestCallId = 1;
var completeTestCallId = 2;
var frontendReopeningCount = 0;

function reopenFrontend()
{
    closeFrontend(openFrontendAndIncrement);
}

function closeFrontend(callback)
{
    // Do this asynchronously to allow InspectorBackendDispatcher to send response
    // back to the frontend before it's destroyed.
    setTimeout(function() {
        testRunner.closeWebInspector();
        callback();
    }, 0);
}

function openFrontendAndIncrement()
{
    frontendReopeningCount++;
    testRunner.showWebInspector();
    runTest();
}

function runAfterIframeIsLoaded()
{
    if (window.testRunner)
        testRunner.waitUntilDone();
    function step()
    {
        if (!window.iframeLoaded)
            setTimeout(step, 100);
        else
            runTest();
    }
    setTimeout(step, 100);
}

function runTest(enableWatchDogWhileDebugging)
{
    if (!window.testRunner)
        return;

    testRunner.dumpAsText();
    testRunner.waitUntilDone();

    function initializeFrontend(initializationFunctions)
    {
        if (window.InspectorTest) {
            InspectorTest.pageLoaded();
            return;
        }

        InspectorTest = {};
    
        for (var i = 0; i < initializationFunctions.length; ++i) {
            try {
                initializationFunctions[i]();
            } catch (e) {
                console.error("Exception in test initialization: " + e);
                InspectorTest.completeTest();
            }
        }
    }

    function runTestInFrontend(testFunction, completeTestCallId)
    {
        if (InspectorTest.completeTestCallId) 
            return;

        InspectorTest.completeTestCallId = completeTestCallId;

        WebInspector.showPanel("audits");
        try {
            testFunction();
        } catch (e) {
            console.error("Exception during test execution: " + e,  (e.stack ? e.stack : "") );
            InspectorTest.completeTest();
        }
    }

    var initializationFunctions = [ String(initialize_InspectorTest) ];
    for (var name in window) {
        if (name.indexOf("initialize_") === 0 && typeof window[name] === "function" && name !== "initialize_InspectorTest")
            initializationFunctions.push(window[name].toString());
    }
    var parameters = ["[" + initializationFunctions + "]"];
    var toEvaluate = "(" + initializeFrontend + ")(" + parameters.join(", ") + ");";
    testRunner.evaluateInWebInspector(initializeCallId, toEvaluate);

    parameters = [test, completeTestCallId];
    toEvaluate = "(" + runTestInFrontend + ")(" + parameters.join(", ") + ");";
    testRunner.evaluateInWebInspector(runTestCallId, toEvaluate);

    if (enableWatchDogWhileDebugging) {
        function watchDog()
        {
            console.log("Internal watchdog triggered at 20 seconds. Test timed out.");
            closeInspectorAndNotifyDone();
        }
        window._watchDogTimer = setTimeout(watchDog, 20000);
    }
}

function didEvaluateForTestInFrontend(callId)
{
    if (callId !== completeTestCallId)
        return;
    delete window.completeTestCallId;
    // Close inspector asynchrously to allow caller of this
    // function send response before backend dispatcher and frontend are destroyed.
    setTimeout(closeInspectorAndNotifyDone, 0);
}

function closeInspectorAndNotifyDone()
{
    if (window._watchDogTimer)
        clearTimeout(window._watchDogTimer);

    testRunner.closeWebInspector();
    setTimeout(function() {
        testRunner.notifyDone();
    }, 0);
}

var outputElement;

function output(text)
{
    if (!outputElement) {
        var intermediate = document.createElement("div");
        document.body.appendChild(intermediate);

        var intermediate2 = document.createElement("div");
        intermediate.appendChild(intermediate2);

        outputElement = document.createElement("div");
        outputElement.className = "output";
        outputElement.id = "output";
        outputElement.style.whiteSpace = "pre";
        intermediate2.appendChild(outputElement);
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

function StandaloneTestRunnerStub()
{
}

StandaloneTestRunnerStub.prototype = {
    dumpAsText: function()
    {
    },

    waitUntilDone: function()
    {
    },

    closeWebInspector: function()
    {
        window.opener.postMessage(["closeWebInspector"], "*");
    },

    notifyDone: function()
    {
        var actual = document.body.innerText + "\n";
        window.opener.postMessage(["notifyDone", actual], "*");
    },

    evaluateInWebInspector: function(callId, script)
    {
        window.opener.postMessage(["evaluateInWebInspector", callId, script], "*");
    },

    display: function() { }
}

if (!window.testRunner && window.opener)
    window.testRunner = new StandaloneTestRunnerStub();
