// A generator, async function, async generator, or async arrow body contains no tail positions
// (https://tc39.es/ecma262/#sec-static-semantics-isintailposition, steps 4 to 7), so an ordinary
// call under "return" must stay an ordinary call there.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)}, expected ${String(expected)}`);
}

function drain(promise) {
    var settled = false;
    var result;
    promise.then((value) => {
        settled = true;
        result = value;
    });
    drainMicrotasks();
    shouldBe(settled, true);
    return result;
}

// An async generator awaits whatever its body returns. A tail call would jump over that await and
// hand the caller the promise itself.
(function asyncGeneratorAwaitsReturnedCall() {
    "use strict";
    function f() { return Promise.resolve(42); }
    var object = { f };

    async function* plainCall() { return f(); }
    async function* memberCall() { return object.f(); }
    async function* spreadCall() { return f(...[]); }
    async function* applyCall() { return f.apply(null, []); }

    for (var generatorFunction of [plainCall, memberCall, spreadCall, applyCall]) {
        for (var i = 0; i < testLoopCount; ++i) {
            var result = drain(generatorFunction().next());
            shouldBe(result.done, true);
            shouldBe(result.value, 42);
        }
    }
})();

// The enclosing frame therefore stays on the stack, unlike an ordinary strict function's.
(function bodiesKeepTheirFrame() {
    "use strict";
    function callerName() {
        return new Error().stack.split("\n")[1].split("@")[0];
    }

    function* generatorBody() { return callerName(); }
    shouldBe(generatorBody().next().value, "generatorBody");

    async function asyncFunctionBody() { await 0; return callerName(); }
    shouldBe(drain(asyncFunctionBody()), "asyncFunctionBody");

    async function* asyncGeneratorBody() { return callerName(); }
    shouldBe(drain(asyncGeneratorBody().next()).value, "asyncGeneratorBody");

    // A resumed async arrow body carries no name of its own, so the point of this one is only
    // that a frame is there at all: without it the caller would be drainMicrotasks.
    var asyncArrowBody = async () => { await 0; return callerName(); };
    shouldBe(drain(asyncArrowBody()), "");

    function ordinaryStrictFunction() { return callerName(); }
    shouldBe(ordinaryStrictFunction(), "bodiesKeepTheirFrame");
})();
