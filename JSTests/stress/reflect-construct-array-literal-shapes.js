function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)}, expected: ${String(expected)}`);
}

function Collect()
{
    this.args = Array.prototype.slice.call(arguments);
}

function hole(a, b)
{
    return Reflect.construct(Collect, [a, , b]);
}
noInline(hole);

function trailingHole(a)
{
    return Reflect.construct(Collect, [a, , ]);
}
noInline(trailingHole);

function spread(a, rest)
{
    return Reflect.construct(Collect, [a, ...rest]);
}
noInline(spread);

function parenthesized(a, b)
{
    return Reflect.construct(Collect, ([a, b]));
}
noInline(parenthesized);

function commaExpression(a, b)
{
    return Reflect.construct(Collect, (a, [b]));
}
noInline(commaExpression);

for (let i = 0; i < testLoopCount; ++i) {
    let object = hole(i, "b");
    shouldBe(object.args.length, 3);
    shouldBe(object.args[0], i);
    shouldBe(object.args[1], undefined);
    shouldBe(object.args[2], "b");

    object = trailingHole(i);
    shouldBe(object.args.length, 2);
    shouldBe(object.args[1], undefined);

    object = spread(i, ["b", "c"]);
    shouldBe(object.args.join(), i + ",b,c");

    shouldBe(parenthesized(i, "b").args.join(), i + ",b");
    shouldBe(commaExpression(i, "b").args.join(), "b");
}
