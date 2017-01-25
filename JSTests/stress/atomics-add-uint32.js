var sab = new SharedArrayBuffer(4);
var a = new Uint32Array(sab);
var result = Atomics.add(a, 0, 4000000000);
if (result != 0)
    throw new Error("bad result: " + result);
if (a[0] != 4000000000)
    throw new Error("bad value read back: " + a[0]);
result = Atomics.sub(a, 0, 4000000000);
if (result != 4000000000)
    throw new Error("bad result: " + result);
if (a[0] != 0)
    throw new Error("bad value read back: " + a[0]);

