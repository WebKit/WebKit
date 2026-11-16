
function test(v) {
    return v.byteLength;
}
noInline(test);

const b = new ArrayBuffer(16);
const v = new DataView(b);

globalThis.testLoopCount ??= 10000;
for (let i = 0; i < testLoopCount; i++) {
    if (test(v) != 16)
        throw new Error("bad");
}
