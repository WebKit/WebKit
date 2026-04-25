var a1 = new Uint8Array(1024 * 1024 * 1);
var a2 = new Uint16Array(1024 * 1024 * 1);
var a3 = new Uint32Array(1024 * 1024 * 1);
var a4 = new Float64Array(1024 * 1024 * 1);

for (var i = 0; i < 3e2; ++i) {
    a1.fill(99);
    a2.fill(99);
    a3.fill(99);
    a4.fill(99);
}
