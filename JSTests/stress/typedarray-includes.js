load("resources/typedarray-test-helper-functions.js", "caller relative");

for (constructor of typedArrays) {
    a = new constructor(10);
    passed = true;
    result = a.includes({ valueOf() { passed = false; return 1; } });
    shouldBeTrue("passed");
    shouldBeFalse("result");

    shouldBeTrue("a.includes(undefined, { valueOf() { transferArrayBuffer(a.buffer); return 0; } })");
    shouldThrow("a.includes(undefined)");

    a = new constructor(1);
    shouldBeFalse("a.includes(null, { valueOf() { transferArrayBuffer(a.buffer); return 0; } })");
}
finishJSTest();
