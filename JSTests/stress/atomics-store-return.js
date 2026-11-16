var sab = new SharedArrayBuffer(1);
var a = new Int8Array(sab);
var result = Atomics.store(a, 0, 1000);
if (result != 1000)
    throw new Error("bad result: " + result);

sab = new SharedArrayBuffer(4);
a = new Uint32Array(sab);
result = Atomics.store(a, 0, 4000000000);
if (result != 4000000000)
    throw new Error("bad result: " + result);
if (a[0] != 4000000000)
    throw new Error("bad value read back: " + a[0]);
result = Atomics.store(a, 0, -4000000000);
if (result != -4000000000)
    throw new Error("bad result: " + result);
if (a[0] != 294967296)
    throw new Error("bad value read back: " + a[0]);

var count = 0;
result = Atomics.store(a, 0, { valueOf() { count++; return 42; } });
if (result != 42)
    throw new Error("bad result: " + result);
if (count != 1)
    throw new Error("bad count: " + count);

