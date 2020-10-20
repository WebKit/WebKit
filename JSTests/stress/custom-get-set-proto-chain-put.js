"use strict";

function assert(x) {
    if (!x)
        throw new Error("Bad assert!");
}

function shouldThrow(func, expectedError) {
    var errorThrown = false;
    try {
        func();
    } catch (error) {
        errorThrown = true;
        if (String(error) !== expectedError)
            throw new Error(`Bad error: ${error}`);
    }
    if (!errorThrown)
        throw new Error("Didn't throw!");
}

const customTestGetterSetter = $vm.createCustomTestGetterSetter();
Object.setPrototypeOf(customTestGetterSetter, {
    set customValue(_v) { throw new Error("Should be unreachable!"); },
    set customValueGlobalObject(_v) { throw new Error("Should be unreachable!"); },
    set customAccessor(_v) { throw new Error("Should be unreachable!"); },
    set customAccessorGlobalObject(_v) { throw new Error("Should be unreachable!"); },
});

const primitives = [true, 1, "", Symbol(), 1n];
for (let primitive of primitives) {
    let prototype = Object.getPrototypeOf(primitive);
    Object.setPrototypeOf(prototype, customTestGetterSetter);
}

function getObjects() {
    return [
        ...primitives.map(Object),
        Object.create(customTestGetterSetter),
        Object.create(Object.create(customTestGetterSetter)),
        // FIXME: ordinarySetSlow() should handle Custom{Accessor,Value} descriptors.
        // new Proxy(customTestGetterSetter, {}),
    ];
}

function getBases() {
    return [
        ...primitives,
        ...getObjects(),
    ];
}

// CustomAccessor: exception check
(() => {
    for (let base of getBases()) {
        for (let i = 0; i < 100; ++i) {
            let value = {};
            base.customAccessor = value;
            assert(value.hasOwnProperty("result"));
        }
        assert(Reflect.set(Object(base), "customAccessor", {}));
    }
})();

// CustomValue: exception check
(() => {
    for (let base of getBases()) {
        for (let i = 0; i < 100; ++i) {
            let value = {};
            base.customValue = value;
            assert(value.hasOwnProperty("result"));
        }
        assert(Reflect.set(Object(base), "customValue", {}));
    }
})();

// CustomAccessor: setter returns |false|
(() => {
    for (let base of getBases()) {
        for (let i = 0; i < 100; ++i)
            base.customAccessor = 1;
        // This is intentional:
        assert(!base.hasOwnProperty("customAccessor"));
        assert(Reflect.set(Object(base), "customAccessor", 1));
    }
})();

// CustomValue: setter returns |false|
(() => {
    for (let base of getBases()) {
        for (let i = 0; i < 100; ++i)
            base.customValue = 1;
        // This is weird, but it isn't exposed to userland code:
        assert(!base.hasOwnProperty("customValue"));
        assert(!Reflect.set(Object(base), "customValue", 1));
    }
})();

// CustomAccessor: no setter
(() => {
    for (let base of getBases()) {
        for (let i = 0; i < 100; ++i)
            shouldThrow(() => { base.customAccessorGlobalObject = {}; }, "TypeError: Attempted to assign to readonly property.");
        assert(!Reflect.set(Object(base), "customAccessorGlobalObject", {}));
    }
})();

// CustomValue: no setter
(() => {
    for (let primitive of primitives) {
        for (let i = 0; i < 100; ++i)
            shouldThrow(() => { primitive.customValueGlobalObject = {}; }, "TypeError: Attempted to assign to readonly property.");
    }

    for (let object of getObjects()) {
        for (let i = 0; i < 100; ++i)
            object.customValueGlobalObject = {};
        assert(object.hasOwnProperty("customValueGlobalObject"));
        assert(Reflect.set(object, "customValueGlobalObject", {}));
    }

    for (let object of getObjects()) {
        Object.preventExtensions(object);
        for (let i = 0; i < 100; ++i) {
            shouldThrow(() => { object.customValueGlobalObject = {}; }, "TypeError: Attempted to assign to readonly property.");
            assert(!Reflect.set(object, "customValueGlobalObject", {}));
        }
    }
})();
