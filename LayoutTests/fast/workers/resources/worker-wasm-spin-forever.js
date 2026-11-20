const wasmBytes = new Uint8Array([
    0, 97, 115, 109, 1, 0, 0, 0, 1, 4, 1, 96, 0, 0, 3, 2, 1, 0, 7, 7, 1, 3, 114, 117, 110, 0, 0, 10, 9, 1, 7, 0, 3, 64, 12, 0, 11, 11
]);
WebAssembly.instantiate(wasmBytes).then(({ instance }) => {
    self.postMessage("ready");
    instance.exports.run();
});
