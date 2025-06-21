function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function identity(value)
{
    return value;
}
noInline(identity);

function test(flag)
{
    var ab = new ArrayBuffer(42);
    var view = new Uint8Array(ab);
    view[0] = 42;
    if (flag) {
        OSRExit();
        return identity(view);
    }
    return ab;
}
noInline(test);

for (let i = 0; i < testLoopCount; ++i)
    shouldBe(test(false) instanceof ArrayBuffer, true);
let result = test(true);
shouldBe(result instanceof Uint8Array, true);
