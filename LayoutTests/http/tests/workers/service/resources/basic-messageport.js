navigator.serviceWorker.addEventListener("message", function(event) {
    log("Message received from ServiceWorker: " + event.data);
});

var channel = new MessageChannel;
channel.port1.onmessage = function(event) {
	log("Message received from MessagePort: " + event.data);
	finishSWTest();
}

navigator.serviceWorker.register("resources/messageport-echo-worker.js", { }).then(function(registration) {
	try {
    	registration.installing.postMessage("Here is your port", [channel.port2]);
        channel.port1.postMessage("Message to a port!");
	} catch(e) {
		log("Exception: " + e);
		finishSWTest();
	}
});
