load("resources/typedarray-test-helper-functions.js");

for (constructor of typedArrays) {
    let a = new constructor(10);
    passed = true;
    result = a.includes({ valueOf() { passed = false; return 1; } });
    shouldBeTrue("passed");
    shouldBeFalse("result");
}
finishJSTest();
