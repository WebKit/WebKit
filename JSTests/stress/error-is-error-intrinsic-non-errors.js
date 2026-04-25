function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + actual + " expected: " + expected);
}

function test(v) {
    return Error.isError(v);
}
noInline(test);

var fakeError = {
    __proto__: Error.prototype,
    constructor: Error,
    message: "",
    [Symbol.toStringTag]: "Error",
};

var { proxy: revokedProxy, revoke } = Proxy.revocable(new Error(), {});
revoke();

var inputs = [
    undefined, null, true, false,
    0, -0, NaN, Infinity, 42,
    "", "foo", 42n, Symbol(),
    {}, [], function () {}, /a/g,
    new Map(), new Set(),
    Error, TypeError, AggregateError,
    Error.prototype,
    fakeError,
    new Proxy(new Error(), {}),
    new Proxy({}, {}),
    revokedProxy,
];

for (var i = 0; i < testLoopCount; ++i) {
    for (var j = 0; j < inputs.length; ++j)
        shouldBe(test(inputs[j]), false);
}

function testNoArgs() {
    return Error.isError();
}
noInline(testNoArgs);

for (var i = 0; i < testLoopCount; ++i)
    shouldBe(testNoArgs(), false);
