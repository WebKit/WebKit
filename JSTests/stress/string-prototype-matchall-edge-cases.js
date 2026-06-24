// Exercises edge cases of String.prototype.matchAll across LLInt/Baseline/DFG/FTL tiers.
// Mirrors string-prototype-match-edge-cases.js, adapted for matchAll's spec semantics
// (mandatory "g" flag, returns an iterator, dispatches via @@matchAll).

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

function arrFromIter(it) {
    return [...it].map(m => [...m]);
}

// ----- |this| coercion -----
shouldBe(arrFromIter("abc1".matchAll(/[0-9]/g)), [["1"]]);
shouldBe(arrFromIter(String.prototype.matchAll.call(12345, /[0-9]/g)), [["1"], ["2"], ["3"], ["4"], ["5"]]);
shouldBe(arrFromIter(String.prototype.matchAll.call(true, /[a-z]/g)), [["t"], ["r"], ["u"], ["e"]]);
shouldBe(arrFromIter(String.prototype.matchAll.call({ toString() { return "obj1"; } }, /[0-9]/g)), [["1"]]);
shouldThrow(() => String.prototype.matchAll.call(null, /a/g), "TypeError");
shouldThrow(() => String.prototype.matchAll.call(undefined, /a/g), "TypeError");

// ----- non-global regexp argument throws -----
shouldThrow(() => "abc".matchAll(/a/), "TypeError");
shouldThrow(() => "abc".matchAll(/a/i), "TypeError");
shouldThrow(() => "abc".matchAll(/a/y), "TypeError");

// ----- regexp argument types (non-RegExp -> RegExpCreate(arg, "g")) -----
shouldBe(arrFromIter("abc".matchAll()), [[""], [""], [""], [""]]);  // /(?:)/g
shouldBe(arrFromIter("abc".matchAll(undefined)), [[""], [""], [""], [""]]);
shouldBe(arrFromIter("anull".matchAll(null)), [["null"]]);
shouldBe(arrFromIter("a123b".matchAll(123)), [["123"]]);
shouldBe(arrFromIter("xtruey".matchAll(true)), [["true"]]);
shouldBe(arrFromIter("abc1def2".matchAll("[0-9]")), [["1"], ["2"]]);
shouldBe(arrFromIter("aaa".matchAll("a+")), [["aaa"]]);

// invalid regexp pattern from string -> SyntaxError
shouldThrow(() => "abc".matchAll("["), "SyntaxError");
shouldThrow(() => "abc".matchAll("(?"), "SyntaxError");

// Symbol cannot be coerced to a string when constructing the regexp.
shouldThrow(() => "abc".matchAll(Symbol("s")), "TypeError");

// ----- flag combinations (must include "g") -----
shouldBe(arrFromIter("abc1def2ghi3".matchAll(/[0-9]/g)), [["1"], ["2"], ["3"]]);
shouldBe(arrFromIter("ABC".matchAll(/abc/gi)), [["ABC"]]);
shouldBe(arrFromIter("a\nb".matchAll(/^./gm)), [["a"], ["b"]]);
shouldBe(arrFromIter("a\u{1F600}b".matchAll(/./gu)), [["a"], ["\u{1F600}"], ["b"]]);
shouldBe(arrFromIter("a\u{1F600}b".matchAll(/./gv)), [["a"], ["\u{1F600}"], ["b"]]);

// Capture groups
{
    var arr = arrFromIter("abcd".matchAll(/(.)(.)/g));
    shouldBe(arr, [["ab", "a", "b"], ["cd", "c", "d"]]);
}
{
    var [m] = "ab".matchAll(/(?<x>a)(?<y>b)/g);
    shouldBe(m.groups, { x: "a", y: "b" });
}

// ----- lastIndex behavior -----
{
    // matchAll preserves the source RegExp's lastIndex by copying onto a new matcher.
    var g = /[0-9]/g;
    g.lastIndex = 3;
    shouldBe(arrFromIter("a1b2c3".matchAll(g)), [["2"], ["3"]]);
    // The original's lastIndex must not have been mutated by matchAll.
    shouldBe(g.lastIndex, 3);
}
{
    // Non-numeric lastIndex: fast path bails and the spec path kicks in.
    var g2 = /[0-9]/g;
    g2.lastIndex = { valueOf() { return 0; } };
    shouldBe(arrFromIter("a1b2".matchAll(g2)), [["1"], ["2"]]);
}

