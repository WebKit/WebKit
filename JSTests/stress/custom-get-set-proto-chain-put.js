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
const global = $vm.createGlobalObject(customTestGetterSetter);
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
        Object.create(new Proxy(customTestGetterSetter, {})),
        Object.create($vm.createGlobalProxy(global)),
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
            if (base == null || typeof base !== 'object') {
                shouldThrow(() => {
                    base.customValue = value;
                }, `TypeError: Attempted to assign to readonly property.`);
            } else {
                base.customValue = value;
                assert(!value.hasOwnProperty("result"));
                assert(base.customValue === value);
                assert(base.hasOwnProperty('customValue'));
            }
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
// FIXME: Once legacy RegExp features are implemented, there would be no use case for calling CustomValue setter if receiver is altered.
(() => {
    for (let base of getBases()) {
        for (let i = 0; i < 100; ++i) {
            if (base == null || typeof base !== 'object') {
                shouldThrow(() => {
                    base.customValue = 1;
                }, `TypeError: Attempted to assign to readonly property.`);
            } else {
                base.customValue = 1;
                assert(base.hasOwnProperty("customValue"));
            }
        }
        assert(Reflect.set(Object(base), "customValue", 1));
    }
})();

// CustomAccessor: no setter
(() => {
    for (let base of getBases()) {
        for (let i = 0; i < 100; ++i)
            shouldThrow(() => { base.customAccessorReadOnly = {}; }, "TypeError: Attempted to assign to readonly property.");
        assert(!Reflect.set(Object(base), "customAccessorReadOnly", {}));
    }
})();

// CustomValue: no setter
(() => {
    for (let primitive of primitives) {
        for (let i = 0; i < 100; ++i)
            shouldThrow(() => { primitive.customValueNoSetter = {}; }, "TypeError: Attempted to assign to readonly property.");
    }

    for (let object of getObjects()) {
        for (let i = 0; i < 100; ++i)
            object.customValueNoSetter = {};
        assert(object.hasOwnProperty("customValueNoSetter"));
        assert(Reflect.set(object, "customValueNoSetter", {}));
    }

    for (let object of getObjects()) {
        Object.preventExtensions(object);
        for (let i = 0; i < 100; ++i) {
            shouldThrow(() => { object.customValueNoSetter = {}; }, "TypeError: Attempting to define property on object that is not extensible.");
            assert(!Reflect.set(object, "customValueNoSetter", {}));
        }
    }
})();
