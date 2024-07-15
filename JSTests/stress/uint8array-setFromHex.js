//@ requireOptions("--useUint8ArrayBase64Methods=1")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`FAIL: expected '${expected}' actual '${actual}'`);
}

function shouldBeObject(actual, expected) {
    for (let key in expected) {
        shouldBe(key in actual, true);
        shouldBe(actual[key], expected[key]);
    }

    for (let key in actual)
        shouldBe(key in expected, true);
}

function shouldBeArray(actual, expected) {
    shouldBe(actual.length, expected.length);
    for (let i = 0; i < expected.length; ++i)
        shouldBe(actual[i], expected[i]);
}

function test(input, expectedResult, expectedValue) {
    let uint8array = new Uint8Array([42, 42, 42, 42, 42]);

    let result = uint8array.setFromHex(input);

    shouldBeObject(result, expectedResult);
    shouldBeArray(uint8array, expectedValue);
}

test("", {read: 0, written: 0}, [42, 42, 42, 42, 42]);
test("00", {read: 2, written: 1}, [0, 42, 42, 42, 42]);
test("01", {read: 2, written: 1}, [1, 42, 42, 42, 42]);
test("80", {read: 2, written: 1}, [128, 42, 42, 42, 42]);
test("fe", {read: 2, written: 1}, [254, 42, 42, 42, 42]);
test("FE", {read: 2, written: 1}, [254, 42, 42, 42, 42]);
test("ff", {read: 2, written: 1}, [255, 42, 42, 42, 42]);
test("FF", {read: 2, written: 1}, [255, 42, 42, 42, 42]);
test("0001", {read: 4, written: 2}, [0, 1, 42, 42, 42]);
test("feff", {read: 4, written: 2}, [254, 255, 42, 42, 42]);
test("FEFF", {read: 4, written: 2}, [254, 255, 42, 42, 42]);
test("000180feff", {read: 10, written: 5}, [0, 1, 128, 254, 255]);
test("000180FEFF", {read: 10, written: 5}, [0, 1, 128, 254, 255]);
test("000180feff33", {read: 10, written: 5}, [0, 1, 128, 254, 255]);
test("000180FEFF33", {read: 10, written: 5}, [0, 1, 128, 254, 255]);
test("000180feff33cc", {read: 10, written: 5}, [0, 1, 128, 254, 255]);
test("000180FEFF33CC", {read: 10, written: 5}, [0, 1, 128, 254, 255]);

try {
    let uint8array = new Uint8Array;
    $.detachArrayBuffer(uint8array.buffer);
    uint8array.setFromHex("");
} catch (e) {
    shouldBe(e instanceof TypeError, true);
}

for (let invalid of [undefined, null, false, true, 42, {}, []]) {
    try {
        (new Uint8Array).setFromHex(invalid);
    } catch (e) {
        shouldBe(e instanceof TypeError, true);
    }
}

for (let invalid of ["0", "012", "0g", "g0", "0✅", "✅0"]) {
    try {
        (new Uint8Array).setFromHex(invalid);
    } catch (e) {
        shouldBe(e instanceof SyntaxError, true);
    }
}
