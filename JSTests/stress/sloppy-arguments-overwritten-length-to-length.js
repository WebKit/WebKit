function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)}, expected: ${String(expected)}`);
}

function shouldThrow(func, errorConstructor)
{
    let error;
    try {
        func();
    } catch (e) {
        error = e;
    }
    if (!(error instanceof errorConstructor))
        throw new Error(`bad error: ${String(error)}`);
}

function direct(length)
{
    if (length !== undefined)
        arguments.length = length;
    return arguments;
}

function scoped(length)
{
    (function () { return length; })();
    if (length !== undefined)
        arguments.length = length;
    return arguments;
}

function count()
{
    return arguments.length;
}

function apply(args)
{
    return count.apply(null, args);
}
noInline(apply);

function reflectApply(args)
{
    return Reflect.apply(count, null, args);
}
noInline(reflectApply);

function testNegative(create, length)
{
    let pushed = create(length, 2, 3);
    shouldBe(Array.prototype.push.call(pushed, 4), 1);
    shouldBe(pushed[0], 4);
    shouldBe(pushed.length, 1);

    let popped = create(length, 2, 3);
    shouldBe(Array.prototype.pop.call(popped), undefined);
    shouldBe(popped.length, 0);

    let args = create(length, 2, 3);
    shouldBe(Array.prototype.slice.call(args).length, 0);
    shouldBe(Array.prototype.slice.call(args, 0, 2).length, 0);
    shouldBe(Array.prototype.indexOf.call(args, 2), -1);
    shouldBe(Array.prototype.indexOf.call(args, 4), -1);
    shouldBe(Array.prototype.lastIndexOf.call(args, 2), -1);
    shouldBe(Array.prototype.includes.call(args, 3), false);
    shouldBe(Array.prototype.join.call(args), "");
    shouldBe(Array.prototype.reverse.call(args), args);
    shouldBe(Array.prototype.fill.call(args, 0), args);
    shouldBe(Array.prototype.toReversed.call(args).length, 0);
    shouldBe(apply(args), 0);
    shouldBe(reflectApply(args), 0);
    shouldBe(args[1], 2);
    shouldBe(args[2], 3);
    shouldBe(args.length, length);
}

function testLarge(create, length, expected)
{
    let args = create(length, 2, 3);
    shouldBe(Array.prototype.indexOf.call(args, 3), 2);
    shouldBe(Array.prototype.includes.call(args, undefined), true);
    shouldBe(Array.prototype.slice.call(args, 1, 3).join(), "2,3");
    shouldThrow(() => Array.prototype.slice.call(args), RangeError);
    shouldThrow(() => apply(args), RangeError);
    shouldThrow(() => reflectApply(args), RangeError);
    shouldBe(args.length, length);

    shouldBe(Array.prototype.pop.call(args), undefined);
    shouldBe(args.length, expected - 1);
    shouldBe(Array.prototype.push.call(args, 4), expected);
    shouldBe(args[expected - 1], 4);
    shouldBe(args.length, expected);
}

for (let i = 0; i < testLoopCount; ++i) {
    shouldBe(apply(direct(undefined, 2, 3)), 3);
    shouldBe(apply(scoped(undefined, 2, 3)), 3);
    shouldBe(reflectApply(direct(undefined, 2, 3)), 3);
    shouldBe(reflectApply(scoped(undefined, 2, 3)), 3);
}

for (let i = 0; i < 10; ++i) {
    for (let create of [direct, scoped]) {
        testNegative(create, -1);
        testNegative(create, -1.5);
        testNegative(create, -(2 ** 31));
        testNegative(create, -(2 ** 32) + 2);
        testNegative(create, -(2 ** 32) - 1);
        testNegative(create, -Infinity);
        testNegative(create, "-1");

        testLarge(create, 2 ** 32, 2 ** 32);
        testLarge(create, 2 ** 32 + 1, 2 ** 32 + 1);
        testLarge(create, 2 ** 32 + 2.5, 2 ** 32 + 2);
        testLarge(create, 2 ** 53 - 1, 2 ** 53 - 1);
        testLarge(create, 2 ** 53 + 2, 2 ** 53 - 1);
        testLarge(create, Infinity, 2 ** 53 - 1);

        shouldThrow(() => Array.prototype.push.call(create(Infinity, 2, 3), 4), TypeError);
    }
}
