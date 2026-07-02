importScripts("worker-resources.js");

onmessage = function(event) {
    var memory = event.data;
    var didStartIdx = 0;
    var shouldGoIdx = 1;
    var didEndIdx = 2;
    
    postMessage("Started!");
    postMessage("Memory: " + memory);
    
    Atomics.store(memory, didStartIdx, 1);
    wake(memory, didStartIdx);

    wait(memory, shouldGoIdx, 0, 1);
    
    Atomics.store(memory, didEndIdx, 1);
    wake(memory, didEndIdx, 1);

    postMessage("Memory: " + memory);
    postMessage("done");
}
