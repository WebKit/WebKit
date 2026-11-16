const worker = new Worker("./offscreen-nested-worker.js");
onmessage = function(e) {
    worker.postMessage({canvas: e.data.canvas}, [e.data.canvas]);
}
worker.onmessage = function(e) {
    self.postMessage("done");
}