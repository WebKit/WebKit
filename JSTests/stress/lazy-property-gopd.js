function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`Bad value: ${actual}.`);
}

(function lazyCallbackAccessor() {
    const lazyProperty = $vm.createStaticLazyProperty();
    const key = "testLazyCallbackAccessor";

    shouldBe(Object.getOwnPropertyDescriptor(lazyProperty, key), undefined);
    globalThis.isEnabledLazyCallbackAccessor = true;

    const desc = Object.getOwnPropertyDescriptor(lazyProperty, key);
    shouldBe(typeof desc.get, "function");
    shouldBe(desc.set, undefined);
    shouldBe(desc.enumerable, true);
    shouldBe(desc.configurable, true);
})();

(function lazyCallbackCustomAccessor() {
    const lazyProperty = $vm.createStaticLazyProperty();
    const key = "testLazyCallbackCustomAccessor";

    shouldBe(Object.getOwnPropertyDescriptor(lazyProperty, key), undefined);
    globalThis.isEnabledLazyCallbackCustomAccessor = true;

    const desc1 = Object.getOwnPropertyDescriptor(lazyProperty, key);
    shouldBe(typeof desc1.get, "function");
    shouldBe(typeof desc1.set, "function");
    shouldBe(desc1.enumerable, true);
    shouldBe(desc1.configurable, true);

    const desc2 = Object.getOwnPropertyDescriptor(lazyProperty, key);
    shouldBe(desc2.get, desc1.get);
    shouldBe(desc2.set, desc1.set);
})();

(function lazyCallbackConstantInteger() {
    const lazyProperty = $vm.createStaticLazyProperty();
    const key = "testLazyCallbackConstantInteger";

    shouldBe(Object.getOwnPropertyDescriptor(lazyProperty, key), undefined);
    globalThis.isEnabledLazyCallbackConstantInteger = true;

    const desc = Object.getOwnPropertyDescriptor(lazyProperty, key);
    shouldBe(desc.value, 42);
    shouldBe(desc.writable, false);
    shouldBe(desc.enumerable, true);
    shouldBe(desc.configurable, false);
})();
