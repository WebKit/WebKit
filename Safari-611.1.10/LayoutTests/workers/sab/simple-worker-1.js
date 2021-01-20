importScripts("worker-resources.js");

onmessage = function (event) {
    var memory = event.data;
    var didStartIdx = 0;
    var shouldGoIdx = 1;
    var didEndIdx = 2;
    
    postMessage("Started!");
    postMessage("Memory: " + memory);
    
    wait(memory, didStartIdx, 0, 1);
    
    postMessage("It started!");
    
    memory[shouldGoIdx] = 1;
    wake(memory, shouldGoIdx);
    
    wait(memory, didEndIdx, 0, 1);
    
    postMessage("All done!");
    postMessage("Memory: " + memory);
    postMessage("done");
}
