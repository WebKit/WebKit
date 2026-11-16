
const b = new ArrayBuffer(16);
const v1 = new DataView(b);
const v2 = new DataView(b);
const v3 = new DataView(b);
const v4 = new DataView(b);

function test(v1, v2, v3, v4) {
    return v1.byteLength + v2.byteLength + v3.byteLength + v4.byteLength;
}
noInline(test);


for (let i = 0; i < 1e6; i++) {
    if (test(v1, v2, v3, v4) != 16 * 4)
        throw new Error("bad");
}
