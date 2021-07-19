function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`Bad value: ${actual}.`);
}

(function lazyCallbackAccessor1() {
    const lazyProperty = $vm.createStaticLazyProperty();

    const get = () => lazyProperty.testLazyCallbackAccessor;
    noInline(get);

    globalThis.isEnabledLazyCallbackAccessor = false;
    for (let i = 0; i < 1e4; ++i)
        shouldBe(get(), undefined);

    globalThis.isEnabledLazyCallbackAccessor = true;
    shouldBe(get(), lazyProperty);
})();

(function lazyCallbackAccessor2() {
    const lazyProperty = $vm.createStaticLazyProperty();
    Object.setPrototypeOf(lazyProperty, { testLazyCallbackAccessor: 21 });

    const get = () => lazyProperty.testLazyCallbackAccessor;
    noInline(get);

    globalThis.isEnabledLazyCallbackAccessor = false;
    for (let i = 0; i < 1e4; ++i)
        shouldBe(get(), 21);

    globalThis.isEnabledLazyCallbackAccessor = true;
    shouldBe(get(), lazyProperty);
})();

(function lazyCallbackAccessor3() {
    const heir = Object.create($vm.createStaticLazyProperty());

    const get = () => heir.testLazyCallbackAccessor;
    noInline(get);

    globalThis.isEnabledLazyCallbackAccessor = false;
    for (let i = 0; i < 1e4; ++i)
        shouldBe(get(), undefined);

    globalThis.isEnabledLazyCallbackAccessor = true;
    shouldBe(get(), heir);
})();


(function lazyCallbackCustomAccessor1() {
    const testField = {};
    const lazyProperty = $vm.createStaticLazyProperty();
    lazyProperty.testField = testField;

    const get = () => lazyProperty.testLazyCallbackCustomAccessor;
    noInline(get);

    globalThis.isEnabledLazyCallbackCustomAccessor = false;
    for (let i = 0; i < 1e4; ++i)
        shouldBe(get(), undefined);

    globalThis.isEnabledLazyCallbackCustomAccessor = true;
    shouldBe(get(), testField);
})();

(function lazyCallbackCustomAccessor2() {
    const testField = {};
    const lazyProperty = $vm.createStaticLazyProperty();
    lazyProperty.testField = testField;
    Object.setPrototypeOf(lazyProperty, { testLazyCallbackCustomAccessor: 21 });

    const get = () => lazyProperty.testLazyCallbackCustomAccessor;
    noInline(get);

    globalThis.isEnabledLazyCallbackCustomAccessor = false;
    for (let i = 0; i < 1e4; ++i)
        shouldBe(get(), 21);

    globalThis.isEnabledLazyCallbackCustomAccessor = true;
    shouldBe(get(), testField);
})();

(function lazyCallbackCustomAccessor3() {
    const testField = {};
    const heir = Object.create($vm.createStaticLazyProperty());
    heir.testField = testField;

    const get = () => heir.testLazyCallbackCustomAccessor;
    noInline(get);

    globalThis.isEnabledLazyCallbackCustomAccessor = false;
    for (let i = 0; i < 1e4; ++i)
        shouldBe(get(), undefined);

    globalThis.isEnabledLazyCallbackCustomAccessor = true;
    shouldBe(get(), testField);
})();


(function lazyCallbackConstantInteger1() {
    const lazyProperty = $vm.createStaticLazyProperty();

    const get = () => lazyProperty.testLazyCallbackConstantInteger;
    noInline(get);

    globalThis.isEnabledLazyCallbackConstantInteger = false;
    for (let i = 0; i < 1e4; ++i)
        shouldBe(get(), undefined);

    globalThis.isEnabledLazyCallbackConstantInteger = true;
    shouldBe(get(), 42);
})();

(function lazyCallbackConstantInteger2() {
    const lazyProperty = $vm.createStaticLazyProperty();
    Object.setPrototypeOf(lazyProperty, { testLazyCallbackConstantInteger: 21 });

    const get = () => lazyProperty.testLazyCallbackConstantInteger;
    noInline(get);

    globalThis.isEnabledLazyCallbackConstantInteger = false;
    for (let i = 0; i < 1e4; ++i)
        shouldBe(get(), 21);

    globalThis.isEnabledLazyCallbackConstantInteger = true;
    shouldBe(get(), 42);
})();

(function lazyCallbackConstantInteger3() {
    const heir = Object.create($vm.createStaticLazyProperty());

    const get = () => heir.testLazyCallbackConstantInteger;
    noInline(get);

    globalThis.isEnabledLazyCallbackConstantInteger = false;
    for (let i = 0; i < 1e4; ++i)
        shouldBe(get(), undefined);

    globalThis.isEnabledLazyCallbackConstantInteger = true;
    shouldBe(get(), 42);
})();
