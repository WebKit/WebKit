var sab = new SharedArrayBuffer(1);
var a = new Int8Array(sab);
var result = Atomics.store(a, 0, 1000);
if (result != 1000)
    throw "Error: bad result: " + result;

sab = new SharedArrayBuffer(4);
a = new Uint32Array(sab);
var result = Atomics.store(a, 0, 4000000000);
if (result != 4000000000)
    throw "Error: bad result: " + result;
