function get(a, i) { return a[i]; }
function getConstant2(a) { return a[2]; }
function getConstantNegative(a) { return a[-1]; }
function getConstantTrue(a) { return a[true]; }
noInline(get); noInline(getConstant2); noInline(getConstantNegative); noInline(getConstantTrue);
function shouldBe(actual, expected, m) { if (!Object.is(actual, expected)) throw new Error(m + ": " + String(actual) + " expected " + String(expected)); }
var ints = [1, 2, 3];
var objs = [{ a: 1 }, "x", null];
var dbls = [1.5, 2.5];
var holey = [1, , 3];
Object.prototype[1] = "proto";
var str = "abc";
var ta = new Int32Array([7, 8]);
var cow = [10, 20, 30];
var dict = { 0: "d0", 1: "d1" };
var withGetter = [1, 2];
Object.defineProperty(withGetter, 0, { get() { return "getter"; } });
for (var n = 0; n < testLoopCount; ++n) {
    shouldBe(get(ints, 0), 1, "ints0");
    shouldBe(get(ints, 2), 3, "ints2");
    shouldBe(get(ints, 3), undefined, "ints oob");
    shouldBe(get(ints, -1), undefined, "ints neg");
    shouldBe(get(objs, 1), "x", "objs1");
    shouldBe(get(objs, 2), null, "objs2");
    shouldBe(get(dbls, 1), 2.5, "dbls");
    shouldBe(get(holey, 1), "proto", "hole");
    shouldBe(get(str, 1), "b", "str");
    shouldBe(get(ta, 1), 8, "typed");
    shouldBe(get(cow, 2), 30, "cow");
    shouldBe(get(dict, 1), "d1", "dict");
    shouldBe(get(withGetter, 0), "getter", "getter");
    shouldBe(get(ints, 1.5), undefined, "fractional");
    shouldBe(get(ints, "1"), 2, "string index");
    shouldBe(getConstant2(ints), 3, "constant index");
    shouldBe(getConstant2(holey), 3, "constant index holey");
    shouldBe(getConstant2([1]), undefined, "constant index oob");
    shouldBe(getConstantNegative(ints), undefined, "constant negative index");
    shouldBe(getConstantTrue(ints), undefined, "constant non-int index");
    shouldBe(getConstantTrue({ true: "t" }), "t", "constant non-int index hit");
}
