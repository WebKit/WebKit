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

function shouldThrow(callback, errorConstructor) {
    try {
        callback();
    } catch (e) {
        shouldBe(e instanceof errorConstructor, true);
        return
    }
    throw new Error('FAIL: should have thrown');
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
test("000180FEFF33CC✅0✅0✅0✅0", {read: 10, written: 5}, [0, 1, 128, 254, 255]);
test("000180FEFF33CCgggggggg", {read: 10, written: 5}, [0, 1, 128, 254, 255]);

function testLarge(input, expectedResult, expectedValue) {
    let uint8array = new Uint8Array(1024);
    for (let i = 0; i < uint8array.length; ++i)
        uint8array[i] = 42;

    let result = uint8array.setFromHex(input);

    shouldBeObject(result, expectedResult);
    let i = 0;
    for (; i < expectedValue.length; ++i)
        shouldBe(uint8array[i], expectedValue[i]);
    for (; i < uint8array.length; ++i)
        shouldBe(uint8array[i], 42);
}

testLarge("", {read: 0, written: 0}, []);
testLarge("00", {read: 2, written: 1}, [0, 42, 42, 42, 42]);
testLarge("01", {read: 2, written: 1}, [1, 42, 42, 42, 42]);
testLarge("80", {read: 2, written: 1}, [128, 42, 42, 42, 42]);
testLarge("fe", {read: 2, written: 1}, [254, 42, 42, 42, 42]);
testLarge("FE", {read: 2, written: 1}, [254, 42, 42, 42, 42]);
testLarge("ff", {read: 2, written: 1}, [255, 42, 42, 42, 42]);
testLarge("FF", {read: 2, written: 1}, [255, 42, 42, 42, 42]);
testLarge("0001", {read: 4, written: 2}, [0, 1, 42, 42, 42]);
testLarge("feff", {read: 4, written: 2}, [254, 255, 42, 42, 42]);
testLarge("FEFF", {read: 4, written: 2}, [254, 255, 42, 42, 42]);
testLarge("000180feff", {read: 10, written: 5}, [0, 1, 128, 254, 255]);
testLarge("000180FEFF", {read: 10, written: 5}, [0, 1, 128, 254, 255]);
testLarge("000180feff33", {read: 12, written: 6}, [0, 1, 128, 254, 255, 51]);
testLarge("000180FEFF33", {read: 12, written: 6}, [0, 1, 128, 254, 255, 51]);
testLarge("000180feff33cc", {read: 14, written: 7}, [0, 1, 128, 254, 255, 51, 204]);
testLarge("000180FEFF33CC", {read: 14, written: 7}, [0, 1, 128, 254, 255, 51, 204]);
testLarge($vm.make16BitStringIfPossible(""), {read: 0, written: 0}, []);
testLarge($vm.make16BitStringIfPossible("00"), {read: 2, written: 1}, [0, 42, 42, 42, 42]);
testLarge($vm.make16BitStringIfPossible("01"), {read: 2, written: 1}, [1, 42, 42, 42, 42]);
testLarge($vm.make16BitStringIfPossible("80"), {read: 2, written: 1}, [128, 42, 42, 42, 42]);
testLarge($vm.make16BitStringIfPossible("fe"), {read: 2, written: 1}, [254, 42, 42, 42, 42]);
testLarge($vm.make16BitStringIfPossible("FE"), {read: 2, written: 1}, [254, 42, 42, 42, 42]);
testLarge($vm.make16BitStringIfPossible("ff"), {read: 2, written: 1}, [255, 42, 42, 42, 42]);
testLarge($vm.make16BitStringIfPossible("FF"), {read: 2, written: 1}, [255, 42, 42, 42, 42]);
testLarge($vm.make16BitStringIfPossible("0001"), {read: 4, written: 2}, [0, 1, 42, 42, 42]);
testLarge($vm.make16BitStringIfPossible("feff"), {read: 4, written: 2}, [254, 255, 42, 42, 42]);
testLarge($vm.make16BitStringIfPossible("FEFF"), {read: 4, written: 2}, [254, 255, 42, 42, 42]);
testLarge($vm.make16BitStringIfPossible("000180feff"), {read: 10, written: 5}, [0, 1, 128, 254, 255]);
testLarge($vm.make16BitStringIfPossible("000180FEFF"), {read: 10, written: 5}, [0, 1, 128, 254, 255]);
testLarge($vm.make16BitStringIfPossible("000180feff33"), {read: 12, written: 6}, [0, 1, 128, 254, 255, 51]);
testLarge($vm.make16BitStringIfPossible("000180FEFF33"), {read: 12, written: 6}, [0, 1, 128, 254, 255, 51]);
testLarge($vm.make16BitStringIfPossible("000180feff33cc"), {read: 14, written: 7}, [0, 1, 128, 254, 255, 51, 204]);
testLarge($vm.make16BitStringIfPossible("000180FEFF33CC"), {read: 14, written: 7}, [0, 1, 128, 254, 255, 51, 204]);
shouldThrow(() => {
    testLarge("000180FEFF33CC✅0✅0✅0✅0", {read: 10, written: 5}, [0, 1, 128, 254, 255]);
}, SyntaxError);
shouldThrow(() => {
    testLarge("000180FEFF33CCgggggggg", {read: 10, written: 5}, [0, 1, 128, 254, 255]);
}, SyntaxError);

testLarge("000180feff33cc0000000000000000000000000000aa00aa00aa00aa00aa00aa00", {read: 66, written: 33}, [0,1,128,254,255,51,204,0,0,0,0,0,0,0,0,0,0,0,0,0,0,170,0,170,0,170,0,170,0,170,0,170,0]);
testLarge("000180FEFF33CC0000000000000000000000000000aa00aa00aa00aa00aa00aa00", {read: 66, written: 33}, [0,1,128,254,255,51,204,0,0,0,0,0,0,0,0,0,0,0,0,0,0,170,0,170,0,170,0,170,0,170,0,170,0]);
testLarge($vm.make16BitStringIfPossible("000180feff33cc0000000000000000000000000000aa00aa00aa00aa00aa00aa00"), {read: 66, written: 33}, [0,1,128,254,255,51,204,0,0,0,0,0,0,0,0,0,0,0,0,0,0,170,0,170,0,170,0,170,0,170,0,170,0]);
testLarge($vm.make16BitStringIfPossible("000180FEFF33CC0000000000000000000000000000aa00aa00aa00aa00aa00aa00"), {read: 66, written: 33}, [0,1,128,254,255,51,204,0,0,0,0,0,0,0,0,0,0,0,0,0,0,170,0,170,0,170,0,170,0,170,0,170,0]);

shouldThrow(() => {
    testLarge($vm.make16BitStringIfPossible("000180FEFF33CC✅0✅0✅0✅0"), {read: 10, written: 5}, [0, 1, 128, 254, 255]);
}, SyntaxError);
shouldThrow(() => {
    testLarge($vm.make16BitStringIfPossible("000180FEFF33CCgggggggg"), {read: 10, written: 5}, [0, 1, 128, 254, 255]);
}, SyntaxError);

shouldThrow(() => {
    let uint8array = new Uint8Array;
    $.detachArrayBuffer(uint8array.buffer);
    uint8array.setFromHex("");
}, TypeError);

for (let invalid of [undefined, null, false, true, 42, {}, []]) {
    shouldThrow(() => {
        (new Uint8Array).setFromHex(invalid);
    }, TypeError);
}

for (let invalid of ["0", "012", "0g", "0G", "g0", "G0", "0✅", "✅0"]) {
    shouldThrow(() => {
        (new Uint8Array([42, 42, 42, 42, 42])).setFromHex(invalid);
    }, SyntaxError);
}
