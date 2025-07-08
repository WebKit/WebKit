function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function check(flag, ab)
{
    if (flag)
        ab.transfer();
}
noInline(check);

function test(flag, val, index)
{
    var ab = new ArrayBuffer(42);
    var view = new Uint8Array(ab);
    view[index] = val;
    var res = view[0];
    check(flag, ab);
    if (flag) {
        OSRExit();
        return view;
    }
    return [ab, res];
}
noInline(test);

for (let i = 0; i < testLoopCount; ++i) {
    let result = test(false, 42, i & 0x1);
    shouldBe(result[0] instanceof ArrayBuffer, true);
    shouldBe(result[1], i & 0x1 ? 0 : 42);
}
let result = test(true, 42);
shouldBe(result instanceof Uint8Array, true);
shouldBe(result.length, 0);
shouldBe(result.byteLength, 0);
shouldBe(result.byteOffset, 0);
shouldBe(result.buffer.detached, true);
