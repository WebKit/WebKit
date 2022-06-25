load("./resources/typedarray-test-helper-functions.js", "caller relative");
description("This test checks the behavior of the TypedArray.prototype.copyWithin function");


shouldBe("Int32Array.prototype.copyWithin.length", "2");
shouldBe("Int32Array.prototype.copyWithin.name", "'copyWithin'");

shouldBeTrue("isSameFunctionForEachTypedArrayPrototype('copyWithin')");
shouldBeTrue("testPrototypeReceivesArray('copyWithin', [undefined, this, { }, [ ], true, ''])");

shouldBeTrue("testPrototypeFunction('copyWithin', '(0, 3)', [1, 2, 3, 4, 5], [4, 5, 3, 4, 5])");
shouldBeTrue("testPrototypeFunction('copyWithin', '(0, 3, 4)', [1, 2, 3, 4, 5], [4, 2, 3, 4, 5])");
shouldBeTrue("testPrototypeFunction('copyWithin', '(0, -2, -1)', [1, 2, 3, 4, 5], [4, 2, 3, 4, 5])");
shouldBeTrue("testPrototypeFunction('copyWithin', '(5, -5, 5)', [1, 2, 3, 4, 5], [1, 2, 3, 4, 5])");
shouldBeTrue("testPrototypeFunction('copyWithin', '(1, -5, 5)', [1, 2, 3, 4, 5], [1, 1, 2, 3, 4])");
finishJSTest();