// ----- per-instance @@matchAll override on a RegExp -----
{
    var re = /a/g;
    re[Symbol.matchAll] = function (s) { return ["instance-override:" + s]; };
    shouldBe("hello".matchAll(re), ["instance-override:hello"]);
}
// per-instance @@matchAll deleted -> falls back to RegExp.prototype[@@matchAll].
{
    var re2 = /a/g;
    re2[Symbol.matchAll] = undefined;
    // GetMethod returns undefined -> falls through to step 3-5: ToString(this) + RegExpCreate(re2, "g").
    // re2.toString() yields "/a/g" so the constructed regexp is /\/a\/g/g, no match in "hello".
    shouldBe(arrFromIter("hello".matchAll(re2)), []);
    shouldBe(arrFromIter("/a/g/a/g".matchAll(re2)), [["/a/g"], ["/a/g"]]);
}
{
    var re3 = /a/g;
    re3[Symbol.matchAll] = "not callable";
    shouldThrow(() => "abc".matchAll(re3), "TypeError");
}

// ----- @@matchAll on a plain object (custom matcher) -----
{
    // Non-RegExp object: IsRegExp returns false (no @@match, no [[RegExpMatcher]]).
    // Thus the flags check is skipped, and @@matchAll is invoked directly.
    var obj = { [Symbol.matchAll](s) { return [s, "custom"]; } };
    shouldBe("hello".matchAll(obj), ["hello", "custom"]);
}
{
    var obj2 = { [Symbol.matchAll]: 42 };
    shouldThrow(() => "hello".matchAll(obj2), "TypeError");
}
// @@matchAll getter side effects.
{
    var log = [];
    var obj3 = {
        get [Symbol.matchAll]() {
            log.push("get @@matchAll");
            return function (s) { log.push("call @@matchAll"); return [s]; };
        }
    };
    shouldBe("hi".matchAll(obj3), ["hi"]);
    shouldBe(log, ["get @@matchAll", "call @@matchAll"]);
}

// ----- Custom @@match makes IsRegExp return true; flags check then runs. -----
{
    var fakeRegExp = {
        [Symbol.match]: true,
        get flags() { return "g"; },
        [Symbol.matchAll](s) { return ["fake:" + s]; },
    };
    shouldBe("xyz".matchAll(fakeRegExp), ["fake:xyz"]);
}
{
    var fakeNonGlobal = {
        [Symbol.match]: true,
        get flags() { return ""; },
        [Symbol.matchAll](s) { return ["fake:" + s]; },
    };
    shouldThrow(() => "xyz".matchAll(fakeNonGlobal), "TypeError");
}

// ----- Subclassed RegExp -----
{
    class MyRegExp extends RegExp { }
    var mr = new MyRegExp("[0-9]", "g");
    shouldBe(arrFromIter("a1b2c3".matchAll(mr)), [["1"], ["2"], ["3"]]);
}
{
    class MyRegExp2 extends RegExp {
        [Symbol.matchAll](s) { return ["subclass:" + s]; }
    }
    var mr2 = new MyRegExp2("a", "g");
    shouldBe("hello".matchAll(mr2), ["subclass:hello"]);
}

// ----- iterator protocol -----
{
    var iter = "abc".matchAll(/./g);
    shouldBe(typeof iter[Symbol.iterator], "function");
    shouldBe(iter[Symbol.iterator]() === iter, true);
}
{
    var iter = "ab".matchAll(/./g);
    shouldBe(iter.next().value[0], "a");
    shouldBe(iter.next().value[0], "b");
    shouldBe(iter.next().done, true);
}

// ----- empty-string regex iterates by codepoint position -----
{
    shouldBe(arrFromIter("ab".matchAll(/(?:)/g)), [[""], [""], [""]]);
    shouldBe(arrFromIter("a\u{1F600}b".matchAll(/(?:)/gu)), [[""], [""], [""], [""]]);
}

// ----- matched index / input -----
{
    var [m1, m2] = "abc1def2".matchAll(/[0-9]/g);
    shouldBe(m1.index, 3);
    shouldBe(m1.input, "abc1def2");
    shouldBe(m2.index, 7);
    shouldBe(m2.input, "abc1def2");
}
