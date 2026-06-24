// Verifies spec edge cases for RegExp.prototype[@@match] now that it is
// implemented in C++. Covers: ordering of ToString(string) vs flags lookup,
// AdvanceStringIndex semantics, and behavior with custom exec returning
// non-object/non-null values.

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

// Spec step 3: ToString(string) happens BEFORE Get(rx, "flags").
(function () {
    var trace = [];
    var re = /a/;
    Object.defineProperty(re, "flags", {
        configurable: true,
        get() { trace.push("flags"); return ""; }
    });
    var arg = {
        toString() { trace.push("toString"); return "aaa"; }
    };
    re[Symbol.match](arg);
    shouldBe(trace, ["toString", "flags"]);
})();

// Spec step 6.b: For global regex, lastIndex is set to +0.
(function () {
    var re = /a/g;
    re.lastIndex = 5;
    re[Symbol.match]("aaa");
    shouldBe(re.lastIndex, 0);
})();

// Spec 6.e.iii.3: Empty match advances via AdvanceStringIndex.
(function () {
    var re = /(?:)/g;
    shouldBe(re[Symbol.match]("ab"), ["", "", ""]);
})();
(function () {
    // With unicode flag, AdvanceStringIndex is surrogate-pair aware.
    var re = /(?:)/gu;
    shouldBe(re[Symbol.match]("😀x"), ["", "", ""]);
})();
(function () {
    // With unicodeSets flag, AdvanceStringIndex is surrogate-pair aware.
    var re = /(?:)/gv;
    shouldBe(re[Symbol.match]("😀x"), ["", "", ""]);
})();

// Custom exec returning a non-object non-null value must throw.
(function () {
    var re = /a/g;
    re.exec = function () { return 42; };
    shouldThrow(function () { re[Symbol.match]("aaa"); }, "TypeError");
})();

// Custom exec returning an object with non-string [0] is coerced via ToString.
(function () {
    var re = /a/g;
    var execCalls = 0;
    re.exec = function () {
        execCalls++;
        if (execCalls === 1) return { 0: { toString() { return "x"; } } };
        return null;
    };
    shouldBe(re[Symbol.match]("a"), ["x"]);
})();

// Custom exec returning an object with empty-string [0] must advance
// lastIndex via AdvanceStringIndex.
(function () {
    var re = /(?:)/g;
    var ranges = [];
    re.exec = function (s) {
        ranges.push(re.lastIndex);
        if (re.lastIndex >= s.length) return null;
        return { 0: "" };
    };
    shouldBe(re[Symbol.match]("abc"), ["", "", ""]);
    shouldBe(ranges, [0, 1, 2, 3]);
})();

// Non-global path: result is whatever RegExpExec returns (can be an array
// with named captures, etc).
(function () {
    var re = /(?<x>a)/;
    var r = re[Symbol.match]("abc");
    shouldBe(r[0], "a");
    shouldBe(r.index, 0);
    shouldBe(r.groups.x, "a");
})();

// |this| not an object must throw a TypeError.
(function () {
    shouldThrow(function () { RegExp.prototype[Symbol.match].call(null, "x"); }, "TypeError");
    shouldThrow(function () { RegExp.prototype[Symbol.match].call(undefined, "x"); }, "TypeError");
    shouldThrow(function () { RegExp.prototype[Symbol.match].call(42, "x"); }, "TypeError");
})();

// |this| is an arbitrary object (not a RegExpObject): goes to slow path,
// observes flags, exec, lastIndex via property access.
(function () {
    var trace = [];
    var fakeRegExp = {
        get flags() { trace.push("flags"); return ""; },
        get lastIndex() { trace.push("lastIndex.get"); return 0; },
        set lastIndex(v) { trace.push("lastIndex.set:" + v); },
        exec(s) { trace.push("exec:" + s); return null; }
    };
    var r = RegExp.prototype[Symbol.match].call(fakeRegExp, "abc");
    shouldBe(r, null);
    // Non-global path: just calls exec once, no lastIndex writes.
    shouldBe(trace, ["flags", "exec:abc"]);
})();
(function () {
    var trace = [];
    var matches = [{ 0: "a" }, { 0: "" }, null];
    var fakeRegExp = {
        get flags() { return "g"; },
        get lastIndex() { trace.push("lastIndex.get"); return 1; },
        set lastIndex(v) { trace.push("lastIndex.set:" + v); },
        exec(s) {
            trace.push("exec");
            return matches.shift();
        }
    };
    var r = RegExp.prototype[Symbol.match].call(fakeRegExp, "abc");
    shouldBe(r, ["a", ""]);
    // Global path: initial lastIndex set to 0, then exec, then for empty match
    // do lastIndex.get -> AdvanceStringIndex -> lastIndex.set, then exec, then
    // exec returns null and we exit.
    shouldBe(trace, [
        "lastIndex.set:0",
        "exec",
        "exec",
        "lastIndex.get",
        "lastIndex.set:2",
        "exec"
    ]);
})();
