function getInt64(dv, byteLength) {
    let last = 0n;
    for (let i = 0; i < byteLength; i += 8)
        last = dv.getBigInt64(i, true);
    return last;
}
noInline(getInt64);

function getUint64(dv, byteLength) {
    let last = 0n;
    for (let i = 0; i < byteLength; i += 8)
        last = dv.getBigUint64(i, false);
    return last;
}
noInline(getUint64);

const size = 16 * 1024;
let dv = new DataView(new ArrayBuffer(size));
for (let i = 0; i < size; i += 4)
    dv.setUint32(i, i * 2654435761, true);

let acc = 0n;
for (let i = 0; i < 4e3; ++i) {
    acc = getInt64(dv, size);
    acc = getUint64(dv, size);
}
if (typeof acc !== "bigint")
    throw new Error("bad result");
