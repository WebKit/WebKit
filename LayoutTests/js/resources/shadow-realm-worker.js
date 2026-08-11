importScripts("../../resources/js-test-pre.js");

function assert_equals(x, y) {
    if (x === y) return;
    throw new Error(`assertion failure: expected ${x}, got ${y}`);
}

function wrappedLog(prefix) {
    return function (msg) {
        debug(prefix + ": " + msg);
    };
}


async function test(key, f) {
    try {
        await f();
    } catch (e) {
        testFailed(key + e.toString());
        return;
    }
    testPassed(key);
}

(async function () {
    await test("import-value", async () => {
        const outerShadowRealm = new ShadowRealm();
        const checkFn = await outerShadowRealm.importValue("./example-module.js", "check");
        assert_equals(checkFn(wrappedLog("shadowRealm")), true);

        const ourModule = await import("./example-module.js");
        assert_equals(ourModule.value, true, "bloop");
        ourModule.setValue(42);
        assert_equals(ourModule.value, 42);

        const importedVal = await outerShadowRealm.importValue("./example-module.js", "value");
        assert_equals(importedVal, true);
        const setValueImported = await outerShadowRealm.importValue("./example-module.js", "setValue");
        setValueImported(100);
        const importedVal2 = await outerShadowRealm.importValue("./example-module.js", "value");
        assert_equals(importedVal2, 100);
        assert_equals(ourModule.value, 42);
    });

    await test("nested", async () => {
        const outerShadowRealm = new ShadowRealm();
        const checkFn = await outerShadowRealm.importValue("./example-module.js", "check_nested");
        assert_equals(checkFn(wrappedLog("shadowRealm")), true);
    });
    finishJSTest();
})();
