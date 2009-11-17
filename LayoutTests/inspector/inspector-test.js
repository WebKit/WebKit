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
