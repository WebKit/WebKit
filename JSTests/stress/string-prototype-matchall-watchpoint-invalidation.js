// Verifies that String.prototype.matchAll honors dynamic mutations of @@matchAll-related
// state after DFG/FTL code has been compiled. Each test warms up the fast path,
// then invalidates one watchpoint or the per-instance state and confirms the
// override is observed. Mirrors string-prototype-match-watchpoint-invalidation.js.

function shouldBe(actual, expected) {
    var a = JSON.stringify(actual);
    var e = JSON.stringify(expected);
    if (a !== e)
        throw new Error("expected " + e + " but got " + a);
}

// 1. Per-instance @@matchAll override after warm-up.
(function () {
    function matchAll(str, re) { return [...str.matchAll(re)]; }
    noInline(matchAll);
    var re = /[0-9]/g;
    for (var i = 0; i < 1e4; ++i)
        shouldBe(matchAll("a1b2", re), [["1"], ["2"]]);
    re[Symbol.matchAll] = function (s) { return ["override:" + s]; };
    shouldBe(str_matchAll_via_get("a1b2", re), ["override:a1b2"]);
})();

function str_matchAll_via_get(str, re) { return str.matchAll(re); }
noInline(str_matchAll_via_get);

// 2. RegExp.prototype[@@matchAll] replaced after warm-up.
(function () {
    var re = /[0-9]/g;
    for (var i = 0; i < 1e4; ++i)
        shouldBe([...str_matchAll_via_get("a1b2", re)], [["1"], ["2"]]);
    var saved = RegExp.prototype[Symbol.matchAll];
    RegExp.prototype[Symbol.matchAll] = function (s) { return ["proto:" + s]; };
    try {
        shouldBe(str_matchAll_via_get("a1b2", re), ["proto:a1b2"]);
    } finally {
        RegExp.prototype[Symbol.matchAll] = saved;
    }
})();

// 3. RegExp.prototype.exec replaced after warm-up. Per spec, the iterator pulls each
//    match via R.exec(), so a custom exec must be observed at iteration time even when
//    matchAll itself returned without seeing it.
(function () {
    var re = /[0-9]/g;
    for (var i = 0; i < 1e4; ++i)
        shouldBe([...str_matchAll_via_get("a1b2", re)], [["1"], ["2"]]);
    var saved = RegExp.prototype.exec;
    var execCount = 0;
    RegExp.prototype.exec = function (s) {
        execCount++;
        return saved.call(this, s);
    };
    try {
        shouldBe([...str_matchAll_via_get("a1b2", re)], [["1"], ["2"]]);
        if (execCount === 0)
            throw new Error("custom RegExp.prototype.exec must be observed by the iterator");
    } finally {
        RegExp.prototype.exec = saved;
    }
})();

// 4. String.prototype[@@matchAll] defined after warm-up with a primitive pattern.
//    Per spec ("If regexp is not Object"), a primitive pattern argument does NOT trigger
//    a GetMethod(@@matchAll) lookup, so a user-installed String.prototype[@@matchAll]
//    is not observed for a primitive string pattern.
(function () {
    function matchAll(str, pat) { return [...str.matchAll(pat)]; }
    noInline(matchAll);
    for (var i = 0; i < 1e4; ++i)
        shouldBe(matchAll("a1b2", "[0-9]"), [["1"], ["2"]]);
    Object.defineProperty(String.prototype, Symbol.matchAll, {
        configurable: true,
        value: function () { return ["string-proto"]; }
    });
    try {
        shouldBe(matchAll("a1b2", "[0-9]"), [["1"], ["2"]]);
        // But a String *object* DOES go through @@matchAll on its prototype chain.
        shouldBe("a1b2".matchAll(new String("[0-9]")), ["string-proto"]);
    } finally {
        delete String.prototype[Symbol.matchAll];
    }
})();

// 5. Setting non-numeric lastIndex after warm-up forces the watchpoint to invalidate.
(function () {
    function matchAll(str, re) { return [...str.matchAll(re)]; }
    noInline(matchAll);
    var re = /[0-9]/g;
    for (var i = 0; i < 1e4; ++i)
        shouldBe(matchAll("a1b2", re), [["1"], ["2"]]);
    re.lastIndex = { valueOf() { return 0; } };
    shouldBe(matchAll("a1b2", re), [["1"], ["2"]]);
})();

// 6. Reflect.setPrototypeOf to a non-RegExp prototype after warm-up. Unlike match,
//    matchAll's spec requires a flags check before @@matchAll dispatch, so the custom
//    proto must also expose flags containing "g".
(function () {
    function matchAll(str, re) { return [...str.matchAll(re)]; }
    noInline(matchAll);
    var re = /[0-9]/g;
    for (var i = 0; i < 1e4; ++i)
        shouldBe(matchAll("a1b2", re), [["1"], ["2"]]);
    var weird = /[0-9]/g;
    Object.setPrototypeOf(weird, {
        [Symbol.matchAll](s) { return ["weird:" + s]; },
        get flags() { return "g"; },
    });
    shouldBe(str_matchAll_via_get("a1b2", weird), ["weird:a1b2"]);
    // The fast path must keep working for normal regexps after seeing one weird instance.
    shouldBe(matchAll("a1b2", re), [["1"], ["2"]]);
})();

// 7. ToString(this) mutates the regexp instance (adds an own property) between
//    the fast-path check and the iterator construction. The post-toString re-check
//    must observe this and fall back to the primordial @@matchAll path.
(function () {
    var re = /a/g;
    var receiver = {
        toString() {
            // Adding an own property invalidates the fast path's hasCustomProperties() check.
            re.foo = 42;
            return "aabb";
        }
    };
    shouldBe([...String.prototype.matchAll.call(receiver, re)], [["a"], ["a"]]);
})();

// 8. Mutating RegExp.prototype.exec inside ToString fires regExpPrimordialPropertiesWatchpointSet
//    and the iterator must observe the user-installed exec.
(function () {
    var re = /a/g;
    var saved = RegExp.prototype.exec;
    var execCalls = 0;
    var receiver = {
        toString() {
            RegExp.prototype.exec = function (s) {
                execCalls++;
                return saved.call(this, s);
            };
            return "aabb";
        }
    };
    try {
        var results = [...String.prototype.matchAll.call(receiver, re)];
        shouldBe(results, [["a"], ["a"]]);
        if (execCalls === 0)
            throw new Error("user-installed RegExp.prototype.exec must be observed");
    } finally {
        RegExp.prototype.exec = saved;
    }
})();

// 9. Replacing RegExp.prototype[@@matchAll] inside ToString must be observed: matcher dispatch
//    is locked in BEFORE ToString in the spec, so the primordial @@matchAll runs even after
//    invalidation. Since our fast path locks matcher = primordial pre-ToString, the linkTimeConstant
//    fallback should still call the primordial, not the user's replacement.
(function () {
    var re = /a/g;
    var saved = RegExp.prototype[Symbol.matchAll];
    var receiver = {
        toString() {
            RegExp.prototype[Symbol.matchAll] = function () { return ["should-not-be-called"]; };
            return "aa";
        }
    };
    try {
        var results = [...String.prototype.matchAll.call(receiver, re)];
        shouldBe(results, [["a"], ["a"]]);
    } finally {
        RegExp.prototype[Symbol.matchAll] = saved;
    }
})();
