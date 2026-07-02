// Verifies that String.prototype.match honors dynamic mutations of @@match-related
// state after DFG/FTL code has been compiled. Each test warms up the fast path,
// then invalidates one watchpoint or the per-instance state and confirms the
// override is observed.

function shouldBe(actual, expected) {
    var a = JSON.stringify(actual);
    var e = JSON.stringify(expected);
    if (a !== e)
        throw new Error("expected " + e + " but got " + a);
}

// 1. Per-instance @@match override after JIT compilation with a primordial RegExp.
(function () {
    function match(str, re) { return str.match(re); }
    noInline(match);
    var re = /[0-9]/g;
    for (var i = 0; i < 1e4; ++i)
        shouldBe(match("a1b2c3", re), ["1", "2", "3"]);
    re[Symbol.match] = function (s) { return ["override:" + s]; };
    shouldBe(match("a1b2c3", re), ["override:a1b2c3"]);
})();

// 2. RegExp.prototype[@@match] replaced after JIT compilation.
(function () {
    function match(str, re) { return str.match(re); }
    noInline(match);
    var re = /[0-9]/g;
    for (var i = 0; i < 1e4; ++i)
        shouldBe(match("a1b2c3", re), ["1", "2", "3"]);
    var saved = RegExp.prototype[Symbol.match];
    RegExp.prototype[Symbol.match] = function (s) { return ["proto:" + s]; };
    try {
        shouldBe(match("a1b2c3", re), ["proto:a1b2c3"]);
    } finally {
        RegExp.prototype[Symbol.match] = saved;
    }
})();

// 3. RegExp.prototype.exec replaced after JIT compilation.
(function () {
    function match(str, re) { return str.match(re); }
    noInline(match);
    var re = /[0-9]/g;
    for (var i = 0; i < 1e4; ++i)
        shouldBe(match("a1b2c3", re), ["1", "2", "3"]);
    var saved = RegExp.prototype.exec;
    var execCount = 0;
    RegExp.prototype.exec = function (s) {
        execCount++;
        return saved.call(this, s);
    };
    try {
        shouldBe(match("a1b2c3", re), ["1", "2", "3"]);
        if (execCount === 0)
            throw new Error("custom RegExp.prototype.exec must be observed");
    } finally {
        RegExp.prototype.exec = saved;
    }
})();

// 4. String.prototype[@@match] defined after JIT compilation with a string regexp.
(function () {
    function match(str, pat) { return str.match(pat); }
    noInline(match);
    for (var i = 0; i < 1e4; ++i)
        shouldBe(match("abc1def2", "[0-9]"), ["1"]);
    // Per current spec ("2. If regexp is not Object"), a primitive pattern argument
    // does NOT trigger a GetMethod(@@match) lookup, so a user-installed
    // String.prototype[@@match] is not observed for a primitive string pattern.
    // (See also test262 cstm-matcher-on-string-primitive.js.)
    Object.defineProperty(String.prototype, Symbol.match, {
        configurable: true,
        value: function () { return ["string-proto"]; }
    });
    try {
        shouldBe(match("abc1def2", "[0-9]"), ["1"]);
        // But a String *object* DOES go through @@match.
        shouldBe("abc1def2".match(new String("[0-9]")), ["string-proto"]);
    } finally {
        delete String.prototype[Symbol.match];
    }
})();

// 5. RegExp.prototype.global replaced after JIT compilation.
(function () {
    function match(str, re) { return str.match(re); }
    noInline(match);
    var re = /[0-9]/g;
    for (var i = 0; i < 1e4; ++i)
        shouldBe(match("a1b2c3", re), ["1", "2", "3"]);
    var savedDescriptor = Object.getOwnPropertyDescriptor(RegExp.prototype, "global");
    Object.defineProperty(RegExp.prototype, "global", {
        configurable: true,
        get() { return false; }
    });
    try {
        // With global=false, RegExp.prototype[@@match] takes the non-global path:
        // a single RegExpExec(R, S). lastIndex stays 0 after the global resets above.
        shouldBe(match("a1b2c3", re), ["1"]);
    } finally {
        Object.defineProperty(RegExp.prototype, "global", savedDescriptor);
    }
})();

// 6. Setting non-numeric lastIndex after JIT compilation.
(function () {
    function match(str, re) { return str.match(re); }
    noInline(match);
    var re = /[0-9]/g;
    for (var i = 0; i < 1e4; ++i)
        shouldBe(match("a1b2c3", re), ["1", "2", "3"]);
    var valueOfCount = 0;
    re.lastIndex = { valueOf() { valueOfCount++; return 0; } };
    shouldBe(match("a1b2c3", re), ["1", "2", "3"]);
    // The slow path is hit after this since the compiled fast path bailed out.
})();

// 7. Reflect.setPrototypeOf to a non-RegExp prototype after JIT compilation.
(function () {
    function match(str, re) { return str.match(re); }
    noInline(match);
    var re = /[0-9]/g;
    for (var i = 0; i < 1e4; ++i)
        shouldBe(match("a1b2c3", re), ["1", "2", "3"]);
    var weird = /[0-9]/g;
    Object.setPrototypeOf(weird, {
        [Symbol.match](s) { return ["weird:" + s]; }
    });
    shouldBe(match("a1b2c3", weird), ["weird:a1b2c3"]);
    // The fast path must keep working for normal regexps after seeing one weird instance.
    shouldBe(match("a1b2c3", re), ["1", "2", "3"]);
})();

// 8. ToString(this) mutates the regexp instance between the fast-path check and
//    the C++ RegExpMatchFast engine. Per spec, RegExp.prototype[@@match] reads
//    R.flags and dispatches to RegExpExec(R, S) *after* S = ToString(string), so
//    the user-installed exec must be observed.
(function () {
    var re = /a/;
    var receiver = {
        toString() {
            re.exec = function () { var r = ["evil"]; r.index = 0; r.input = "abc"; return r; };
            return "abc";
        }
    };
    shouldBe(String.prototype.match.call(receiver, re), ["evil"]);
})();
(function () {
    // Mutating RegExp.prototype.exec inside ToString fires regExpPrimordialPropertiesWatchpointSet.
    var re = /a/;
    var saved = RegExp.prototype.exec;
    var receiver = {
        toString() {
            RegExp.prototype.exec = function () { var r = ["evil"]; r.index = 0; r.input = "abc"; return r; };
            return "abc";
        }
    };
    try {
        shouldBe(String.prototype.match.call(receiver, re), ["evil"]);
    } finally {
        RegExp.prototype.exec = saved;
    }
})();
(function () {
    // Mutating lastIndex to a non-number inside ToString.
    var re = /a/g;
    var valueOfCount = 0;
    var receiver = {
        toString() {
            re.lastIndex = { valueOf() { valueOfCount++; return 0; } };
            return "abc";
        }
    };
    shouldBe(String.prototype.match.call(receiver, re), ["a"]);
})();
