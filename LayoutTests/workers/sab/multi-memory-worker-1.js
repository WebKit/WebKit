importScripts("worker-resources.js");

onmessage = function (event) {
    var didStart = event.data.didStart;
    var shouldGo = event.data.shouldGo;
    var didEnd = event.data.didEnd;

    checkBufferSharing(event.data.shouldShareBuffer, didStart, shouldGo, didEnd);
    
    postMessage("Started!");
    postMessage("didStart: " + didStart);
    postMessage("shouldGo: " + shouldGo);
    postMessage("didEnd: " + didEnd);
    
    wait(didStart, 0, 0, 1);
    
    postMessage("It started!");
    
    shouldGo[0] = 1;
    wake(shouldGo, 0);
    
    wait(didEnd, 0, 0, 1);
    
    postMessage("All done!");
    postMessage("didStart: " + didStart);
    postMessage("shouldGo: " + shouldGo);
    postMessage("didEnd: " + didEnd);
    postMessage("done");
}
