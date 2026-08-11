
function test(v) {
    return v.byteLength;
}
noInline(test);
noDFG(test);

const buffer = new ArrayBuffer(16, { maxByteLength: 32 });
const view = new DataView(buffer, 8, 8);

globalThis.testLoopCount ??= 10000;
for (let i = 0; i < testLoopCount; i++) {
    if (i == testLoopCount - 1) {
        buffer.resize(4);
        let didThrow = false;
        try {
            test(view);
        } catch (e) {
            didThrow = true;
        }
        if (!didThrow)
            throw new Error("bad")
    } else {
        if (test(view) != 8)
            throw new Error("bad");
    }
}
