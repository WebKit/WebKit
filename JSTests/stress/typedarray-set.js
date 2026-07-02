load("./resources/typedarray-test-helper-functions.js", "caller relative");
description("This test checks the behavior of the TypedArray.prototype.set function");

shouldBe("Int32Array.prototype.set.length", "1");
shouldBe("Int32Array.prototype.set.name", "'set'");

shouldBeTrue("isSameFunctionForEachTypedArrayPrototype('set')");
shouldBeTrue("testPrototypeReceivesArray('set', [undefined, this, { }, [ ], true, ''])");
debug("");

debug("1.0 Normal Calls");
shouldBeTrue("testPrototypeFunction('set', '([2, 3, 4])', [1, 2, 3, 4, 5], undefined, [2, 3, 4, 4, 5])");
debug("This next should pass because -.1 when converted to an integer is -0");
shouldBeTrue("testPrototypeFunction('set', '([2, 3, 4], -.1)', [1, 2, 3, 4, 5], undefined, [2, 3, 4, 4, 5])");
shouldBeTrue("testPrototypeFunction('set', '([2, 3, 4], 2)', [1, 2, 3, 4, 5], undefined, [1, 2, 2, 3, 4])");
shouldBeTrue("testPrototypeFunction('set', '([], 5)', [1, 2, 3, 4, 5], undefined, [1, 2, 3, 4, 5])");
shouldBeTrue("testPrototypeFunction('set', '([])', [1, 2, 3, 4, 5], undefined, [1, 2, 3, 4, 5])");
debug("");

debug("2.0 Bad Range Test");
shouldThrow("testPrototypeFunction('set', '([], -1)', [1, 2, 3, 4, 5], false, false)", "'RangeError: Offset should not be negative'");
shouldThrow("testPrototypeFunction('set', '([2, 3, 4], -1)', [1, 2, 3, 4, 5], false, false)", "'RangeError: Offset should not be negative'");
shouldThrow("testPrototypeFunction('set', '([2, 3, 4], -1.23412)', [1, 2, 3, 4, 5], false, false)", "'RangeError: Offset should not be negative'");
shouldThrow("testPrototypeFunction('set', '([2, 3, 4], 1000)', [1, 2, 3, 4, 5], false, false)", "'RangeError: Range consisting of offset and length are out of bounds'");
shouldThrow("testPrototypeFunction('set', '([2, 3, 4], 1e42*1.2434325231)', [1, 2, 3, 4, 5], false, false)", "'RangeError: Range consisting of offset and length are out of bounds'");

finishJSTest();
