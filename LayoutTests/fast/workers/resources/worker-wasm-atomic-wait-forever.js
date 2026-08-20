const wasmBytes = new Uint8Array([
    0,97,115,109,1,0,0,0,1,4,1,96,0,0,2,16,1,3,101,110,118,6,109,101,109,111,114,121,2,3,1,1,3,2,1,0,7,7,1,3,114,117,110,0,0,10,27,1,25,0,3,64,65,0,65,0,54,2,0,65,0,65,0,66,127,254,1,2,0,26,12,0,11,11
]);
const memory = new WebAssembly.Memory({ initial: 1, maximum: 1, shared: true });
WebAssembly.instantiate(wasmBytes, { env: { memory } }).then(({ instance }) => {
    self.postMessage("ready");
    instance.exports.run();
});
