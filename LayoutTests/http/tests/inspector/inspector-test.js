var lastCallId = 0;
var callbacks = {};

if (window.layoutTestController) {
    layoutTestController.dumpAsText();
    layoutTestController.waitUntilDone();
}

function onload()
{
    var outputElement = document.createElement("div");
    outputElement.id = "output";
    outputElement.style.whiteSpace = "pre";
    document.body.appendChild(outputElement);

    var toInject = [];
    for (var name in window) {
        if (name.indexOf("frontend_") === 0 && typeof window[name] === "function")
            toInject.push(window[name].toString());
    }
    toInject.push("frontend_setupTestEnvironment();");
    evaluateInWebInspector(toInject.join("\n"), doit);
}

function evaluateInWebInspector(script, callback)
{
    var callId = lastCallId++;
    callbacks[callId] = callback;
    if (window.layoutTestController)
        layoutTestController.evaluateInWebInspector(callId, script);
}

function notifyDone()
{
    evaluateInWebInspector("true", function() {
        if (window.layoutTestController) {
            layoutTestController.closeWebInspector();
            // Wait until Web Inspector actually closes before calling notifyDone.
            setTimeout(function() {
                layoutTestController.notifyDone();
            }, 0);
        }
    });
}

function dumpObject(object, nondeterministicProps, prefix, firstLinePrefix)
{
    prefix = prefix || "";
    firstLinePrefix = firstLinePrefix || prefix;
    output(firstLinePrefix + "{");
    for (var prop in object) {
        var prefixWithName = prefix + "    " + prop + " : ";
        var propValue = object[prop];
        if (nondeterministicProps && prop in nondeterministicProps)
            output(prefixWithName + "<" + typeof propValue + ">");
        else if (propValue === null)
            output(prefixWithName + "null");
        else if (typeof propValue === "object")
            dumpObject(propValue, nondeterministicProps, prefix + "    ", prefixWithName);
        else if (typeof propValue === "string")
            output(prefixWithName + "\"" + propValue + "\"");
        else if (typeof propValue === "function")
            output(prefixWithName + "<function>");
        else
            output(prefixWithName + propValue);
    }
    output(prefix + "}");
}

function dumpArray(result)
{
    if (result instanceof Array) {
        for (var i = 0; i < result.length; ++i)
            output(result[i]);
    } else
        output(result);
}

function completeTest(result)
{
    dumpArray(result);
    notifyDone();
}

function output(text)
{
    var output = document.getElementById("output");
    output.appendChild(document.createTextNode(text));
    output.appendChild(document.createElement("br"));
}

function didEvaluateForTestInFrontend(callId, jsonResult)
{
    if (callbacks[callId]) {
        callbacks[callId].call(this, JSON.parse(jsonResult));
        delete callbacks[callId];
    }
}

function runAfterIframeIsLoaded(continuation)
{
    function step()
    {
        if (!window.iframeLoaded)
            setTimeout(step, 100);
        else
            continuation();
    }
    setTimeout(step, 100);
}

// Front-end utilities.

function frontend_dumpTreeOutline(treeItem, result)
{
    var children = treeItem.children;
    for (var i = 0; i < children.length; ++i)
        frontend_dumpTreeItem(children[i], result);
}

function frontend_dumpTreeItem(treeItem, result, prefix)
{
    prefix = prefix || "";
    result.push(prefix + treeItem.listItemElement.textContent);
    treeItem.expand();
    var children = treeItem.children;
    for (var i = 0; children && i < children.length; ++i)
        frontend_dumpTreeItem(children[i], result, prefix + "    ");
}

function frontend_setupTestEnvironment()
{
   WebInspector.showPanel("elements");
}

function frontend_addSniffer(receiver, methodName, override, opt_sticky)
{
    var orig = receiver[methodName];
    if (typeof orig !== "function")
        throw ("Cannot find method to override: " + methodName);
    receiver[methodName] = function(var_args) {
        try {
            var result = orig.apply(this, arguments);
        } finally {
            if (!opt_sticky)
                receiver[methodName] = orig;
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

function frontend_createKeyEvent(keyIdentifier)
{
    var evt = document.createEvent("KeyboardEvent");
    evt.initKeyboardEvent("keydown", true /* can bubble */, true /* can cancel */, null /* view */, keyIdentifier, "");
    return evt;
}

function frontend_evaluateInConsole(code, callback)
{
    WebInspector.console.visible = true;
    WebInspector.console.prompt.text = code;
    WebInspector.console.promptElement.dispatchEvent(frontend_createKeyEvent("Enter"));

    frontend_addSniffer(WebInspector.ConsoleView.prototype, "addMessage",
        function(commandResult) {
            callback(commandResult.toMessageElement().textContent);
        });
}
