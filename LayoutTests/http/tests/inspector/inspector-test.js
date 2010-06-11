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
        if (window.layoutTestController)
            layoutTestController.notifyDone();
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
        else if (typeof propValue === "object")
            dumpObject(propValue, nondeterministicProps, prefix + "    ", prefixWithName);
        else if (typeof propValue === "string")
            output(prefixWithName + "\"" + propValue + "\"");
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
