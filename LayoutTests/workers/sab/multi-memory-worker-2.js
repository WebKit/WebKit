importScripts("worker-resources.js");

onmessage = function(event) {
    var didStart = event.data.didStart;
    var shouldGo = event.data.shouldGo;
    var didEnd = event.data.didEnd;

    checkBufferSharing(event.data.shouldShareBuffer, didStart, shouldGo, didEnd);
    
    postMessage("Started!");
    postMessage("didStart: " + didStart);
    postMessage("shouldGo: " + shouldGo);
    postMessage("didEnd: " + didEnd);
    
    Atomics.store(didStart, 0, 1);
    wake(didStart, 0);

    wait(shouldGo, 0, 0, 1);
    
    Atomics.store(didEnd, 0, 1);
    wake(didEnd, 0, 1);

    postMessage("didStart: " + didStart);
    postMessage("shouldGo: " + shouldGo);
    postMessage("didEnd: " + didEnd);
    postMessage("done");
}
