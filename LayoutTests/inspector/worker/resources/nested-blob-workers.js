// Starts a chain of three nested blob workers. Calls `callback` with the blob URL of each level once all have started.
function startNestedBlobWorkers(callback) {
    function blobWorkerSource(level, childSource) {
        let source = `self.postMessage({level: ${level}, url: self.location.href});\n`;
        if (childSource) {
            source += `let child = new Worker(URL.createObjectURL(new Blob([${JSON.stringify(childSource)}], {type: "application/javascript"})));\n`;
            source += `child.onmessage = (event) => { self.postMessage(event.data); };\n`;
        }
        return source;
    }

    let source = blobWorkerSource(3, null);
    source = blobWorkerSource(2, source);
    source = blobWorkerSource(1, source);

    let urls = {};
    let worker = new Worker(URL.createObjectURL(new Blob([source], {type: "application/javascript"})));
    worker.onmessage = (event) => {
        urls[event.data.level] = event.data.url;
        if (Object.keys(urls).length === 3)
            callback(urls);
    };
    return worker;
}
