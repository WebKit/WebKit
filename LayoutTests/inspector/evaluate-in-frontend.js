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

    var toInject = expandDOMSubtree.toString() + "\n" + dumpConsoleMessages.toString();
    if (document.getElementById("frontend-script"))
        toInject += "\n" + document.getElementById("frontend-script").textContent;
    evaluateInWebInspector(toInject, doit);

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

window.didEvaluateForTestInFrontend = function(callId, jsonResult)
{
    if (callbacks[callId]) {
        callbacks[callId].call(this, JSON.parse(jsonResult));
        delete callbacks[callId];
    }
};

// Injected utility functions.

function expandDOMSubtree(node)
{
    function processChildren(children)
    {
       for (var i = 0; children && i < children.length; ++i)
           expandDOMSubtree(children[i]);
    }
    WebInspector.domAgent.getChildNodesAsync(node, processChildren);
}

function dumpConsoleMessages()
{
    var result = [];
    var messages = WebInspector.console.messages;
    for (var i = 0; i < messages.length; ++i) {
        var element = messages[i].toMessageElement();
        result.push({ text: element.textContent.replace(/\u200b/g, ""), clazz: element.getAttribute("class")});
    }
    return result;
}
