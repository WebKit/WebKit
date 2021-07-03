function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`Bad value: ${actual}.`);
}

function shouldThrow(func, errorMessage) {
    let errorThrown = false;
    try {
        func();
    } catch (error) {
        errorThrown = true;
        if (error.toString() !== errorMessage)
            throw new Error(`Bad error: ${error}`);
    }
    if (!errorThrown)
        throw new Error("Didn't throw!");
}

(function lazyCallbackAccessor1() {
    const lazyProperty = $vm.createStaticLazyProperty();

    Object.setPrototypeOf(lazyProperty, Object.create(null, {
        testLazyCallbackAccessor: Object.getOwnPropertyDescriptor($vm.createCustomTestGetterSetter(), "customAccessor"),
    }));

    function putStrict(val) { "use strict"; lazyProperty.testLazyCallbackAccessor = val; };
    noInline(putStrict);

    globalThis.isEnabledLazyCallbackAccessor = false;
    for (let i = 1; i <= 1e4; ++i) {
        const value = {};
        putStrict(value);
        shouldBe(value.result, lazyProperty);
    }

    globalThis.isEnabledLazyCallbackAccessor = true;
    const value = {};
    shouldThrow(() => { putStrict({}); }, "TypeError: Attempted to assign to readonly property.");
    shouldBe(value.hasOwnProperty("value"), false);
})();

(function lazyCallbackAccessor2() {
    const lazyProperty = $vm.createStaticLazyProperty();
    const heir = Object.create(lazyProperty);

    Object.setPrototypeOf(lazyProperty, Object.create(null, {
        testLazyCallbackAccessor: Object.getOwnPropertyDescriptor($vm.createStaticCustomAccessor(), "testStaticAccessor"),
    }));

    function putToHeirSloppy(val) { heir.testLazyCallbackAccessor = val; };
    noInline(putToHeirSloppy);

    globalThis.isEnabledLazyCallbackAccessor = false;
    for (let i = 1; i <= 1e4; ++i) {
        putToHeirSloppy(i);
        shouldBe(heir.testField, i);
    }

    globalThis.isEnabledLazyCallbackAccessor = true;
    putToHeirSloppy(42);
    shouldBe(heir.testField, 1e4);
})();


(function lazyCallbackCustomAccessor1() {
    const lazyProperty = $vm.createStaticLazyProperty();

    let lastSetValue;
    Object.setPrototypeOf(lazyProperty, {
        set testLazyCallbackCustomAccessor(val) { lastSetValue = val; },
    });

    function putStrict(val) { "use strict"; lazyProperty.testLazyCallbackCustomAccessor = val; };
    noInline(putStrict);

    globalThis.isEnabledLazyCallbackCustomAccessor = false;
    for (let i = 1; i <= 1e4; ++i) {
        putStrict(i);
        shouldBe(lastSetValue, i);
    }

    globalThis.isEnabledLazyCallbackCustomAccessor = true;
    shouldBe(lazyProperty.testField, undefined);
    putStrict(42);
    shouldBe(lazyProperty.testField, 42);
})();

(function lazyCallbackCustomAccessor2() {
    const lazyProperty = $vm.createStaticLazyProperty();
    const heir = Object.create(lazyProperty);

    let lastSetValue;
    Object.setPrototypeOf(lazyProperty, {
        get testLazyCallbackCustomAccessor() {},
        set testLazyCallbackCustomAccessor(val) { lastSetValue = val; },
    });

    function putToHeirSloppy(val) { "use strict"; heir.testLazyCallbackCustomAccessor = val; };
    noInline(putToHeirSloppy);

    globalThis.isEnabledLazyCallbackCustomAccessor = false;
    for (let i = 1; i <= 1e4; ++i) {
        putToHeirSloppy(i);
        shouldBe(lastSetValue, i);
    }

    globalThis.isEnabledLazyCallbackCustomAccessor = true;
    shouldBe(heir.testField, undefined);
    putToHeirSloppy(42);
    shouldBe(heir.testField, 42);
})();


(function lazyCallbackConstantInteger1() {
    const lazyProperty = $vm.createStaticLazyProperty();

    let lastSetValue = 0;
    Object.setPrototypeOf(lazyProperty, {
        get testLazyCallbackConstantInteger() {},
        set testLazyCallbackConstantInteger(val) { lastSetValue = val; },
    });

    function putSloppy(val) { lazyProperty.testLazyCallbackConstantInteger = val; };
    noInline(putSloppy);

    globalThis.isEnabledLazyCallbackConstantInteger = false;
    for (let i = 1; i <= 1e4; ++i) {
        putSloppy(i);
        shouldBe(lastSetValue, i);
    }

    globalThis.isEnabledLazyCallbackConstantInteger = true;
    putSloppy(42);
    shouldBe(lastSetValue, 1e4);
})();

(function lazyCallbackConstantInteger2() {
    const lazyProperty = $vm.createStaticLazyProperty();
    const heir = Object.create(lazyProperty);

    let lastSetValue = 0;
    Object.setPrototypeOf(lazyProperty, {
        set testLazyCallbackConstantInteger(val) { lastSetValue = val; },
    });

    function putToHeirStrict(val) { "use strict"; heir.testLazyCallbackConstantInteger = val; }
    noInline(putToHeirStrict);

    globalThis.isEnabledLazyCallbackConstantInteger = false;
    for (let i = 1; i <= 1e4; ++i) {
        putToHeirStrict(i);
        shouldBe(lastSetValue, i);
    }

    globalThis.isEnabledLazyCallbackConstantInteger = true;
    shouldThrow(() => { putToHeirStrict(42); }, "TypeError: Attempted to assign to readonly property.");
    shouldBe(lastSetValue, 1e4);
})();
