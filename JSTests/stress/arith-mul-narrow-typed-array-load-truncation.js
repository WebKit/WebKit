function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' expected: ' + expected);
}

function hash(array, scale) {
    var sum = 0;
    for (var i = 0; i < array.length; i++)
        sum ^= (i * scale * array[i]) | 0;
    return sum;
}
noInline(hash);

function referenceHash(array, scale) {
    var sum = 0;
    for (var i = 0; i < array.length; i++)
        sum ^= Number(BigInt.asIntN(32, BigInt(i * scale) * BigInt(array[i])));
    return sum;
}

function mulByLoad(array, index, value) {
    return (value * array[index]) | 0;
}
noInline(mulByLoad);

var uint8 = new Uint8Array(64);
var int16 = new Int16Array(64);
for (var i = 0; i < 64; ++i) {
    uint8[i] = (i * 37) & 0xff;
    int16[i] = (i * 7919) & 0xffff;
}

var expectedProducts = [];
for (var i = 0; i < 64; ++i)
    expectedProducts.push(Number(BigInt.asIntN(32, 0x7fffffffn * BigInt(uint8[i]))));
var indexStrings = [];
for (var i = 0; i < 64; ++i)
    indexStrings.push(String(i));
var expectedUint8Hash = referenceHash(uint8, 3);
var expectedInt16Hash = referenceHash(int16, 30000001);
for (var i = 0; i < testLoopCount; ++i) {
    shouldBe(hash(uint8, 3), expectedUint8Hash);
    shouldBe(hash(int16, 30000001), expectedInt16Hash);
    shouldBe(mulByLoad(uint8, i & 63, 0x7fffffff), expectedProducts[i & 63]);
}

// A non-int32 index refines the load into a generic access even though only a Uint8Array was
// profiled. The product must then stay exact for arbitrary int32 elements.
function mulByLoadWithAnyIndex(array, index, value) {
    return (value * array[index]) | 0;
}
noInline(mulByLoadWithAnyIndex);

for (var i = 0; i < testLoopCount; ++i) {
    shouldBe(mulByLoadWithAnyIndex(uint8, i & 63, 3), (3 * uint8[i & 63]) | 0);
    shouldBe(mulByLoadWithAnyIndex(uint8, indexStrings[i & 63], 3), (3 * uint8[i & 63]) | 0);
}

var wide = [0x7fffffff, -0x80000000, 0x12345678];
for (var i = 0; i < testLoopCount; ++i) {
    var index = i % 3;
    shouldBe(mulByLoadWithAnyIndex(wide, index, 0x7fffffff), (0x7fffffff * wide[index]) | 0);
}
