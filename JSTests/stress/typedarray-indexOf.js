load("./resources/typedarray-test-helper-functions.js", "caller relative");
description(
"This test checks the behavior of the TypedArray.prototype.indexOf function"
);

shouldBe("Int32Array.prototype.indexOf.length", "1");
shouldBe("Int32Array.prototype.indexOf.name", "'indexOf'");
shouldBeTrue("isSameFunctionForEachTypedArrayPrototype('indexOf')");
shouldBeTrue("testPrototypeReceivesArray('indexOf', [undefined, this, { }, [ ], true, ''])");
debug("");

debug("testPrototypeFunction has the following arg list (name, args, init, result [ , expectedArray ])");
debug("");

function keepEven(e, i) {
    return !(e & 1) || (this.keep ? this.keep === i : false);
}

var array = [2, 5, 9, 2]

shouldBeTrue("testPrototypeFunction('indexOf', '(2, -500)', array, 0)");
shouldBeTrue("testPrototypeFunction('indexOf', '(9, 500)', array, -1)");
shouldBeTrue("testPrototypeFunction('indexOf', '(2)', array, 0)");
shouldBeTrue("testPrototypeFunction('indexOf', '(7)', array, -1)");
shouldBeTrue("testPrototypeFunction('indexOf', '(2, 3)', array, 3)");
shouldBeTrue("testPrototypeFunction('indexOf', '(2, 2)', array, 3)");
shouldBeTrue("testPrototypeFunction('indexOf', '(2, 0)', array, 0)");
shouldBeTrue("testPrototypeFunction('indexOf', '(2, -1)', array, 3)");
shouldBeTrue("testPrototypeFunction('indexOf', '(2, -2)', array, 3)");
debug("");

debug("Check object coersion");
for (constructor of typedArrays) {
    a = new constructor([0,2,3]);
    passed = true;

    shouldBe("a.indexOf({ valueOf() { passed = false; return 1; }})", "-1");
    shouldBeTrue("passed");
    shouldBe("a.indexOf(3, {valueOf: () => -1})", "2");

    // test we don't coerce non-native values
    shouldBe("a.indexOf(\"abc\")", "-1");
    shouldBe("a.indexOf(null)", "-1");
    shouldBe("a.indexOf(undefined)", "-1");
    shouldBe("a.indexOf({1: ''})", "-1");
    shouldBe("a.indexOf(\"\")", "-1");
}


for (constructor of intArrays) {
    a = new constructor([0,2,3]);

    shouldBe("a.indexOf(2.0)", "1");
    shouldBe("a.indexOf(2.5)", "-1");
}

for (constructor of floatArrays) {
    a = new constructor([0,2.0,3.6, NaN, Infinity]);

    shouldBe("a.indexOf(2.0)", "1");
    shouldBe("a.indexOf(2.5)", "-1");
    shouldBe("a.indexOf(3.600001)", "-1");
    shouldBe("a.indexOf(NaN)", "-1");
    shouldBe("a.indexOf(Infinity)", "4");
}

finishJSTest();
