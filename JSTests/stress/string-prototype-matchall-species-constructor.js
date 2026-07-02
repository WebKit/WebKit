// Verifies that String.prototype.matchAll and RegExp.prototype[@@matchAll] observe
// SpeciesConstructor(R, %RegExp%): a custom species constructor must be called, and a
// non-object RegExp.prototype.constructor must cause a TypeError, even after the C++
// fast path has been warmed up.

function shouldBe(actual, expected) {
    var a = JSON.stringify(actual);
    var e = JSON.stringify(expected);
    if (a !== e)
        throw new Error("expected " + e + " but got " + a);
}

function shouldThrow(func, errorMessage) {
    var error = null;
    try {
        func();
    } catch (e) {
        error = e;
    }
    if (!error)
        throw new Error("not thrown");
    if (String(error) !== errorMessage)
        throw new Error("bad error: " + String(error));
}

function matchAllToArray(str, re) {
    return [...str.matchAll(re)];
}
noInline(matchAllToArray);

for (var i = 0; i < testLoopCount; ++i)
    shouldBe(matchAllToArray("a1b2", /[0-9]/g), [["1"], ["2"]]);

// 1. A custom species constructor must be observed: its return value becomes the matcher.
(function () {
    var calls = [];
    var savedConstructor = RegExp.prototype.constructor;
    RegExp.prototype.constructor = {
        [Symbol.species]: function (re, flags) {
            calls.push(flags);
            return new RegExp("b", flags);
        }
    };
    try {
        shouldBe(matchAllToArray("ab", /a/g), [["b"]]);
        shouldBe(calls, ["g"]);

        calls.length = 0;
        shouldBe([.../a/g[Symbol.matchAll]("ab")], [["b"]]);
        shouldBe(calls, ["g"]);
    } finally {
        RegExp.prototype.constructor = savedConstructor;
    }
})();

// 2. A non-object constructor must cause SpeciesConstructor to throw a TypeError.
(function () {
    RegExp.prototype.constructor = 5;
    shouldThrow(() => "x".matchAll(/y/g), "TypeError: |this|.constructor is not an Object or undefined");
    shouldThrow(() => /y/g[Symbol.matchAll]("x"), "TypeError: |this|.constructor is not an Object or undefined");
    // Primitive pattern: matchAll creates a fresh global RegExp internally, which still
    // inherits the poisoned constructor.
    shouldThrow(() => "x".matchAll("y"), "TypeError: |this|.constructor is not an Object or undefined");
})();
