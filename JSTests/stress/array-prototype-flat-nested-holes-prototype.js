function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`Expected ${expected} but got ${actual}`);
}

function shouldBeArray(actual, expected) {
    shouldBe(actual.length, expected.length);
    for (let i = 0; i < expected.length; ++i)
        shouldBe(actual[i], expected[i]);
}

// Nested ArrayWithInt32 with a hole and a prototype that fills it.
{
    let child = [, 1, 2];
    Object.setPrototypeOf(child, { 0: 99 });
    shouldBeArray([child].flat(), [99, 1, 2]);
}

// Nested ArrayWithDouble with a hole and a prototype that fills it.
{
    let child = [1.5, , 3.5];
    Object.setPrototypeOf(child, { 1: 2.5 });
    shouldBeArray([child].flat(), [1.5, 2.5, 3.5]);
}

// Nested ArrayWithContiguous with a hole and a prototype that fills it.
{
    let child = ["a", , "c"];
    Object.setPrototypeOf(child, { 1: "b" });
    shouldBeArray([child].flat(), ["a", "b", "c"]);
}

// Deeper nesting (depth = 2): the array two levels deep has the prototype.
{
    let inner = [, 1];
    Object.setPrototypeOf(inner, { 0: 42 });
    shouldBeArray([[inner]].flat(2), [42, 1]);
}

// The hole resolves to an array on the prototype, which itself should be flattened.
{
    let child = [, 3];
    Object.setPrototypeOf(child, { 0: [1, 2] });
    shouldBeArray([child].flat(2), [1, 2, 3]);
}

// Prototype with a getter, ensure it is invoked.
{
    let calls = 0;
    let proto = {};
    Object.defineProperty(proto, "0", { get() { calls++; return "x"; } });
    let child = [, "y"];
    Object.setPrototypeOf(child, proto);
    shouldBeArray([child].flat(), ["x", "y"]);
    shouldBe(calls, 1);
}
