function shouldBe(actual, expected) {
    if (actual !== expected) throw new Error("bad value: " + actual);
}

function shouldThrow(func, errorMessage) {
    var errorThrown = false;
    var error = null;
    try {
        func();
    } catch (e) {
        errorThrown = true;
        error = e;
    }
    if (!errorThrown) {
        throw new Error("not thrown");
    }
    if (String(error) !== errorMessage) {
        throw new Error(`bad error: ${String(error)}`);
    }
}

var array = new Uint8Array([0, 1, 2, 3, 4, 5, 6, 7, 0x80, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10]);
var dataView = new DataView(array.buffer);

// values in the range (-1, 0], i.e., 0 inclusive, -1 exclusive
var values0ToMinus1Exclusive = [0, -Number.EPSILON, -0.1, -0.9, -1 + Number.EPSILON];

shouldBe(dataView.getBigInt64(8), -9223088349902469625n);
shouldBe(dataView.getBigUint64(8), 9223655723807081991n);
shouldBe(dataView.getBigInt64(11), 217304205466536202n);
shouldBe(dataView.getBigUint64(11), 217304205466536202n);
shouldThrow(() => {
    dataView.getBigInt64(12);
}, "RangeError: Out of bounds access");
shouldThrow(() => {
    dataView.getBigUint64(12);
}, "RangeError: Out of bounds access");
for (let val of values0ToMinus1Exclusive) {
    shouldBe(dataView.getBigInt64(val), 0x01020304050607n);
    shouldBe(dataView.getBigUint64(val), 0x01020304050607n);
}

shouldBe(dataView.setBigInt64(0, -1n), undefined);
for (let innerVal of values0ToMinus1Exclusive) {
    shouldBe(dataView.getBigInt64(innerVal), -1n);
    shouldBe(dataView.getBigUint64(innerVal), 0xffffffffffffffffn);
}

for (let outerVal of values0ToMinus1Exclusive) {
    shouldBe(dataView.setBigUint64(outerVal, 0xfffffffffffffffen), undefined);
    shouldBe(dataView.getBigInt64(0), -2n);
    shouldBe(dataView.getBigUint64(0), 0xfffffffffffffffen);
    shouldBe(dataView.getUint8(0), 0xff);
    for (let innerVal of values0ToMinus1Exclusive) {
        shouldBe(dataView.getBigInt64(innerVal), -2n);
        shouldBe(dataView.getBigUint64(innerVal), 0xfffffffffffffffen);
    }
}

for (let outerVal of values0ToMinus1Exclusive) {
    shouldBe(dataView.setBigUint64(outerVal, 0x1fffffffffffffffen), undefined);
    shouldBe(dataView.getBigInt64(0), -2n);
    shouldBe(dataView.getBigUint64(0), 0xfffffffffffffffen);
    shouldBe(dataView.getUint8(0), 0xff);
    for (let innerVal of values0ToMinus1Exclusive) {
        shouldBe(dataView.getBigInt64(innerVal), -2n);
        shouldBe(dataView.getBigUint64(innerVal), 0xfffffffffffffffen);
    }
}
