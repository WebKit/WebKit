onconnect = function(event) {
    if (event.__proto__ === MessageEvent.prototype)
        event.ports[0].postMessage("PASS: event.__proto__ is MessageEvent.prototype");
    else
        event.ports[0].postMessage("FAIL: event.__proto__ is not MessageEvent.prototype");
    if (event.source === event.ports[0])
        event.ports[0].postMessage("PASS: event.source is event.ports[0]");
    else
        event.ports[0].postMessage("FAIL: event.source is not event.ports[0]");
    event.ports[0].postMessage("DONE:");
};
