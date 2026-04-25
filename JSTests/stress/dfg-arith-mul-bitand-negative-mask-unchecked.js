function testMaskRight(a, b) {
    return ((a & -4) * b) | 0;
}
noInline(testMaskRight);

function testMaskLeft(a, b) {
    return ((-4 & a) * b) | 0;
}
noInline(testMaskLeft);

function testMaskRightSwappedMul(a, b) {
    return (b * (a & -4)) | 0;
}
noInline(testMaskRightSwappedMul);

let a = 0x7ffffffc;
let b = 0x7ffffffc;
let expected = 0;

for (let i = 0; i < testLoopCount; ++i) {
    let r1 = testMaskRight(a, b);
    if (r1 !== expected)
        throw new Error("testMaskRight: expected " + expected + " but got " + r1 + " at iteration " + i);
    let r2 = testMaskLeft(a, b);
    if (r2 !== expected)
        throw new Error("testMaskLeft: expected " + expected + " but got " + r2 + " at iteration " + i);
    let r3 = testMaskRightSwappedMul(a, b);
    if (r3 !== expected)
        throw new Error("testMaskRightSwappedMul: expected " + expected + " but got " + r3 + " at iteration " + i);
}
