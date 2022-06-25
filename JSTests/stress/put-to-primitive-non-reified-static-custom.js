"use strict";

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`Bad value: ${actual}!`);
}

function shouldThrow(func, errorMessage) {
    let errorThrown = false;
    try {
        func();
    } catch (error) {
        errorThrown = true;
        if (String(error) !== errorMessage)
            throw new Error(`Bad error: ${error}`);
    }
    if (!errorThrown)
        throw new Error("Didn't throw!");
}

const poisonedSetter = { set() { throw new Error("Object.prototype setter should be unreachable!"); } };
const primitives = [true, 1, "", Symbol(), 0n];

(function testStaticCustomValue() {
    Object.defineProperties(Object.prototype, {
        testStaticValue: poisonedSetter,
        testStaticValueNoSetter: poisonedSetter,
        testStaticValueReadOnly: poisonedSetter,
    });

    for (const primitive of primitives) {
        const primitivePrototype = Object.getPrototypeOf(primitive);
        const staticCustomValue = $vm.createStaticCustomValue();
        Object.setPrototypeOf(primitivePrototype, staticCustomValue);

        primitive.testStaticValue = 1;
        shouldBe(staticCustomValue.testStaticValue, 1);
        shouldThrow(() => { primitive.testStaticValue = 1; }, "TypeError: Attempted to assign to readonly property.");

        shouldThrow(() => { primitive.testStaticValueNoSetter = 1; }, "TypeError: Attempted to assign to readonly property.");
        shouldThrow(() => { primitive.testStaticValueReadOnly = 1; }, "TypeError: Attempted to assign to readonly property.");
        Object.setPrototypeOf(primitivePrototype, Object.prototype);
    }
})();

(function testStaticCustomAccessor() {
    Object.defineProperties(Object.prototype, {
        testStaticAccessor: poisonedSetter,
        testStaticAccessorReadOnly: poisonedSetter,
    });

    for (const primitive of primitives) {
        const primitivePrototype = Object.getPrototypeOf(primitive);
        Object.setPrototypeOf(primitivePrototype, $vm.createStaticCustomAccessor());

        for (let i = 0; i < 1000; i++) {
            primitive.testStaticAccessor = i;
            shouldBe(primitivePrototype.testField, i);
        }

        shouldThrow(() => { primitive.testStaticAccessorReadOnly = 1; }, "TypeError: Attempted to assign to readonly property.");
        Object.setPrototypeOf(primitivePrototype, Object.prototype);
    }
})();
