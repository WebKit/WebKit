var messages = [];

function doLog(message)
{
    messages.push(message)
    if (messages.length === 3) {
        messages.sort();
        for (let message of messages)
            log(message);
        finishSWTest();
    }
}

navigator.serviceWorker.addEventListener("message", function(event) {
    doLog("Message received from ServiceWorker: " + event.data);
});

var channel = new MessageChannel;
channel.port1.onmessage = function(event) {
    doLog("Message received from MessagePort: " + event.data);
}

navigator.serviceWorker.register("resources/messageport-echo-worker.js", { }).then(function(registration) {
    try {
        registration.installing.postMessage("Here is your port", [channel.port2]);
        channel.port1.postMessage("Message to a port!");
    } catch(e) {
        doLog("Exception: " + e);
        finishSWTest();
    }
});
