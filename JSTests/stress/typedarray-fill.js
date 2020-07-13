load("./resources/typedarray-test-helper-functions.js", "caller relative");
description(
"This test checks the behavior of the TypedArray.prototype.fill function"
);

shouldBe("Int32Array.prototype.fill.length", "1");
shouldBe("Int32Array.prototype.fill.name", "'fill'");
shouldBeTrue("isSameFunctionForEachTypedArrayPrototype('fill')");
shouldBeTrue("testPrototypeReceivesArray('fill', [undefined, this, { }, [ ], true, ''])");
debug("");

debug("testPrototypeFunction has the following arg list (name, args, init, result [ , expectedArray ])");
debug("");

debug("1.0 Single Argument Testing");
shouldBeTrue("testPrototypeFunction('fill', '(12)', [15, 5, 8, 13, 44], [12,12,12,12,12])");
shouldBeTrue("testPrototypeFunction('fill', '(true)', [12, 54, 18, 13, 44], [1,1,1,1,1])");
debug("");

debug("2.0 Two Argument Testing");
shouldBeTrue("testPrototypeFunction('fill', '(12, 2)', [14, 15, 10, 13, 44], [14, 15, 12, 12, 12])");
shouldBeTrue("testPrototypeFunction('fill', '(4, NaN)', [14, 15, 10, 13, 44], [4, 4, 4, 4, 4])");
shouldBeTrue("testPrototypeFunction('fill', '(4, -5)', [14, 15, 10, 13, 44], [4, 4, 4, 4, 4])");
shouldBeTrue("testPrototypeFunction('fill', '(4, -1)', [14, 15, 10, 13, 44], [14, 15, 10, 13, 4])");
debug("");

debug("3.0 Three Argument Testing");
shouldBeTrue("testPrototypeFunction('fill', '(4, -1, 0)', [14, 15, 10, 13, 44], [14, 15, 10, 13, 44])");
shouldBeTrue("testPrototypeFunction('fill', '(4, 1, 1)', [14, 15, 10, 13, 44], [14, 15, 10, 13, 44])");
shouldBeTrue("testPrototypeFunction('fill', '(4, 1, NaN)', [14, 15, 10, 13, 44], [14, 15, 10, 13, 44])");
shouldBeTrue("testPrototypeFunction('fill', '(4, NaN, NaN)', [14, 15, 10, 13, 44], [14, 15, 10, 13, 44])");
shouldBeTrue("testPrototypeFunction('fill', '(4, NaN, 5)', [14, 15, 10, 13, 44], [4, 4, 4, 4, 4])");
shouldBeTrue("testPrototypeFunction('fill', '(4, -3, -2)', [14, 15, 10, 13, 44], [14, 15, 4, 13, 44])");
shouldBeTrue("testPrototypeFunction('fill', '(4, 5, 5)', [14, 15, 10, 13, 44], [14, 15, 10, 13, 44])");

debug("4.0 Coercion Testing");
for (constructor of typedArrays) {
    count = 0;
    let p = new Proxy({}, { get(target, name) {
        count++;
        return target[name];
    }});
    new constructor(10).fill(p);
    shouldBeTrue("count === 40");
}



finishJSTest();
