onmessage = function(event) {
    // This quickly creates a need for GC.
    postMessage(event.data + event.data);
}

// Trigger the ping-pong.
postMessage("kaboom?");
