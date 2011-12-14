importScripts("worker-pre.js");

function log(message)
{
    postMessage("log " + message);
}

function done()
{
    postMessage("DONE");
}

function init()
{
    try {
        req = new XMLHttpRequest;
        req.open("GET", "missing-file", false);
        req.send();
    } catch (e) {
        log("Exception received.");
    }
    done();
};
