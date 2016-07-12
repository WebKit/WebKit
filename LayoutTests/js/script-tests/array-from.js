// [ARM] jsc-layout-tests.yaml/js/script-tests/array-from.js fails on Aarch64
// https://bugs.webkit.org/show_bug.cgi?id=142272
//@ skip if $architecture == "arm64"
description("Tests for Array.from");

function section(title) {
    debug("");
    debug(title);
    debug("-------");
}

section("Length of Array.from");
shouldBe("Array.from.length", "1");

section("Simple construction");
shouldBeTrue("Array.from instanceof Function")
shouldBe("Array.from([1])", "[1]");
shouldBe("Array.from([1, 2, 3])", "[1, 2, 3]");
shouldBe("Array.from([1, 2, 3]).length", "3");
shouldBe("Array.from('abc')", "['a', 'b', 'c']");
shouldBe("Array.from('abc').length", "3");
shouldBe("Array.from(Array.from([4, 5, 6]))", "[4, 5, 6]");
shouldBe("Array.from([null, null])", "[null, null]");
shouldBe("Array.from([]).length", "0");
shouldBe("Array.from(new Uint8Array([1, 2, 3]))", "[1, 2, 3]");

section("Incorrect construction");
shouldThrow("Array.from()");
shouldThrow("Array.from(null)");
shouldThrow("Array.from(undefined)");
debug("Declare wayTooSmall = { length: -1 }");
wayTooSmall = { length: -1 };
shouldBe("Array.from(wayTooSmall)", "[]");
debug("Declare wayTooBig = { length: Infinity }");
wayTooBig = { length: Infinity };
shouldThrow("Array.from(wayTooBig)");

section("Mapped construction");
shouldBe("Array.from([1, 2, 3], function (x) { return x * 10; })", "[10, 20, 30]");
shouldBe("Array.from([1, 2, 3], function (x) { return null; })", "[null, null, null]");
shouldBe("Array.from([1, 2, 3], function (x) { }).length", "3");
shouldBe("Array.from({length: 5}, function(v, k) { return k; })", "[0, 1, 2, 3, 4]");

debug("Declare var bacon = { eggs: 5 }")
var bacon = {
    eggs: 5
};

shouldBe("Array.from([1, 2, 3], function (x) { return x * this.eggs; }, bacon)", "[5, 10, 15]");

section("Incorrect mapped construction");
shouldThrow("Array.from([1, 2, 3], null)");
shouldThrow("Array.from([1, 2, 3], [])");
shouldThrow("Array.from([1, 2, 3], [1])");

section("Weird construction");
shouldBe("Array.from(Math).length", "0");
debug("Declare wayTooWrong = { length: NaN }");
wayTooWrong = { length: NaN };
shouldBe("Array.from(wayTooWrong)", "[]");

section("Array with holes");
arrayWithHoles = [];
arrayWithHoles[3] = true;
arrayWithHoles[9] = "hi";

shouldBe("Array.from(arrayWithHoles)", "[,,, true,,,, , , 'hi']");

section("Modify length during construction");
var crazyPants = {
    _length: 3,
    get 0() {
        return "one";
    },
    get 1() {
        return "two";
    },
    get 2() {
        return "three";
    },
    get 3() {
        return "four";
    },
    get 4() {
        return "ERROR: this should never be called";
    },
    get length() {
        return ++crazyPants._length;
    }
};
shouldBe("Array.from(crazyPants)", "['one', 'two', 'three', 'four']");

section("Modify length during mapping");
crazyPants._length = 3; // Reset the length
shouldBe("Array.from(crazyPants, function (x) { crazyPants.length = x; return x; })", "['one', 'two', 'three', 'four']");

section("Construction using Set object");
var set = new Set;
set.add("zero");
set.add("one");
set.add("two");
shouldBe("Array.from(set)", "['zero', 'one', 'two']");

section("\"this\" is a constructor");
var CustomConstructor = function (length) { this.givenLength = length; };
shouldBe("Array.from.call(CustomConstructor, ['WebKit']).constructor", "CustomConstructor");
shouldBe("Object.getPrototypeOf(Array.from.call(CustomConstructor, ['WebKit']))", "CustomConstructor.prototype");
shouldBe("Array.from.call(nonConstructor, ['WebKit']).length", "1");
shouldBeEqualToString("Array.from.call(nonConstructor, ['WebKit'])[0]", "WebKit");

var nonIterable = {
    get 0() {
        return "one";
    },
    get 1() {
        return 2;
    },
    get 2() {
        throw "ERROR: this should never be called";
    },
    get length() {
        return 2;
    }
};

shouldBe("Array.from.call(CustomConstructor, nonIterable).constructor", "CustomConstructor");
shouldBe("Object.getPrototypeOf(Array.from.call(CustomConstructor, nonIterable))", "CustomConstructor.prototype");
shouldBe("Array.from.call(nonConstructor, nonIterable).length", "2");
shouldBeEqualToString("Array.from.call(nonConstructor, nonIterable)[0]", "one");
shouldBe("Array.from.call(nonConstructor, nonIterable)[1]", "2");


section("\"this\" is not a constructor");
var nonConstructorWasCalled = false;
var nonConstructor = () => { nonConstructorWasCalled = true; };
shouldBe("Array.from.call(nonConstructor, ['WebKit']).constructor", "Array");
shouldBe("Object.getPrototypeOf(Array.from.call(nonConstructor, ['WebKit']))", "Array.prototype");
shouldBe("Array.from.call(nonConstructor, ['WebKit']).length", "1");
shouldBeEqualToString("Array.from.call(nonConstructor, ['WebKit'])[0]", "WebKit");
shouldBeFalse("nonConstructorWasCalled");

shouldBe("Array.from.call(nonConstructor, nonIterable).constructor", "Array");
shouldBe("Object.getPrototypeOf(Array.from.call(nonConstructor, nonIterable))", "Array.prototype");
shouldBe("Array.from.call(nonConstructor, nonIterable).length", "2");
shouldBeEqualToString("Array.from.call(nonConstructor, nonIterable)[0]", "one");
shouldBe("Array.from.call(nonConstructor, nonIterable)[1]", "2");
