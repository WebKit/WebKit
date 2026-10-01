function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)}, expected: ${String(expected)}`);
}

function Collect()
{
    this.args = Array.prototype.slice.call(arguments);
}

const rareStride = Math.max(1, Math.floor(testLoopCount / 100));

let received;
let fake = {
    construct(target, list)
    {
        received = list;
        return "fake";
    }
};

function test(Reflect, a, b)
{
    return Reflect.construct(Collect, [a, b]);
}
noInline(test);

for (let i = 0; i < testLoopCount; ++i)
    shouldBe(test(Reflect, i, 1).args[0], i);

let setterCalls = 0;
Object.defineProperty(Array.prototype, 0, {
    get() { return "getter"; },
    set(value) { ++setterCalls; },
    configurable: true
});

for (let i = 0; i < testLoopCount; ++i) {
    let object = test(Reflect, i, 1);
    shouldBe(object.args.length, 2);
    shouldBe(object.args[0], i);
    shouldBe(object.args[1], 1);

    if (i % rareStride)
        continue;
    shouldBe(test(fake, i, 1), "fake");
    shouldBe(received.length, 2);
    shouldBe(received[0], i);
    shouldBe(received[1], 1);
    shouldBe(Object.hasOwn(received, 0), true);
}
shouldBe(setterCalls, 0);
