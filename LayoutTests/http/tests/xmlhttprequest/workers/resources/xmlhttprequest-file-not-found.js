function log(message)
{
    postMessage("log " + message);
}

function done()
{
    postMessage("DONE");
}

req = new XMLHttpRequest;
try {
    req.open("GET", "missing-file", false);
    req.send();
} catch (e) {
    log("Exception received.");
}
done();
