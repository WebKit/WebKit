// Make a SharedWorker that has the same external interface as a DedicatedWorker, to use in shared test code.
function createWorker(url)
{
    var worker = new SharedWorker(url, url);
    worker.port.onmessage = function(event) { worker.onmessage(event); };
    worker.postMessage = function(message, port) { worker.port.postMessage(message, port); };
    return worker;
}
