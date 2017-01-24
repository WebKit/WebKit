var sab = new SharedArrayBuffer(4);
var a = new Int32Array(sab);
Atomics.add(a, -0, 1); // This should not throw.
