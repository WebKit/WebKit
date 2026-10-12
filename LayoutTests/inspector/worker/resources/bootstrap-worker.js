let values = {
    valueFromBootstrapScript: self.valueFromBootstrapScript,
    executionCount: self.bootstrapScriptExecutionCount || 0,
};

self.addEventListener("message", (event) => {
    let {type, depth} = event.data;
    if (!depth) {
        self.postMessage([values]);
        return;
    }

    let worker = new Worker(type === "module" ? "bootstrap-worker-module.js" : "bootstrap-worker.js", {type});
    worker.addEventListener("message", (event) => {
        worker.terminate();
        self.postMessage([values, ...event.data]);
    }, {once: true});
    worker.postMessage({type, depth: depth - 1});
});
