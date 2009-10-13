var lastCallId = 0;
var callbacks = {};

function evaluateInWebInspector(script, callback)
{
    callbacks[lastCallId] = callback;
    setTimeout(function() {
        if (window.layoutTestController) {
            layoutTestController.evaluateInWebInspector(lastCallId++, script);
        }
    }, 0);

}

function showWebInspector()
{
    setTimeout(function() {
        if (window.layoutTestController) {
            layoutTestController.showWebInspector();
        }
    }, 0);
}

function closeWebInspector()
{
    setTimeout(function() {
        if (window.layoutTestController) {
            layoutTestController.closeWebInspector();
        }
    }, 0);
}

function onload()
{
    setTimeout(function() {
        if (window.layoutTestController) {
            layoutTestController.showWebInspector();
            layoutTestController.evaluateInWebInspector(lastCallId++, document.getElementById("frontend-script").textContent);
        }
        doit();
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

if (window.layoutTestController) {
    layoutTestController.dumpAsText();
    layoutTestController.waitUntilDone();
}
