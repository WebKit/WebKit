function add(a, b) { return a + b; }
function sub(a, b) { return a - b; }
noInline(add); noInline(sub);
function shouldBe(actual, expected) { if (!Object.is(actual, expected)) throw new Error("bad value: " + String(actual) + " expected " + String(expected)); }
for (var i = 0; i < testLoopCount / 10; ++i) { shouldBe(add(1.5, 2.25), 3.75); shouldBe(sub(1.5, 2.25), -0.75); }
var object = { valueOf() { return 3; }, toString() { return "obj"; } };
for (var i = 0; i < testLoopCount / 10; ++i) {
    shouldBe(add(1.5, 2), 3.5); shouldBe(add(2, 1.5), 3.5); shouldBe(add(3, 4), 7); shouldBe(add(2147483647, 1), 2147483648);
    shouldBe(add(NaN, 2.5), NaN); shouldBe(add(2.5, NaN), NaN); shouldBe(add(-0, -0), -0); shouldBe(add(1.5, "2"), "1.52"); shouldBe(add("a", 1.5), "a1.5");
    shouldBe(add(true, 2.5), 3.5); shouldBe(add(null, 2.5), 2.5); shouldBe(add(undefined, 2.5), NaN); shouldBe(add(1.5, object), 4.5);
    shouldBe(add(2n, 3n), 5n); shouldBe(add(1.5, Infinity), Infinity);
    shouldBe(sub(1.5, 2), -0.5); shouldBe(sub(2, 1.5), 0.5); shouldBe(sub(3, 4), -1); shouldBe(sub(-2147483648, 1), -2147483649);
    shouldBe(sub(NaN, 2.5), NaN); shouldBe(sub(2.5, NaN), NaN); shouldBe(sub(-0, 0), -0); shouldBe(sub(1.5, "2"), -0.5); shouldBe(sub(true, 2.5), -1.5);
    shouldBe(sub(null, 2.5), -2.5); shouldBe(sub(undefined, 2.5), NaN); shouldBe(sub(1.5, object), -1.5); shouldBe(sub(5n, 3n), 2n);
}
