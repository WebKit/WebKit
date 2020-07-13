load("./resources/typedarray-test-helper-functions.js", "caller relative");
description(
"This test checks the behavior of the TypedArray.prototype.lastIndexOf function"
);

shouldBe("Int32Array.prototype.lastIndexOf.length", "1");
shouldBe("Int32Array.prototype.lastIndexOf.name", "'lastIndexOf'");
shouldBeTrue("isSameFunctionForEachTypedArrayPrototype('lastIndexOf')");
shouldBeTrue("testPrototypeReceivesArray('lastIndexOf', [undefined, this, { }, [ ], true, ''])");
debug("");

debug("testPrototypeFunction has the following arg list (name, args, init, result [ , expectedArray ])");
debug("");

var array = [2, 5, 9, 2]

shouldBeTrue("testPrototypeFunction('lastIndexOf', '(2, -500)', array, -1)");
shouldBeTrue("testPrototypeFunction('lastIndexOf', '(9, 500)', array, 2)");
shouldBeTrue("testPrototypeFunction('lastIndexOf', '(2)', array, 3)");
shouldBeTrue("testPrototypeFunction('lastIndexOf', '(5)', array, 1)");
shouldBeTrue("testPrototypeFunction('lastIndexOf', '(7)', array, -1)");
shouldBeTrue("testPrototypeFunction('lastIndexOf', '(2, 3)', array, 3)");
shouldBeTrue("testPrototypeFunction('lastIndexOf', '(2, 2)', array, 0)");
shouldBeTrue("testPrototypeFunction('lastIndexOf', '(2, 0)', array, 0)");
shouldBeTrue("testPrototypeFunction('lastIndexOf', '(2, -1)', array, 3)");
shouldBeTrue("testPrototypeFunction('lastIndexOf', '(2, -2)', array, 0)");
debug("");

debug("Check object coersion");
for (constructor of typedArrays) {
    a = new constructor([0,2,3]);
    passed = true;

    shouldBe("a.lastIndexOf({ valueOf() { passed = false; return 1; }})", "-1");
    shouldBeTrue("passed");
    shouldBe("a.lastIndexOf(3, {valueOf: () => 3})", "2");

    // test we don't coerce non-native values
    shouldBe("a.lastIndexOf(\"abc\")", "-1");
    shouldBe("a.lastIndexOf(null)", "-1");
    shouldBe("a.lastIndexOf(undefined)", "-1");
    shouldBe("a.lastIndexOf({1: ''})", "-1");
    shouldBe("a.lastIndexOf(\"\")", "-1");
}


for (constructor of intArrays) {
    a = new constructor([0,2,3]);

    shouldBe("a.lastIndexOf(2.0)", "1");
    shouldBe("a.lastIndexOf(2.5)", "-1");
}

for (constructor of floatArrays) {
    a = new constructor([0,2.0,3.6, NaN, Infinity]);

    shouldBe("a.lastIndexOf(2.0)", "1");
    shouldBe("a.lastIndexOf(2.5)", "-1");
    shouldBe("a.lastIndexOf(3.600001)", "-1");
    shouldBe("a.lastIndexOf(NaN)", "-1");
    shouldBe("a.lastIndexOf(Infinity)", "4");
}

finishJSTest();
