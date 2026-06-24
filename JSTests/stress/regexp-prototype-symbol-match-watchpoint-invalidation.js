// Verifies that direct calls to RegExp.prototype[@@match] honor dynamic mutations
// after DFG/FTL has compiled the call. Each scenario warms the fast path with
// testLoopCount iterations, then invalidates a watchpoint or per-instance
// state and confirms the override is observed.

function shouldBe(actual, expected) {
    var a = JSON.stringify(actual);
    var e = JSON.stringify(expected);
    if (a !== e)
        throw new Error("expected " + e + " but got " + a);
}

function shouldThrow(func, expected) {
    var error;
    try {
        func();
    } catch (e) {
        error = e;
    }
    if (!error)
        throw new Error("expected to throw");
    if (String(error).indexOf(expected) === -1)
        throw new Error("expected '" + expected + "' but got '" + error + "'");
}

// 1. Per-instance exec override after JIT compilation.
//    The DFG-emitted CheckIsConstant on regexp.exec must catch this and OSR-exit.
(function () {
    function match(re, s) { return re[Symbol.match](s); }
    noInline(match);
    var primordial = /[0-9]/g;
    for (var i = 0; i < testLoopCount; ++i)
        shouldBe(match(primordial, "a1b2c3"), ["1", "2", "3"]);
    var custom = /[0-9]/g;
    custom.exec = function () { return null; };
    shouldBe(match(custom, "a1b2c3"), null);
    shouldBe(match(primordial, "a1b2c3"), ["1", "2", "3"]);
})();

// 2. RegExp.prototype.exec replaced after JIT compilation.
//    The primordial properties watchpoint fires and the compiled code is invalidated.
(function () {
    function match(re, s) { return re[Symbol.match](s); }
    noInline(match);
    var re = /[0-9]/g;
    for (var i = 0; i < testLoopCount; ++i)
        shouldBe(match(re, "a1b2c3"), ["1", "2", "3"]);
    var saved = RegExp.prototype.exec;
    var execCount = 0;
    RegExp.prototype.exec = function (s) {
        execCount++;
        return saved.call(this, s);
    };
    try {
        shouldBe(match(re, "a1b2c3"), ["1", "2", "3"]);
        if (execCount === 0)
            throw new Error("custom RegExp.prototype.exec must be observed");
    } finally {
        RegExp.prototype.exec = saved;
    }
})();

// 3. lastIndex set to a non-number after JIT compilation.
//    The fixup-time Check<NumberUse> on lastIndex must OSR-exit and the C++
//    slow path must coerce it to a number per spec.
(function () {
    function match(re, s) { return re[Symbol.match](s); }
    noInline(match);
    var re = /[0-9]/g;
    for (var i = 0; i < testLoopCount; ++i)
        shouldBe(match(re, "a1b2c3"), ["1", "2", "3"]);
    re.lastIndex = "garbage";
    // For global regexps, [@@match] resets lastIndex to 0 first, so the result is
    // unaffected by the prior value of lastIndex.
    shouldBe(match(re, "a1b2c3"), ["1", "2", "3"]);
})();

// 4. lastIndex with side-effecting valueOf after JIT compilation.
//    For non-global regexps lastIndex is consulted by RegExpExec; the user's
//    valueOf must be observed.
(function () {
    function match(re, s) { return re[Symbol.match](s); }
    noInline(match);
    var re = /[0-9]/;
    for (var i = 0; i < testLoopCount; ++i)
        shouldBe(match(re, "a1b2c3"), ["1"]);
    var valueOfCount = 0;
    re.lastIndex = { valueOf() { valueOfCount++; return 0; } };
    // exec is the primordial one and the regexp is non-global, so RegExpExec
    // calls ToLength(lastIndex), which calls valueOf.
    shouldBe(match(re, "a1b2c3"), ["1"]);
    if (valueOfCount === 0)
        throw new Error("lastIndex.valueOf was not observed");
})();

