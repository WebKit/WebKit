load("./resources/typedarray-test-helper-functions.js");
description(
"This test checks the behavior of the TypedArray.prototype.slice function"
);

shouldBe("Int32Array.prototype.slice.length", "2");
shouldBe("Int32Array.prototype.slice.name", "'slice'");
shouldBeTrue("isSameFunctionForEachTypedArrayPrototype('slice')");
shouldBeTrue("testPrototypeReceivesArray('slice', [undefined, this, { }, [ ], true, ''])");
debug("");

debug("testPrototypeFunction has the following arg list (name, args, init, result [ , expectedArray ])");
debug("");

debug("1.0 Test Basic Functionality");
shouldBeTrue("testPrototypeFunction('slice', '(2, 3)', [12, 5, 8, 13, 44], [8], [12, 5, 8, 13, 44])");
shouldBeTrue("testPrototypeFunction('slice', '(5, 5)', [12, 5, 8, 13, 44], [])");
shouldBeTrue("testPrototypeFunction('slice', '(0, 5)', [12, 5, 8, 13, 44], [12, 5, 8, 13, 44])");
shouldBeTrue("testPrototypeFunction('slice', '(0, -5)', [12, 5, 8, 13, 44], [])");
shouldBeTrue("testPrototypeFunction('slice', '(-3, -2)', [12, 5, 8, 13, 44], [8])");
shouldBeTrue("testPrototypeFunction('slice', '(4, 2)', [12, 5, 8, 13, 44], [])");
shouldBeTrue("testPrototypeFunction('slice', '(-50, 50)', [12, 5, 8, 13, 44], [12, 5, 8, 13, 44])");
debug("");

debug("2.0 Preserve Underlying bits");

var intView = new Int32Array(5);
intView[0] = -1;
var floatView = new Float32Array(intView.buffer);
floatView = floatView.slice(0,1);
intView = new Int32Array(floatView.buffer);

shouldBe("intView[0]", "-1");
debug("");

debug("3.0 Creates New Buffer");

var intView = new Int32Array(1)
var newView = intView.slice(0,1);
newView[0] = 1;

shouldBe("intView[0]", "0");
debug("");
finishJSTest();
