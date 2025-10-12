//@skip if $memoryLimited
function test(v) {
    return v.byteLength;
}
noInline(test);

let oneGiga = 1024 * 1024 * 1024;
let fourGigs = 4 * oneGiga;
const b = new ArrayBuffer(fourGigs);
const v = new DataView(b);

globalThis.testLoopCount ??= 10000;
for (let i = 0; i < testLoopCount; i++) {
    if (test(v) != fourGigs)
        throw new Error("bad");
}
