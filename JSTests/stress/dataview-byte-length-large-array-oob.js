//@skip if $memoryLimited
function test(v) {
    return v.byteLength;
}
noInline(test);

let oneGiga = 1024 * 1024 * 1024;
let twoGigs = 2 * oneGiga;
let fourGigs = 4 * oneGiga;

const buffer = new ArrayBuffer(fourGigs, { maxByteLength: fourGigs });
const view = new DataView(buffer, 2, twoGigs);

globalThis.testLoopCount ??= 10000;
for (let i = 0; i < testLoopCount; i++) {
    if (i == testLoopCount - 1) {
        buffer.resize(1);
        let didThrow = false;
        try {
            test(view);
        } catch (e) {
            didThrow = true;
        }
        if (!didThrow)
            throw new Error("bad")
    } else {
        if (test(view) != twoGigs)
            throw new Error("bad");
    }
}
