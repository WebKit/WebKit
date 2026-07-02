// The DFG/FTL intrinsic for %RegExpStringIteratorPrototype%.next must keep every observable
// behavior of the spec even after the call site is compiled against primordial iterators.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("Expected " + expected + " but got " + actual);
}

function shouldThrow(fn, ctor) {
    let threw = false;
    try { fn(); } catch (e) {
        threw = true;
        if (ctor && !(e instanceof ctor))
            throw new Error("Expected " + ctor.name + ", got " + e);
    }
    if (!threw)
        throw new Error("Expected throw");
}

function drain(iterator) {
    let count = 0;
    for (;;) {
        const result = iterator.next();
        if (result.done)
            return count;
        count++;
    }
}
noInline(drain);

// Warm up with primordial iterators so the call site tiers up through DFG/FTL.
for (let i = 0; i < 1e4; ++i)
    shouldBe(drain("a1b2c3".matchAll(/[a-z]/g)), 3);

// 1. A matcher with a custom exec must still have exec invoked once per next() call.
{
    let execCalls = 0;
    class TraceExec extends RegExp {
        exec(str) {
            execCalls++;
            return RegExp.prototype.exec.call(this, str);
        }
    }
    for (let i = 0; i < 100; ++i) {
        execCalls = 0;
        shouldBe(drain("a1b2c3".matchAll(new TraceExec("[a-z]", "g"))), 3);
        shouldBe(execCalls, 4);
    }
}

// 2. ToLength(Get(matcher, "lastIndex")) stays observable for empty matches.
{
    let valueOfCalls = 0;
    let matcher;
    class EmptySpecies extends RegExp {
        static get [Symbol.species]() {
            return function (pattern, flags) {
                matcher = new RegExp(pattern, flags);
                let returnedEmptyMatch = false;
                matcher.exec = function () {
                    if (returnedEmptyMatch)
                        return null;
                    returnedEmptyMatch = true;
                    return [""];
                };
                return matcher;
            };
        }
    }
    for (let i = 0; i < 100; ++i) {
        valueOfCalls = 0;
        const iterator = "abc".matchAll(new EmptySpecies("a", "g"));
        matcher.lastIndex = { valueOf() { valueOfCalls++; return 0; } };
        shouldBe(drain(iterator), 1);
        shouldBe(valueOfCalls, 1);
        shouldBe(matcher.lastIndex, 1);
    }
}

// 3. The brand check keeps throwing TypeError for non-iterator receivers after warmup.
{
    const next = Object.getPrototypeOf("".matchAll(/x/g)).next;
    function callNext(receiver) {
        return next.call(receiver);
    }
    noInline(callNext);

    for (let i = 0; i < 1e4; ++i)
        shouldBe(callNext("ab".matchAll(/a/g)).done, false);

    for (let i = 0; i < 100; ++i) {
        shouldThrow(() => callNext({}), TypeError);
        shouldThrow(() => callNext(42), TypeError);
        shouldThrow(() => callNext(Object.getPrototypeOf("".matchAll(/x/g))), TypeError);
    }
}

// 4. Exhausted iterators keep returning { undefined, true } in compiled code.
{
    const iterator = "a".matchAll(/a/g);
    drain(iterator);
    for (let i = 0; i < 1e4; ++i) {
        const result = iterator.next();
        shouldBe(result.done, true);
        shouldBe(result.value, undefined);
    }
}
