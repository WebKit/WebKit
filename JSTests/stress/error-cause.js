function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
}

const errorConstructors = [Error, EvalError, RangeError, ReferenceError, SyntaxError, TypeError, URIError, AggregateError];
if (typeof WebAssembly !== 'undefined')
    errorConstructors.push(WebAssembly.CompileError, WebAssembly.LinkError, WebAssembly.RuntimeError);

const constructError = (E, ...args) => E === AggregateError ? new E([], '', ...args) : new E('', ...args);

for (const E of errorConstructors) {
    shouldBe(constructError(E).cause, undefined);
    shouldBe(constructError(E, undefined).cause, undefined);
    shouldBe(constructError(E, null).cause, undefined);
    shouldBe(constructError(E, true).cause, undefined);
    shouldBe(constructError(E, 3).cause, undefined);
    shouldBe(constructError(E, 'hi').cause, undefined);
    shouldBe(constructError(E, {}).cause, undefined);

    shouldBe(constructError(E, { cause: undefined }).cause, undefined);
    shouldBe(constructError(E, { cause: null }).cause, null);
    shouldBe(constructError(E, { cause: true }).cause, true);
    shouldBe(constructError(E, { cause: 3 }).cause, 3);
    shouldBe(constructError(E, { cause: 'hi' }).cause, 'hi');

    const cause = new Error();
    shouldBe(constructError(E, { cause }).cause, cause);
}
