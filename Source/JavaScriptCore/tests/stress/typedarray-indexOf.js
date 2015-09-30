load("./resources/typedarray-test-helper-functions.js");
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
finishJSTest();
