load("./resources/typedarray-test-helper-functions.js");
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
finishJSTest();
