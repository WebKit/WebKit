function byteSwap64(dv, byteLength) {
    for (let i = 0; i < byteLength; i += 8)
        dv.setBigUint64(i, dv.getBigUint64(i, false), true);
}
noInline(byteSwap64);

const size = 16 * 1024;
let dv = new DataView(new ArrayBuffer(size));
for (let i = 0; i < size; ++i)
    dv.setUint8(i, (i * 37 + 11) & 255);

for (let i = 0; i < 4e3; ++i)
    byteSwap64(dv, size);

for (let i = 0; i < size; ++i) {
    if (dv.getUint8(i) !== ((i * 37 + 11) & 255))
        throw new Error("bad result");
}
