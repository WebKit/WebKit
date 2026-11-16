function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' (expected: ' + expected + ')');
}

function shouldThrow(func, errorType)
{
    let threw = false;
    try {
        func();
    } catch (e) {
        threw = true;
        if (!(e instanceof errorType))
            throw new Error('unexpected error: ' + e);
    }
    if (!threw)
        throw new Error('did not throw');
}

// 1. SymbolUse fast path: repeated .toString() on a Symbol with a description.
(function () {
    function test(sym) {
        return sym.toString();
    }
    noInline(test);

    const sym = Symbol("cocoa");
    for (let i = 0; i < testLoopCount; ++i)
        shouldBe(test(sym), "Symbol(cocoa)");
}());

// 2. SymbolUse fast path: Symbol with no description must stringify as "Symbol()".
(function () {
    function test(sym) {
        return sym.toString();
    }
    noInline(test);

    const sym = Symbol();
    for (let i = 0; i < testLoopCount; ++i)
        shouldBe(test(sym), "Symbol()");
}());

// 3. Well-known symbols should tier up and return "Symbol(Symbol.iterator)" etc.
(function () {
    function test(sym) {
        return sym.toString();
    }
    noInline(test);

    for (let i = 0; i < testLoopCount; ++i) {
        shouldBe(test(Symbol.iterator), "Symbol(Symbol.iterator)");
        shouldBe(test(Symbol.asyncIterator), "Symbol(Symbol.asyncIterator)");
        shouldBe(test(Symbol.hasInstance), "Symbol(Symbol.hasInstance)");
    }
}());

// 4. Cached result must be stable across many invocations (same observable value).
(function () {
    function test(sym) {
        return sym.toString();
    }
    noInline(test);

    const sym = Symbol("stable");
    const first = test(sym);
    for (let i = 0; i < testLoopCount; ++i)
        shouldBe(test(sym), first);
}());

// 5. String(symbol) goes through stringConstructor's symbol fast path and should
//    match Symbol.prototype.toString.call(symbol).
(function () {
    function viaString(sym) {
        return String(sym);
    }
    function viaToString(sym) {
        return sym.toString();
    }
    noInline(viaString);
    noInline(viaToString);

    const sym = Symbol("matcha");
    for (let i = 0; i < testLoopCount; ++i) {
        shouldBe(viaString(sym), "Symbol(matcha)");
        shouldBe(viaString(sym), viaToString(sym));
    }
}());

// 6. Symbol.prototype.description must return undefined (not null) for a
//    descriptionless Symbol, per ECMA-262 20.4.3.2.
(function () {
    function desc(sym) {
        return sym.description;
    }
    noInline(desc);

    const withDesc = Symbol("rize");
    const withoutDesc = Symbol();
    for (let i = 0; i < testLoopCount; ++i) {
        shouldBe(desc(withDesc), "rize");
        shouldBe(desc(withoutDesc), undefined);
    }
}());

// 7. Symbol.prototype.toString.call on a non-Symbol must throw TypeError.
//    Exercises the generic (non-intrinsified) path and its OSR behavior.
(function () {
    const toString = Symbol.prototype.toString;
    function test(receiver) {
        return toString.call(receiver);
    }
    noInline(test);

    for (let i = 0; i < testLoopCount; ++i) {
        shouldThrow(() => test({}), TypeError);
        shouldThrow(() => test("not a symbol"), TypeError);
        shouldThrow(() => test(42), TypeError);
        shouldThrow(() => test(undefined), TypeError);
    }
}());

// 8. Symbol created from a non-string description (coerced via toString) should
//    preserve the coerced description across repeated toString() calls.
(function () {
    function test(sym) {
        return sym.toString();
    }
    noInline(test);

    const descriptor = { toString() { return "coerced"; } };
    const sym = Symbol(descriptor);
    for (let i = 0; i < testLoopCount; ++i)
        shouldBe(test(sym), "Symbol(coerced)");
}());

// 9. OSR-exit path: feed a non-Symbol to a function that has tiered up on
//    Symbols. The SymbolUse speculation must fail cleanly and the call must
//    fall back to the generic toString (which throws TypeError for non-Symbol
//    receivers when invoked via Symbol.prototype.toString.call).
(function () {
    const toString = Symbol.prototype.toString;
    function test(receiver) {
        return toString.call(receiver);
    }
    noInline(test);

    const sym = Symbol("kilimanjaro");
    // Warm up on Symbol to encourage speculation.
    for (let i = 0; i < testLoopCount; ++i)
        shouldBe(test(sym), "Symbol(kilimanjaro)");

    // Now hit the non-Symbol path.
    shouldThrow(() => test({}), TypeError);

    // And make sure the Symbol path still works after OSR exit.
    for (let i = 0; i < testLoopCount; ++i)
        shouldBe(test(sym), "Symbol(kilimanjaro)");
}());
