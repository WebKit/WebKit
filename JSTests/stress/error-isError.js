function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
}

shouldBe(Error.isError(undefined), false);
shouldBe(Error.isError(null), false);
shouldBe(Error.isError(true), false);
shouldBe(Error.isError(42), false);
shouldBe(Error.isError("test"), false);
shouldBe(Error.isError([]), false);
shouldBe(Error.isError({}), false);

const errorConstructors = [Error, EvalError, RangeError, ReferenceError, SyntaxError, TypeError, URIError];
if (typeof WebAssembly !== 'undefined')
    errorConstructors.push(WebAssembly.CompileError, WebAssembly.LinkError, WebAssembly.RuntimeError);
for (const E of errorConstructors) {
    shouldBe(Error.isError(E), false);
    shouldBe(Error.isError(E.prototype), false);
    shouldBe(Error.isError(new E), true);
}

shouldBe(Error.isError(AggregateError), false);
shouldBe(Error.isError(AggregateError.prototype), false);
shouldBe(Error.isError(new AggregateError([])), true);
