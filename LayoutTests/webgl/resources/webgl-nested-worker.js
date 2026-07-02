const result = [
    `WebGL supported in nested Worker: ${new OffscreenCanvas(1, 1).getContext("webgl") !== null}`,
    `WebGL2 supported in nested Worker: ${new OffscreenCanvas(1, 1).getContext("webgl2") !== null}`,
];
self.postMessage(result);