var databaseWorker = new Worker('resources/database-worker.js');

databaseWorker.onmessage = function(event) {
    if (event.data.indexOf('log:') == 0)
        log(event.data.substring(4));
    else if (event.data == 'notifyDone') {
        if (window.layoutTestController)
                layoutTestController.notifyDone();
    } else
        throw new Error("Unrecognized message: " + event);
}

function log(message)
{
    document.getElementById("console").innerText += message + "\n";
}
