// Exercises edge cases of String.prototype.match across the LLInt/Baseline/DFG/FTL
// tiers, in particular around the C++ host function and the StringMatch DFG node.

function shouldBe(actual, expected) {
    var a = JSON.stringify(actual);
    var e = JSON.stringify(expected);
    if (a !== e)
        throw new Error("expected " + e + " but got " + a);
}

function shouldThrow(fn, expectedMessage) {
    var threw = false;
    try {
        fn();
    } catch (e) {
        threw = true;
        if (expectedMessage && String(e).indexOf(expectedMessage) === -1)
            throw new Error("expected error message to contain " + JSON.stringify(expectedMessage) + " but got " + JSON.stringify(String(e)));
    }
    if (!threw)
        throw new Error("expected exception but none was thrown");
}

// ----- |this| coercion -----
shouldBe("abc1".match(/[0-9]/), ["1"]);
shouldBe(String.prototype.match.call(12345, /3/), ["3"]);
shouldBe(String.prototype.match.call(true, /ru/), ["ru"]);
shouldBe(String.prototype.match.call({ toString() { return "obj1"; } }, /[0-9]/), ["1"]);
shouldThrow(() => String.prototype.match.call(null, /a/), "TypeError");
shouldThrow(() => String.prototype.match.call(undefined, /a/), "TypeError");

// ----- regexp argument types -----
// undefined / no argument: RegExpCreate(undefined, undefined) -> /(?:)/
shouldBe("abc".match(), [""]);
shouldBe("abc".match(undefined), [""]);
// null -> /null/
shouldBe("anull".match(null), ["null"]);
shouldBe("abc".match(null), null);
// number -> /123/
shouldBe("a123b".match(123), ["123"]);
// boolean -> /true/
shouldBe("xtruey".match(true), ["true"]);
// string regexp pattern
shouldBe("abc1def2".match("[0-9]"), ["1"]);
shouldBe("aaa".match("a+"), ["aaa"]);
// invalid regexp pattern from string -> SyntaxError
shouldThrow(() => "abc".match("["), "SyntaxError");
shouldThrow(() => "abc".match("(?"), "SyntaxError");
// Symbol cannot be coerced to a string when constructing the regexp.
shouldThrow(() => "abc".match(Symbol("s")), "TypeError");

// ----- flag combinations -----
shouldBe("abc1def2ghi3".match(/[0-9]/g), ["1", "2", "3"]);
shouldBe("abc1def2".match(/[0-9]/), ["1"]);
shouldBe("1abc".match(/[0-9]/y), ["1"]);
shouldBe("abc1".match(/[0-9]/y), null);
shouldBe("ABC".match(/abc/i), ["ABC"]);
shouldBe("a\nb".match(/^b/m), ["b"]);
shouldBe("a\u{1F600}b".match(/./gu), ["a", "\u{1F600}", "b"]);
shouldBe("a\u{1F600}b".match(/./gv), ["a", "\u{1F600}", "b"]);
// Capture groups
shouldBe("ab".match(/(a)(b)/), ["ab", "a", "b"]);
shouldBe("ab".match(/(?<x>a)(?<y>b)/).groups, { x: "a", y: "b" });

// ----- lastIndex behavior -----
{
    var g = /[0-9]/g;
    g.lastIndex = 5;
    // RegExp.prototype[@@match] sets lastIndex to 0 for global regexps before iterating.
    shouldBe("1a2b3".match(g), ["1", "2", "3"]);
    shouldBe(g.lastIndex, 0);
}
{
    var y = /[0-9]/y;
    y.lastIndex = 3;
    shouldBe("abc1def".match(y), ["1"]);
    shouldBe(y.lastIndex, 4);
}
{
    // Non-numeric lastIndex must not break the spec semantics: the watchpoint fast
    // path bails out and the full lookup path is used.
    var g2 = /[0-9]/g;
    g2.lastIndex = { valueOf() { return 0; } };
    shouldBe("1a2b3".match(g2), ["1", "2", "3"]);
}

// ----- per-instance @@match override on a RegExp -----
{
    var re = /a/;
    re[Symbol.match] = function (s) { return ["instance-override", s]; };
    shouldBe("hello".match(re), ["instance-override", "hello"]);
}
// per-instance @@match deleted -> falls back to RegExp.prototype[@@match]
{
    var re2 = /[0-9]/;
    re2[Symbol.match] = undefined;
    // GetMethod returns undefined -> falls through to step 3-5: ToString(/[0-9]/) = "/[0-9]/"
    // and "abc1" matched against /\/[0-9]\// (literal slash) -> null
    shouldBe("abc1".match(re2), null);
    shouldBe("/3/x".match(re2), ["/3/"]);
}
{
    var re3 = /[0-9]/;
    re3[Symbol.match] = null;
    // GetMethod returns undefined for null too -> same as above
    shouldBe("/3/x".match(re3), ["/3/"]);
}
{
    var re4 = /[0-9]/;
    re4[Symbol.match] = "not callable";
    shouldThrow(() => "abc1".match(re4), "TypeError");
}

// ----- @@match on a plain object (custom matcher) -----
{
    var obj = { [Symbol.match](s) { return [s, "custom"]; } };
    shouldBe("hello".match(obj), ["hello", "custom"]);
}
{
    var obj2 = { [Symbol.match]: 42 };
    shouldThrow(() => "hello".match(obj2), "TypeError");
}
// @@match getter side effects
{
    var log = [];
    var obj3 = {
        get [Symbol.match]() {
            log.push("get @@match");
            return function (s) { log.push("call @@match"); return [s]; };
        }
    };
    shouldBe("hi".match(obj3), ["hi"]);
    shouldBe(log, ["get @@match", "call @@match"]);
}

// ----- Subclassed RegExp -----
{
    class MyRegExp extends RegExp { }
    var mr = new MyRegExp("[0-9]", "g");
    shouldBe("a1b2c3".match(mr), ["1", "2", "3"]);
}
{
    class MyRegExp2 extends RegExp {
        [Symbol.match](s) { return ["subclass", s]; }
    }
    var mr2 = new MyRegExp2("a");
    shouldBe("hello".match(mr2), ["subclass", "hello"]);
}

// ----- flags getter side effects via custom regexp object -----
{
    var log = [];
    var fake = {
        [Symbol.match]: RegExp.prototype[Symbol.match],
        get flags() { log.push("flags"); return "g"; },
        get global() { log.push("global"); return true; },
        get hasIndices() { return false; },
        get ignoreCase() { return false; },
        get multiline() { return false; },
        get sticky() { return false; },
        get unicode() { return false; },
        get unicodeSets() { return false; },
        get dotAll() { return false; },
        lastIndex: 0,
        exec(s) {
            log.push("exec");
            if (this.lastIndex >= s.length) return null;
            var idx = this.lastIndex;
            this.lastIndex = idx + 1;
            return Object.assign([s[idx]], { index: idx, input: s });
        }
    };
    shouldBe("ab".match(fake), ["a", "b"]);
    if (log.indexOf("flags") === -1)
        throw new Error("custom flags getter must be invoked");
    if (log.indexOf("exec") === -1)
        throw new Error("custom exec must be invoked");
}
