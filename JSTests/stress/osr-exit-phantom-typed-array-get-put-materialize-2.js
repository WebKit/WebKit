function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function identity(value)
{
    return value;
}
noInline(identity);

function test(flag, value, index)
{
    var ab = new ArrayBuffer(42);
    var view = new Uint8Array(ab);
    view[index] = value;
    var res = view[0];
    if (flag) {
        OSRExit();
        return identity(view);
    }
    return [ab, res];
}
noInline(test);

for (let i = 0; i < testLoopCount; ++i) {
    let result = test(false, 42, i & 0x1);
    shouldBe(result[0] instanceof ArrayBuffer, true);
    shouldBe(result[1], i & 0x1 ? 0 : 42);
}
let result = test(true);
shouldBe(result instanceof Uint8Array, true);
