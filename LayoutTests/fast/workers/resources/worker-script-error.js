self.addEventListener("error", function(event) {
    self.postMessage("Worker saw: " + event.error);
});

throw "WorkerScriptError";
