function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`FAIL: expected '${expected}' actual '${actual}'`);
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

shouldBeArray(Uint8Array.fromHex(""), []);
shouldBeArray(Uint8Array.fromHex("00"), [0]);
shouldBeArray(Uint8Array.fromHex("01"), [1]);
shouldBeArray(Uint8Array.fromHex("80"), [128]);
shouldBeArray(Uint8Array.fromHex("fe"), [254]);
shouldBeArray(Uint8Array.fromHex("FE"), [254]);
shouldBeArray(Uint8Array.fromHex("ff"), [255]);
shouldBeArray(Uint8Array.fromHex("FF"), [255]);
shouldBeArray(Uint8Array.fromHex("0001"), [0, 1]);
shouldBeArray(Uint8Array.fromHex("feff"), [254, 255]);
shouldBeArray(Uint8Array.fromHex("FEFF"), [254, 255]);
shouldBeArray(Uint8Array.fromHex("000180feff"), [0, 1, 128, 254, 255]);
shouldBeArray(Uint8Array.fromHex("000180FEFF"), [0, 1, 128, 254, 255]);

{
    let expected = ''
    let buffer = new Uint8Array(16 * 1024 + 15);
    for (let i = 0; i < buffer.length; ++i) {
        buffer[i] = i & 0xff;
        expected += (i & 0xff).toString(16).padStart(2, '0');
    }
    shouldBeArray(Uint8Array.fromHex(expected), buffer);
}
{
    let expected = ''
    let buffer = new Uint8Array(15);
    for (let i = 0; i < buffer.length; ++i) {
        buffer[i] = i & 0xff;
        expected += (i & 0xff).toString(16).padStart(2, '0');
    }
    shouldBeArray(Uint8Array.fromHex(expected), buffer);
}


for (let invalid of [undefined, null, false, true, 42, {}, []]) {
    shouldThrow(() => {
        Uint8Array.fromHex(invalid);
    }, TypeError);
}

for (let invalid of ["0", "012", "0g", "0G", "g0", "G0", "0✅", "✅0", "00000000000000000000000000000000✅0", "00000000000000000000000000000000000000✅0", "00000000000000000000000000000000v0", "00000000000000000000000000000000000000g0"]) {
    shouldThrow(() => {
        Uint8Array.fromHex(invalid);
    }, SyntaxError);
}
