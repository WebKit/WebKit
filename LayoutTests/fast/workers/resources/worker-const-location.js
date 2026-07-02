const location = 2; // Should not throw.

function log(message)
{
    postMessage(message);
}

onmessage = function(event) {
    if (location == 2)
        log("SUCCESS: location was properly shadowed");
    else
        log("FAIL: location was not shadowed");

    log("DONE");
}
