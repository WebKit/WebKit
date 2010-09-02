function log(message)
{
    postMessage(message);
}

function sendXMLHttpRequest(method, url)
{
    var xhr = new XMLHttpRequest();
    xhr.open(method, url, false);
    try {
        xhr.send();
        log("Status: " + xhr.status);
        log("Response: " + xhr.responseText);
    } catch (error) {
        log("Received exception " + error.code + ": " + error.name);
    }
}

onmessage = function(event)
{
    var file = event.data;
    var fileURL = createBlobURL(file);

    log("Test that XMLHttpRequest GET succeeds.");
    sendXMLHttpRequest("GET", fileURL);

    log("Test that XMLHttpRequest POST fails.");
    xhr = new XMLHttpRequest();
    sendXMLHttpRequest("POST", fileURL);

    log("Test that XMLHttpRequest GET fails after the blob URL is revoked.");
    revokeBlobURL(fileURL);
    sendXMLHttpRequest("GET", fileURL);

    log("DONE");
}
