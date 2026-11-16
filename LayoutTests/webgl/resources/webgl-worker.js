const worker = new Worker("./webgl-nested-worker.js");
worker.addEventListener("message", (e) => {
    const result = [
        `WebGL supported in Worker: ${new OffscreenCanvas(1, 1).getContext("webgl") !== null}`,
        `WebGL2 supported in Worker: ${new OffscreenCanvas(1, 1).getContext("webgl2") !== null}`
    ];
    self.postMessage(result.concat(e.data));
});