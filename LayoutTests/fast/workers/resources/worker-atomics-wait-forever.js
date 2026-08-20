const sab = new SharedArrayBuffer(4);
const view = new Int32Array(sab);
self.postMessage("ready");
Atomics.wait(view, 0, 0);
