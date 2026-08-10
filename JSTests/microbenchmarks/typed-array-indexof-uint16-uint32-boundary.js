function testU16(array, target) {
    return array.indexOf(target);
}
noInline(testU16);

function testU32(array, target) {
    return array.indexOf(target);
}
noInline(testU32);

const lengths = [4, 8, 16, 32, 64, 128];

function makeU16(len) {
    const array = new Uint16Array(len);
    for (let i = 0; i < len; ++i)
        array[i] = 0x1000 + (i % 100);
    return array;
}

function makeU32(len) {
    const array = new Uint32Array(len);
    for (let i = 0; i < len; ++i)
        array[i] = 0x1000 + (i % 100);
    return array;
}

const arrays16 = lengths.map(makeU16);
const arrays32 = lengths.map(makeU32);
const missingTarget = 0xBEEF;

for (let i = 0; i < 1e5; ++i) {
    for (const array of arrays16)
        testU16(array, missingTarget);
    for (const array of arrays32)
        testU32(array, missingTarget);
}
