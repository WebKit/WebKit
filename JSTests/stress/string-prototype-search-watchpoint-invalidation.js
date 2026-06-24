// Verifies that String.prototype.search honors dynamic mutations of @@search-related
// state after DFG/FTL code has been compiled. Each test warms up the fast path,
// then invalidates one watchpoint or the per-instance state and confirms the
// override is observed.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("expected " + expected + " but got " + actual);
}

// 1. Per-instance @@search override after JIT compilation with a primordial RegExp.
(function () {
    function search(str, re) { return str.search(re); }
    noInline(search);
    var re = /[0-9]/;
    for (var i = 0; i < 1e4; ++i)
        shouldBe(search("a1b2c3", re), 1);
    re[Symbol.search] = function (s) { return "override:" + s; };
    shouldBe(search("a1b2c3", re), "override:a1b2c3");
})();

// 2. RegExp.prototype[@@search] replaced after JIT compilation.
(function () {
    function search(str, re) { return str.search(re); }
    noInline(search);
    var re = /[0-9]/;
    for (var i = 0; i < 1e4; ++i)
        shouldBe(search("a1b2c3", re), 1);
    var saved = RegExp.prototype[Symbol.search];
    RegExp.prototype[Symbol.search] = function (s) { return "proto:" + s; };
    try {
        shouldBe(search("a1b2c3", re), "proto:a1b2c3");
    } finally {
        RegExp.prototype[Symbol.search] = saved;
    }
})();

// 3. RegExp.prototype.exec replaced after JIT compilation.
(function () {
    function search(str, re) { return str.search(re); }
    noInline(search);
    var re = /[0-9]/;
    for (var i = 0; i < 1e4; ++i)
        shouldBe(search("a1b2c3", re), 1);
    var saved = RegExp.prototype.exec;
    var execCount = 0;
    RegExp.prototype.exec = function (s) {
        execCount++;
        return saved.call(this, s);
    };
    try {
        shouldBe(search("a1b2c3", re), 1);
        if (execCount === 0)
            throw new Error("custom RegExp.prototype.exec must be observed");
    } finally {
        RegExp.prototype.exec = saved;
    }
})();

// 4. String.prototype[@@search] defined after JIT compilation with a string regexp.
(function () {
    function search(str, pat) { return str.search(pat); }
    noInline(search);
    for (var i = 0; i < 1e4; ++i)
        shouldBe(search("abc1def2", "[0-9]"), 3);
    // Per current spec ("2. If regexp is not Object"), a primitive pattern argument
    // does NOT trigger a GetMethod(@@search) lookup, so a user-installed
    // String.prototype[@@search] is not observed for a primitive string pattern.
    Object.defineProperty(String.prototype, Symbol.search, {
        configurable: true,
        value: function () { return "string-proto"; }
    });
    try {
        shouldBe(search("abc1def2", "[0-9]"), 3);
        // But a String *object* DOES go through @@search.
        shouldBe("abc1def2".search(new String("[0-9]")), "string-proto");
    } finally {
        delete String.prototype[Symbol.search];
    }
})();

// 5. Non-numeric lastIndex after JIT compilation. @@search never coerces lastIndex,
//    so valueOf must not be observed and the result stays correct.
(function () {
    function search(str, re) { return str.search(re); }
    noInline(search);
    var re = /[0-9]/g;
    for (var i = 0; i < 1e4; ++i)
        shouldBe(search("a1b2c3", re), 1);
    var valueOfCount = 0;
    re.lastIndex = { valueOf() { valueOfCount++; return 0; } };
    shouldBe(search("a1b2c3", re), 1);
    shouldBe(valueOfCount, 0);
})();

// 6. Non-writable lastIndex after JIT compilation.
(function () {
    function search(str, re) { return str.search(re); }
    noInline(search);
    var re = /[0-9]/g;
    for (var i = 0; i < 1e4; ++i)
        shouldBe(search("a1b2c3", re), 1);
    // With lastIndex frozen at 0 and a non-global regexp, @@search performs no
    // lastIndex writes and succeeds.
    var frozenZero = /[0-9]/;
    Object.defineProperty(frozenZero, "lastIndex", { writable: false });
    shouldBe(search("a1b2c3", frozenZero), 1);
    // The original regexp keeps using the fast path.
    shouldBe(search("a1b2c3", re), 1);
})();

// 7. Reflect.setPrototypeOf to a non-RegExp prototype after JIT compilation.
(function () {
    function search(str, re) { return str.search(re); }
    noInline(search);
    var re = /[0-9]/;
    for (var i = 0; i < 1e4; ++i)
        shouldBe(search("a1b2c3", re), 1);
    var weird = /[0-9]/;
    Object.setPrototypeOf(weird, {
        [Symbol.search](s) { return "weird:" + s; }
    });
    shouldBe(search("a1b2c3", weird), "weird:a1b2c3");
    // The fast path must keep working for normal regexps after seeing one weird instance.
    shouldBe(search("a1b2c3", re), 1);
})();

// 8. ToString(this) mutates the regexp instance between the fast-path check and
//    the search engine. Per spec, RegExp.prototype[@@search] dispatches to
//    RegExpExec(rx, S) *after* S = ToString(string), so the user-installed exec
//    must be observed.
(function () {
    var re = /a/;
    var receiver = {
        toString() {
            re.exec = function () { return { index: 42 }; };
            return "abc";
        }
    };
    shouldBe(String.prototype.search.call(receiver, re), 42);
})();
(function () {
    // Mutating RegExp.prototype.exec inside ToString fires regExpPrimordialPropertiesWatchpointSet.
    var re = /a/;
    var saved = RegExp.prototype.exec;
    var receiver = {
        toString() {
            RegExp.prototype.exec = function () { return { index: 42 }; };
            return "abc";
        }
    };
    try {
        shouldBe(String.prototype.search.call(receiver, re), 42);
    } finally {
        RegExp.prototype.exec = saved;
    }
})();
(function () {
    // Mutating lastIndex to a non-number inside ToString.
    var re = /b/;
    var receiver = {
        toString() {
            re.lastIndex = { valueOf() { return 0; } };
            return "abc";
        }
    };
    shouldBe(String.prototype.search.call(receiver, re), 1);
})();
(function () {
    // Making lastIndex non-writable (and non-zero) inside ToString: the generic
    // @@search path must throw when resetting lastIndex to 0.
    var re = /b/;
    re.lastIndex = 3;
    var receiver = {
        toString() {
            Object.defineProperty(re, "lastIndex", { writable: false });
            return "abc";
        }
    };
    var threw = false;
    try {
        String.prototype.search.call(receiver, re);
    } catch (e) {
        threw = e instanceof TypeError;
    }
    shouldBe(threw, true);
})();
