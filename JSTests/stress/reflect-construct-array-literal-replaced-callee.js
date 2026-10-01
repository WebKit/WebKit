function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)}, expected: ${String(expected)}`);
}

function Collect()
{
    this.args = Array.prototype.slice.call(arguments);
    this.newTarget = new.target;
}

function Other() { }

let received = [];
let fake = {
    construct(target, list, newTarget)
    {
        received.push({ self: this, target, list, newTarget, count: arguments.length });
        return "fake";
    }
};

function none(Reflect)
{
    return Reflect.construct(Collect, []);
}
noInline(none);

function three(Reflect, a)
{
    return Reflect.construct(Collect, [a, 1.5, "c"]);
}
noInline(three);

function withNewTarget(Reflect, a)
{
    return Reflect.construct(Collect, [a, { a }], Other, "extra0", "extra1");
}
noInline(withNewTarget);

function many(Reflect, a)
{
    return Reflect.construct(Collect, [a, a + 1, a + 2, a + 3, a + 4, a + 5, a + 6, a + 7, a + 8, a + 9, a + 10, a + 11, a + 12, a + 13, a + 14, a + 15, a + 16]);
}
noInline(many);

function checkList(list, length)
{
    shouldBe(Array.isArray(list), true);
    shouldBe(Object.getPrototypeOf(list), Array.prototype);
    shouldBe(list.length, length);
    shouldBe(Object.keys(list).length, length);
    for (let i = 0; i < length; ++i) {
        let descriptor = Object.getOwnPropertyDescriptor(list, i);
        shouldBe(descriptor.writable, true);
        shouldBe(descriptor.enumerable, true);
        shouldBe(descriptor.configurable, true);
    }
    list.push("pushed");
    shouldBe(list.length, length + 1);
}

let previous = null;
for (let i = 0; i < testLoopCount; ++i) {
    let which = i % 3 ? Reflect : fake;
    received = [];

    let result = none(which);
    if (which === fake) {
        shouldBe(result, "fake");
        checkList(received[0].list, 0);
        shouldBe(received[0].count, 2);
    } else
        shouldBe(result.args.length, 0);

    result = three(which, i);
    if (which === fake) {
        shouldBe(result, "fake");
        let { self, target, list, count } = received[1];
        shouldBe(self, fake);
        shouldBe(target, Collect);
        shouldBe(count, 2);
        shouldBe(list[0], i);
        shouldBe(list[1], 1.5);
        shouldBe(list[2], "c");
        checkList(list, 3);
        shouldBe(list !== previous, true);
        previous = list;
    } else
        shouldBe(result.args.join(), i + ",1.5,c");

    result = withNewTarget(which, i);
    if (which === fake) {
        shouldBe(result, "fake");
        let { list, newTarget, count } = received[2];
        shouldBe(newTarget, Other);
        shouldBe(count, 5);
        shouldBe(list[0], i);
        shouldBe(list[1].a, i);
        checkList(list, 2);
    } else {
        shouldBe(result.args[1].a, i);
        shouldBe(result.newTarget, Other);
    }

    result = many(which, i);
    if (which === fake) {
        shouldBe(result, "fake");
        let { list } = received[3];
        for (let j = 0; j < 17; ++j)
            shouldBe(list[j], i + j);
        checkList(list, 17);
    } else
        shouldBe(result.args[16], i + 16);
}