// 5. RegExp.prototype.global replaced after JIT compilation.
(function () {
    function match(re, s) { return re[Symbol.match](s); }
    noInline(match);
    var re = /[0-9]/g;
    for (var i = 0; i < testLoopCount; ++i)
        shouldBe(match(re, "a1b2c3"), ["1", "2", "3"]);
    var savedDescriptor = Object.getOwnPropertyDescriptor(RegExp.prototype, "global");
    Object.defineProperty(RegExp.prototype, "global", {
        configurable: true,
        get() { return false; }
    });
    try {
        // With global=false, [@@match] takes the non-global path: a single RegExpExec.
        shouldBe(match(re, "a1b2c3"), ["1"]);
    } finally {
        Object.defineProperty(RegExp.prototype, "global", savedDescriptor);
    }
})();

// 6. RegExp.prototype.flags getter replaced after JIT compilation.
//    The watchpoint covers `flags` so the recompiled slow path observes the override.
(function () {
    function match(re, s) { return re[Symbol.match](s); }
    noInline(match);
    var re = /[0-9]/g;
    for (var i = 0; i < testLoopCount; ++i)
        shouldBe(match(re, "a1b2c3"), ["1", "2", "3"]);
    var savedDescriptor = Object.getOwnPropertyDescriptor(RegExp.prototype, "flags");
    Object.defineProperty(RegExp.prototype, "flags", {
        configurable: true,
        get() { return ""; }
    });
    try {
        // flags="" → not global → single match.
        shouldBe(match(re, "a1b2c3"), ["1"]);
    } finally {
        Object.defineProperty(RegExp.prototype, "flags", savedDescriptor);
    }
})();

// 7. Subclass with primordial prototype properties stays on the slow path
//    (custom-properties guard) and still produces the correct result.
(function () {
    class MyRegExp extends RegExp {}
    function match(re, s) { return re[Symbol.match](s); }
    noInline(match);
    var re = new MyRegExp("[0-9]", "g");
    re.tag = "subclass";
    for (var i = 0; i < testLoopCount; ++i)
        shouldBe(match(re, "a1b2c3"), ["1", "2", "3"]);
})();

// 8. Setting prototype to a non-RegExp prototype after warmup must be observed.
(function () {
    function match(re, s) { return re[Symbol.match](s); }
    noInline(match);
    var re = /[0-9]/g;
    for (var i = 0; i < testLoopCount; ++i)
        shouldBe(match(re, "a1b2c3"), ["1", "2", "3"]);
    var weird = /[0-9]/g;
    Object.setPrototypeOf(weird, {
        [Symbol.match](s) { return ["weird:" + s]; }
    });
    // Direct call goes through the user's [@@match].
    shouldBe(weird[Symbol.match]("a1b2c3"), ["weird:a1b2c3"]);
    // The fast path must keep working for normal regexps after one weird instance.
    shouldBe(match(re, "a1b2c3"), ["1", "2", "3"]);
})();

// 9. Calling RegExp.prototype[@@match] on a non-object must throw.
(function () {
    var match = RegExp.prototype[Symbol.match];
    shouldThrow(function () { match.call(null, "x"); }, "TypeError");
    shouldThrow(function () { match.call(undefined, "x"); }, "TypeError");
    shouldThrow(function () { match.call(42, "x"); }, "TypeError");
    shouldThrow(function () { match.call("str", "x"); }, "TypeError");
})();

// 10. Empty-match advancement in the slow path with fullUnicode=true.
//     The empty-match branch must call AdvanceStringIndex; the C++ slow path
//     must surrogate-pair-aware advance to avoid infinite loops.
(function () {
    function match(re, s) { return re[Symbol.match](s); }
    noInline(match);
    var re = /(?:)/gu;
    for (var i = 0; i < testLoopCount; ++i) {
        var r = match(re, "😀x");
        shouldBe(r, ["", "", ""]);
    }
})();

// 11. Empty-match advancement without unicode flag.
(function () {
    function match(re, s) { return re[Symbol.match](s); }
    noInline(match);
    var re = /(?:)/g;
    for (var i = 0; i < testLoopCount; ++i)
        shouldBe(match(re, "ab"), ["", "", ""]);
})();
