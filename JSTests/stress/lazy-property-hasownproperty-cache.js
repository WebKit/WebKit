function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`Bad value: ${actual}.`);
}

(function lazyCallbackAccessor() {
    const lazyProperty = $vm.createStaticLazyProperty();
    Object.setPrototypeOf(lazyProperty, Object.prototype);

    globalThis.isEnabledLazyCallbackConstantInteger = false;
    for (let i = 0; i < 1e4; ++i)
        shouldBe(lazyProperty.hasOwnProperty("testLazyCallbackConstantInteger"), false);

    globalThis.isEnabledLazyCallbackConstantInteger = true;
    shouldBe(lazyProperty.hasOwnProperty("testLazyCallbackConstantInteger"), true);
})();

(function lazyCallbackCustomAccessor() {
    const lazyProperty = $vm.createStaticLazyProperty();
    Object.setPrototypeOf(lazyProperty, Object.prototype);

    globalThis.isEnabledLazyCallbackConstantInteger = false;
    for (let i = 0; i < 1e4; ++i)
        shouldBe(lazyProperty.hasOwnProperty("testLazyCallbackConstantInteger"), false);

    globalThis.isEnabledLazyCallbackConstantInteger = true;
    shouldBe(lazyProperty.hasOwnProperty("testLazyCallbackConstantInteger"), true);
})();

(function lazyCallbackConstantInteger() {
    const lazyProperty = $vm.createStaticLazyProperty();
    Object.setPrototypeOf(lazyProperty, Object.prototype);

    globalThis.isEnabledLazyCallbackConstantInteger = false;
    for (let i = 0; i < 1e4; ++i)
        shouldBe(lazyProperty.hasOwnProperty("testLazyCallbackConstantInteger"), false);

    globalThis.isEnabledLazyCallbackConstantInteger = true;
    shouldBe(lazyProperty.hasOwnProperty("testLazyCallbackConstantInteger"), true);
})();
