var initialize_InspectorTest = function() {

var results = [];
var resultsSynchronized = false;

InspectorTest.completeTest = function()
{
    InspectorBackend.didEvaluateForTestInFrontend(InspectorTest.completeTestCallId, "");
};

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
};

InspectorTest.evaluateInPage = function(code, callback)
{
    InjectedScriptAccess.getDefault().evaluate(code, "console", callback || function() {});
};

InspectorTest.addResult = function(text)
{
    results.push(text);
    if (resultsSynchronized)
        addResultToPage(text);
    else {
        for (var i = 0; i < results.length; ++i)
            addResultToPage(results[i]);
        resultsSynchronized = true;
    }

    function addResultToPage(text)
    {
        InspectorTest.evaluateInPage("output(unescape('" + escape(text) + "'))");
    }
};

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
        else if (typeof propValue === "object")
            InspectorTest.addObject(propValue, nondeterministicProps, prefix + "    ", prefixWithName);
        else if (typeof propValue === "string")
            InspectorTest.addResult(prefixWithName + "\"" + propValue + "\"");
        else
            InspectorTest.addResult(prefixWithName + propValue);
    }
    InspectorTest.addResult(prefix + "}");
};

InspectorTest.reloadPage = function(callback)
{
    InspectorTest._reloadPageCallback = callback;
    InspectorBackend.reloadPage();
};

InspectorTest.pageReloaded = function()
{
    resultsSynchronized = false;
    InspectorTest.addResult("Page reloaded.");
    if (InspectorTest._reloadPageCallback) {
        var callback = InspectorTest._reloadPageCallback;
        delete InspectorTest._reloadPageCallback;
        callback();
    }
};

InspectorTest.runAfterPendingDispatches = function(callback)
{
    WebInspector.TestController.prototype.runAfterPendingDispatches(callback);
};

InspectorTest.enableResourceTracking = function(callback)
{
    if (WebInspector.panels.resources.resourceTrackingEnabled)
        callback();
    else {
        InspectorTest._reloadPageCallback = callback;
        InspectorBackend.enableResourceTracking(false);
    }
};

InspectorTest.disableResourceTracking = function()
{
    InspectorBackend.disableResourceTracking(false);
};

InspectorTest.findDOMNode = function(root, filter, callback)
{
    var found = false;
    var pendingCalls = 0;

    if (root)
        findDOMNode(root);
    else
        waitForDocument();

    function waitForDocument()
    {
        root = WebInspector.domAgent.document;
        if (root)
            findDOMNode(root);
        else
            InspectorTest._addSniffer(WebInspector, setDocument, waitForDocument);
    }

    function findDOMNode(node)
    {
        if (filter(node)) {
            callback(node);
            found = true;
        } else {
            pendingCalls += 1;
            WebInspector.domAgent.getChildNodesAsync(node, processChildren);
        }

        function processChildren(children)
        {
            pendingCalls -= 1;

            for (var i = 0; !found && children && i < children.length; ++i)
                findDOMNode(children[i]);

            if (!found && !pendingCalls && node == root)
                callback(null);
        }
    }
};

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
};

};

var runTestCallId = 0;
var completeTestCallId = 1;

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

function output(text)
{
    var outputElement = document.createElement("div");
    outputElement.id = "output";
    outputElement.style.whiteSpace = "pre";
    outputElement.appendChild(document.createTextNode(text));
    outputElement.appendChild(document.createElement("br"));
    document.body.appendChild(outputElement);
}
