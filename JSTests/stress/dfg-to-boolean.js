//@ if $buildType == "release" then runDefault else skip end

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`Bad value: ${actual}!`);
}
noInline(shouldBe);

(function ObjectOrOtherUse() {
    function test1(x) { return !!x; }
    function test2(x) { return Boolean(x); }
    function testInvert(x) { return !x; }

    noInline(test1);
    noInline(test2);
    noInline(testInvert);

    for (let i = 0; i < 1e5; i++) {
        shouldBe(test1(undefined), false);
        shouldBe(test1({}), true);
        shouldBe(test2([]), true);
        shouldBe(testInvert(new String("")), false);
        shouldBe(testInvert(null), true);

        shouldBe(test1(makeMasquerader()), false);
        shouldBe(test2(makeMasquerader()), false);
        shouldBe(testInvert(makeMasquerader()), true);
    }
})();

(function Int32Use() {
    function test1(x) { return !!x; }
    function test2(x) { return Boolean(x); }
    function testInvert(x) { return !x; }

    noInline(test1);
    noInline(test2);
    noInline(testInvert);

    for (let i = 0; i < 1e5; i++) {
        shouldBe(test1(1), true);
        shouldBe(test2(0), false);
        shouldBe(test2(2147483647), true);
        shouldBe(testInvert(-2147483648), false);
    }
})();

(function DoubleRepUse() {
    function test1(x) { return !!x; }
    function test2(x) { return Boolean(x); }
    function testInvert(x) { return !x; }

    noInline(test1);
    noInline(test2);
    noInline(testInvert);

    for (let i = 0; i < 1e5; i++) {
        shouldBe(test1(0.5), true);
        shouldBe(test1(-Infinity), true);
        shouldBe(test2(-0), false);
        shouldBe(testInvert(NaN), true);
    }
})();

(function BooleanUse() {
    function test1(x) { return !!x; }
    function test2(x) { return Boolean(x); }
    function testInvert(x) { return !x; }

    noInline(test1);
    noInline(test2);
    noInline(testInvert);

    for (let i = 0; i < 1e5; i++) {
        // needsTypeCheck: false
        shouldBe(test1(true), true);
        shouldBe(test2(false), false);
        shouldBe(testInvert(true), false);

        // needsTypeCheck: true
        shouldBe(!test1(false), true);
        shouldBe(Boolean(test2(true)), true);
        shouldBe(!testInvert(false), false);
    }
})();

(function UntypedUse() {
    function test1(x) { return !!x; }
    function test2(x) { return Boolean(x); }
    function testInvert(x) { return !x; }

    noInline(test1);
    noInline(test2);
    noInline(testInvert);

    const testCases = [
        [undefined, false],
        [null, false],
        [0, false],
        [2147483647, true],
        [false, false],
        [true, true],
        ["" + "" + "", false],
        ["foo", true],
        [-0, false],
        [3.14, true],
        [NaN, false],
        [Infinity, true],
        [Symbol(), true],
        [{}, true],
        [[], true],
        [function() {}, true],
        [makeMasquerader(), false],
    ];

    for (let i = 0; i < 1e5; i++) {
        for (const [value, expected] of testCases) {
            shouldBe(test1(value), expected);
            shouldBe(test2(value), expected);
            shouldBe(testInvert(value), !expected);
        }
    }
})();

(function StringUse() {
    function test1(x) { return !!x; }
    function test2(x) { return Boolean(x); }
    function testInvert(x) { return !x; }

    noInline(test1);
    noInline(test2);
    noInline(testInvert);

    for (let i = 0; i < 1e5; i++) {
        shouldBe(test1("\0"), true);
        shouldBe(test2(""), false);
        shouldBe(testInvert("" + "" + ""), true);
        shouldBe(testInvert("foo" + "foo" + "foo"), false);
    }
})();

(function StringOrOtherUse() {
    function test1(x) { return !!x; }
    function test2(x) { return Boolean(x); }
    function testInvert(x) { return !x; }

    noInline(test1);
    noInline(test2);
    noInline(testInvert);

    for (let i = 0; i < 1e5; i++) {
        shouldBe(test1("" + "" + ""), false);
        shouldBe(test1(null), false);
        shouldBe(test2("foo"), true);
        shouldBe(test2(undefined), false);
        shouldBe(testInvert(""), true);
        shouldBe(testInvert(null), true);
    }
})();
