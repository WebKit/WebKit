function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${actual}, expected: ${expected}`);
}

// Array.prototype.copyWithin step 1: Let O be ? ToObject(this value).
// The spec always returns O, so when |this| is a primitive, the return
// value must be the wrapper object, not the original primitive.

// finalIndex < from path (early return)
{
    let result = Array.prototype.copyWithin.call("hello", 0, 3, 1);
    shouldBe(typeof result, "object");
    shouldBe(result instanceof String, true);
    shouldBe(result.valueOf(), "hello");
}

// count == 0 path
{
    let result = Array.prototype.copyWithin.call("hello", 0, 0, 0);
    shouldBe(typeof result, "object");
    shouldBe(result instanceof String, true);
    shouldBe(result.valueOf(), "hello");
}

// Number primitive, finalIndex < from
{
    let result = Array.prototype.copyWithin.call(42, 0, 3, 1);
    shouldBe(typeof result, "object");
    shouldBe(result instanceof Number, true);
    shouldBe(result.valueOf(), 42);
}

// Boolean primitive, finalIndex < from
{
    let result = Array.prototype.copyWithin.call(true, 0, 3, 1);
    shouldBe(typeof result, "object");
    shouldBe(result instanceof Boolean, true);
    shouldBe(result.valueOf(), true);
}

// Symbol primitive, finalIndex < from
{
    let sym = Symbol("s");
    let result = Array.prototype.copyWithin.call(sym, 0, 3, 1);
    shouldBe(typeof result, "object");
    shouldBe(result.valueOf(), sym);
}
