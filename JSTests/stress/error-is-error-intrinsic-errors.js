function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + actual + " expected: " + expected);
}

function test(v) {
    return Error.isError(v);
}
noInline(test);

class MyError extends Error {}
class MyTypeError extends TypeError {}
class MyAggregateError extends AggregateError {}

var inputs = [
    new Error(),
    new EvalError(),
    new RangeError(),
    new ReferenceError(),
    new SyntaxError(),
    new TypeError(),
    new URIError(),
    new AggregateError([]),
    new MyError(),
    new MyTypeError(),
    new MyAggregateError([]),
];

for (var i = 0; i < testLoopCount; ++i) {
    for (var j = 0; j < inputs.length; ++j)
        shouldBe(test(inputs[j]), true);
}

function testThrown() {
    try {
        null.foo;
    } catch (e) {
        return Error.isError(e);
    }
}
noInline(testThrown);

for (var i = 0; i < testLoopCount; ++i)
    shouldBe(testThrown(), true);
