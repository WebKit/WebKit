function setInt64(dv, byteLength, values) {
    for (let i = 0; i < byteLength; i += 8)
        dv.setBigInt64(i, values[(i >> 3) & 3], true);
}
noInline(setInt64);

function setUint64(dv, byteLength, values) {
    for (let i = 0; i < byteLength; i += 8)
        dv.setBigUint64(i, values[(i >> 3) & 3], false);
}
noInline(setUint64);

const size = 16 * 1024;
let dv = new DataView(new ArrayBuffer(size));
let values = [0x0102030405060708n, -0x7fedcba987654321n, (1n << 64n) + 5n, -1n];

for (let i = 0; i < 4e3; ++i) {
    setInt64(dv, size, values);
    setUint64(dv, size, values);
}
if (dv.getBigUint64(0, false) !== 0x0102030405060708n)
    throw new Error("bad result");
