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
    view[0] = 42;
    check(flag, ab);
    if (flag)
        return view;
    return ab;
}
noInline(test);

for (let i = 0; i < testLoopCount; ++i)
    shouldBe(test(false) instanceof ArrayBuffer, true);
let result = test(true);
shouldBe(result instanceof Uint8Array, true);
shouldBe(result.length, 0);
shouldBe(result.byteLength, 0);
shouldBe(result.byteOffset, 0);
shouldBe(result.buffer.detached, true);
