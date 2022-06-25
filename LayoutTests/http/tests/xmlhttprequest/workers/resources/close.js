importScripts("worker-pre.js");

function done()
{
    postMessage("DONE");
}

onmessage = function(evt) {
    req = new XMLHttpRequest();
    req.onreadystatechange = processStateChange;
    req.open("GET", "methods.cgi", evt.data == "async");
    req.send("");
}

var failIfCalled = false;
function processStateChange()
{
    if (failIfCalled)
        // FIXME: XMLHttpRequest::didReceiveData() calls multiple event handlers without returning to the event loop. We need some way to stop active XHR requests, but calling stopActiveDOMObjects() is too draconian (stops everything, including nested workers).
        //  throw "FAIL: processStateChange(" + req.readyState + ") called after close()";
        return;

    if (req.readyState > 1) {
        failIfCalled = true;
        done();
        close();
    }
}

