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
    // Start in a timer, as synchronous opening of web inspector may fail on Windows
    setTimeout(function() {
        if (window.layoutTestController)
            layoutTestController.showWebInspector();
    }, 0);
}

function onload()
{
    if (ignoreLoad) {
        // Inject scripts into the frontend on the first pass.  Some other logic may want to
        // use them before the reload.
        var toInject = [];
        for (var name in window) {
            if (name.indexOf("frontend_") === 0 && typeof window[name] === "function")
                toInject.push(window[name].toString());
        }
        // Invoke a setup method if it has been specified
        if (window["frontend_setup"]) 
            toInject.push("frontend_setup();");

        evaluateInWebInspector(toInject.join("\n"), function(arg) {
            window.location.href += "?reload";
        });
        return;
    }

    var outputElement = document.createElement("div");
    outputElement.id = "output";
    document.body.appendChild(outputElement);

    // Make sure web inspector has settled down before executing user code
    evaluateInWebInspector("true", doit);

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
