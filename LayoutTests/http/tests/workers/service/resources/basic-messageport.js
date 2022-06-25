var messages = [];
var registration;

async function doLog(message)
{
    messages.push(message)
    if (messages.length === 3) {
        messages.sort();
        for (let message of messages)
            log(message);
        channel.port2.close();
        if (registration)
            await registration.unregister();
        log("Test finished");
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

navigator.serviceWorker.register("resources/messageport-echo-worker.js", { }).then((r) => {
    registration = r;
    try {
        registration.installing.postMessage("Here is your port", [channel.port2]);
        channel.port1.postMessage("Message to a port!");
    } catch(e) {
        doLog("Exception: " + e);
        if (registration)
            registration.unregister();
        finishSWTest();
    }
});
