var sab = new SharedArrayBuffer(100 * 4);
var memory = new Int32Array(sab);
postMessage(memory);

