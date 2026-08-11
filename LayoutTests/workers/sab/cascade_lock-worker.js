importScripts("worker-resources.js");

onmessage = function (event) {
    var memory = event.data;
    var workPerCriticalSection = memory[1];
    var workBetweenCriticalSections = memory[2];
    
    var doubleMemory = new Float64Array(memory.buffer);
    
    var lockIdx = 0;
    var countIdx = 2;
    var count = 10000000;
    
    postMessage("Started!");
    postMessage("Memory: " + memory);
    
    var value = 1;
    var localWord = 0;
    for (var i = 0; i < count; ++i) {
        cascadeLock(memory, lockIdx);
        for (var j = workPerCriticalSection; j--;) {
            doubleMemory[countIdx] += value;
        }
        cascadeUnlock(memory, lockIdx);
        for (var j = workBetweenCriticalSections; j--;) {
            localWord += value;
            value = localWord;
        }
    }
    
    postMessage("All done!");
    postMessage("Memory: " + memory);
    postMessage("done");
}
