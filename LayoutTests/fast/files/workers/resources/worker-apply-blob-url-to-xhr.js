function log(message)
{
    postMessage(message);
}

onmessage = function(event)
{
    var file = event.data;
    var fileURL = createBlobURL(file);

    log("Test that XMLHttpRequest GET succeeds.");
    var xhr = new XMLHttpRequest();
    xhr.open("GET", fileURL, false);
    xhr.send();
    log(xhr.responseText);

    log("Test that XMLHttpRequest POST fails.");
    xhr = new XMLHttpRequest();
    xhr.open("POST", fileURL, false);
    try {
        xhr.send();
    } catch (error) {
        log("Received exception " + error.code + ": " + error.name);
    }
    log(xhr.responseText);

    log("DONE");
}
