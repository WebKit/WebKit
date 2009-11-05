var lastCallId = 0;
var callbacks = {};

if (window.layoutTestController) {
    layoutTestController.dumpAsText();
    layoutTestController.waitUntilDone();
}

// We ignore initial load of the page, enable inspector and initiate reload. This allows inspector controller
// to capture events that happen during the initial page load.
var ignoreLoad = window.location.href.indexOf("?reload") === -1;
if (ignoreLoad) {
    setTimeout(function() {
        if (window.layoutTestController)
            layoutTestController.showWebInspector();
        window.location.href += "?reload";
    }, 0);
}

function onload()
{
    if (ignoreLoad)
        return;

    var outputElement = document.createElement("div");
    outputElement.id = "output";
    document.body.appendChild(outputElement);

    var toInject = [];
    for (var name in window) {
        if (name.indexOf("frontend_") === 0 && typeof window[name] === "function")
            toInject.push(window[name].toString());
    }
    evaluateInWebInspector(toInject.join("\n"), doit);

    // Make sure web inspector window is closed before the test is interrupted.
    setTimeout(function() {
        alert("Internal timeout exceeded.")
        if (window.layoutTestController) {
            layoutTestController.closeWebInspector();
            layoutTestController.notifyDone();
        }
    }, 10000);
}

function evaluateInWebInspector(script, callback)
{
    var callId = lastCallId++;
    callbacks[callId] = callback;
    setTimeout(function() {
        if (window.layoutTestController)
            layoutTestController.evaluateInWebInspector(callId, script);
    }, 0);
}

function notifyDone()
{
    setTimeout(function() {
        if (window.layoutTestController) {
            layoutTestController.closeWebInspector();
            layoutTestController.notifyDone();
        }
    }, 0);
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
