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

function none()
{
    return Reflect.construct(Collect, []);
}
noInline(none);

function one(a)
{
    return Reflect.construct(Collect, [a]);
}
noInline(one);

function two(a, b)
{
    return Reflect.construct(Collect, [a, b], Other);
}
noInline(two);

function many(a)
{
    return Reflect.construct(Collect, [a, a + 1, a + 2, a + 3, a + 4, a + 5, a + 6, a + 7, a + 8, a + 9, a + 10, a + 11, a + 12, a + 13, a + 14, a + 15, a + 16], Collect, "ignored");
}
noInline(many);

function mixedTypes(a)
{
    return Reflect.construct(Collect, [a, 1.5, -0, "string", null, undefined, true, 10n, Symbol.iterator, { a }, [a], function () { }]);
}
noInline(mixedTypes);

function fewerThanParameters(a)
{
    return Reflect.construct(function (x, y, z) { this.values = [x, y, z]; this.count = arguments.length; }, [a]);
}
noInline(fewerThanParameters);

for (let i = 0; i < testLoopCount; ++i) {
    let object = none();
    shouldBe(object.args.length, 0);
    shouldBe(object.newTarget, Collect);

    object = one(i);
    shouldBe(object.args.length, 1);
    shouldBe(object.args[0], i);

    object = two(i, "b");
    shouldBe(object.args.join(), i + ",b");
    shouldBe(object.newTarget, Other);
    shouldBe(Object.getPrototypeOf(object), Other.prototype);

    object = many(i);
    shouldBe(object.args.length, 17);
    for (let j = 0; j < 17; ++j)
        shouldBe(object.args[j], i + j);

    object = mixedTypes(i);
    shouldBe(object.args.length, 12);
    shouldBe(object.args[0], i);
    shouldBe(object.args[1], 1.5);
    shouldBe(Object.is(object.args[2], -0), true);
    shouldBe(object.args[3], "string");
    shouldBe(object.args[4], null);
    shouldBe(object.args[5], undefined);
    shouldBe(object.args[6], true);
    shouldBe(object.args[7], 10n);
    shouldBe(object.args[8], Symbol.iterator);
    shouldBe(object.args[9].a, i);
    shouldBe(object.args[10][0], i);
    shouldBe(typeof object.args[11], "function");

    object = fewerThanParameters(i);
    shouldBe(object.count, 1);
    shouldBe(object.values[0], i);
    shouldBe(object.values[1], undefined);
    shouldBe(object.values[2], undefined);
}
