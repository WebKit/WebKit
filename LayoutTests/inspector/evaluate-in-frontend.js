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
        layoutTestController.showWebInspector();
        window.location.href += "?reload";
    }, 0);
}

function onload()
{
    if (ignoreLoad)
        return;
    var callId = lastCallId++;
    setTimeout(function() {
        if (window.layoutTestController) {
            // layoutTestController.showWebInspector();
            layoutTestController.evaluateInWebInspector(callId, document.getElementById("frontend-script").textContent);
        }
        doit();
    }, 0);
}

function evaluateInWebInspector(script, callback)
{
    var callId = lastCallId++;
    callbacks[callId] = callback;
    setTimeout(function() {
        if (window.layoutTestController) {
            layoutTestController.evaluateInWebInspector(callId, script);
        }
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
    output.innerHTML += text + "<BR>";
}

window.didEvaluateForTestInFrontend = function(callId, jsonResult)
{
    if (callbacks[callId]) {
        callbacks[callId].call(this, JSON.parse(jsonResult));
        delete callbacks[callId];
    }
};
