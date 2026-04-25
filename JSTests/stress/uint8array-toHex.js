function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`FAIL: expected '${expected}' actual '${actual}'`);
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

shouldBe((new Uint8Array([])).toHex(), "");
shouldBe((new Uint8Array([0])).toHex(), "00");
shouldBe((new Uint8Array([1])).toHex(), "01");
shouldBe((new Uint8Array([128])).toHex(), "80");
shouldBe((new Uint8Array([254])).toHex(), "fe");
shouldBe((new Uint8Array([255])).toHex(), "ff");
shouldBe((new Uint8Array([0, 1])).toHex(), "0001");
shouldBe((new Uint8Array([254, 255])).toHex(), "feff");
shouldBe((new Uint8Array([0, 1, 128, 254, 255])).toHex(), "000180feff");

{
    let expected = ''
    let buffer = new Uint8Array(16 * 1024);
    for (let i = 0; i < buffer.length; ++i) {
        buffer[i] = i & 0xff;
        expected += (i & 0xff).toString(16).padStart(2, '0');
    }
    shouldBe(buffer.toHex(), expected);
}
{
    let expected = ''
    let buffer = new Uint8Array(15);
    for (let i = 0; i < buffer.length; ++i) {
        buffer[i] = i & 0xff;
        expected += (i & 0xff).toString(16).padStart(2, '0');
    }
    shouldBe(buffer.toHex(), expected);
}

shouldThrow(() => {
    let uint8array = new Uint8Array;
    $.detachArrayBuffer(uint8array.buffer);
    uint8array.toHex();
}, TypeError);
