var refererLocation = location.href.replace(/:\/\/.*\@/, "://").replace(/#.*$/, "");
var xhr = new XMLHttpRequest;

importScripts("worker-pre.js");

function log(message)
{
    postMessage("log " + message);
}

function done()
{
    postMessage("DONE");
}

function verifyReferer(method, xhr)
{
    if (xhr.responseText == refererLocation)
        log("PASS: " + method + " referer.");
    else
        log("FAIL: " + method + ". Expected referer: '" + refererLocation + "' Actual referer: '" + xhr.responseText + "'");
}

function processStateChange()
{
    if (xhr.readyState == 4) {
	verifyReferer("Async", xhr);
        done();
    }
}

function init()
{
    xhr.open("GET", "../../resources/print-referer.cgi", false);
    xhr.send(null);
    verifyReferer("Sync", xhr);
    xhr.open("GET", "../../resources/print-referer.cgi", true);
    xhr.onreadystatechange = processStateChange;
    xhr.send(null);
}

