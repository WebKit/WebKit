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

function test(flag)
{
    var ab = new ArrayBuffer(42);
    var view = new Uint8Array(ab);
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
    let result = test(false);
    shouldBe(result[0] instanceof ArrayBuffer, true);
    shouldBe(result[1], 0);
}
let result = test(true);
shouldBe(result instanceof Uint8Array, true);
shouldBe(result.length, 0);
shouldBe(result.byteLength, 0);
shouldBe(result.byteOffset, 0);
shouldBe(result.buffer.detached, true);
