function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`Bad value: ${actual}.`);
}

(function get() {
    const other = $vm.createGlobalObject();
    const lazyProperty = other.$vm.createStaticLazyProperty();
    other.isEnabledLazyCallbackAccessor = true;
    shouldBe(lazyProperty.testLazyCallbackAccessor, lazyProperty);
})();

(function set() {
    const other = $vm.createGlobalObject();
    const lazyProperty = other.eval("$vm.createStaticLazyProperty()");

    other.isEnabledLazyCallbackCustomAccessor = true;
    const value = {};
    lazyProperty.testLazyCallbackCustomAccessor = value;
    shouldBe(lazyProperty.testField, value);

    other.isEnabledLazyCallbackConstantInteger = true;
    lazyProperty.testLazyCallbackConstantInteger = 1;
    shouldBe(lazyProperty.testLazyCallbackConstantInteger, 42);
})();

(function ownKeys() {
    const other = $vm.createGlobalObject();
    const lazyProperty = other.Function("return $vm.createStaticLazyProperty()")();
    other.isEnabledLazyCallbackCustomAccessor = true;
    other.isEnabledLazyCallbackConstantInteger = true;
    shouldBe(Reflect.ownKeys(lazyProperty).join(), "testLazyCallbackCustomAccessor,testLazyCallbackConstantInteger");
})();

(function deleteProperty() {
    const other = $vm.createGlobalObject();
    const lazyProperty = other.$vm.createStaticLazyProperty();
    other.isEnabledLazyCallbackAccessor = true;
    other.isEnabledLazyCallbackConstantInteger = true;
    shouldBe(delete lazyProperty.testLazyCallbackConstantInteger, false);
    shouldBe(delete lazyProperty.testLazyCallbackAccessor, true);
    shouldBe(Reflect.ownKeys(lazyProperty).join(), "testLazyCallbackConstantInteger");
})();
