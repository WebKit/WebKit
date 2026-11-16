"use strict";
const testValue = Object.freeze({});

function assert(x, key) {
    if (!x)
        throw new Error(`Key: "${key}". Bad assertion!`);
}

function* testCases() {
    for (const key of ["description"])
        yield [$vm.createGlobalObject().Symbol.prototype, key];
    for (const key of ["buffer", "byteLength", "byteOffset"])
        yield [$vm.createGlobalObject().DataView.prototype, key];
    for (const key of ["baseName", "caseFirst", "hourCycle", "numeric", "region"])
        yield [$vm.createGlobalObject().Intl.Locale.prototype, key];

    for (const key of ["testStaticAccessorDontEnum", "testStaticAccessorReadOnly"])
        yield [$vm.createStaticCustomAccessor(), key];
    yield [$vm.createStaticCustomValue(), "testStaticValueReadOnly"];
}

for (const [object, key] of testCases()) {
    const target = {};
    assert(!Reflect.set(target, key, testValue, object), key);
    assert(!target.hasOwnProperty(key), key);
    assert(Object.getOwnPropertyDescriptor(object, key).value !== testValue, key);
}

for (const [object, key] of testCases()) {
    Object.defineProperty(object, key, { value: {}, writable: false });
    const target = {};
    assert(!Reflect.set(target, key, testValue, object), key);
    assert(!target.hasOwnProperty(key), key);
    assert(Object.getOwnPropertyDescriptor(object, key).value !== testValue, key);
}

for (const [object, key] of testCases()) {
    Object.defineProperty(object, key, { value: {}, writable: true, configurable: false });
    object[key] = testValue;

    const desc = Object.getOwnPropertyDescriptor(object, key);
    assert(desc.value === testValue, key);
    assert(desc.writable, key);
    assert(!desc.enumerable, key);
    assert(!desc.configurable, key);
}

for (const [object, key] of testCases()) {
    Object.defineProperty(object, key, { value: {}, writable: true, enumerable: true });
    const target = {};
    assert(Reflect.set(target, key, testValue, object), key);

    const desc = Object.getOwnPropertyDescriptor(object, key);
    assert(desc.value === testValue, key);
    assert(desc.writable, key);
    assert(desc.enumerable, key);
    assert(desc.configurable, key);
}
