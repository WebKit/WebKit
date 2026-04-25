function shouldBe(actual, expected, testInfo) {
    if (actual !== expected)
        throw new Error(`Bad value: ${actual} (${testInfo})`);
}

function shouldThrow(fn, expectedError, testInfo) {
    let errorThrown = false;
    try {
        fn();
    } catch (error) {
        errorThrown = true;
        if (error.toString() !== expectedError)
            throw new Error(`Bad error: ${error} (${testInfo})`);
    }
    if (!errorThrown)
        throw new Error(`Not thrown (${testInfo})`);
}

function getFunctionsWithoutPrototype() {
    const { get, set } = Object.getOwnPropertyDescriptor({ get accessor() {}, set accessor(_v) {} }, "accessor");
    const arrowFunction = () => {};
    const asyncArrowFunction = async () => {};

    return [
        get,
        set,
        arrowFunction,
        asyncArrowFunction,
        { method() {} }.method,
        async function asyncFunction() {},
        { async asyncMethod() {} }.asyncMethod,
    ];
}

function getFunctionsWithNonConfigurablePrototype() {
    return [
        function normalNonStrictFunction() {},
        function normalStrictFunction() { "use strict"; },
        class baseConstructor {},
        class derivedConstructor extends Array {},
        function* syncGenerator() {},
        { * syncGeneratorMethod() {} }.syncGeneratorMethod,
        async function* asyncGenerator() {},
        { async * asyncGeneratorMethod() {} }.asyncGeneratorMethod,
        Array,
    ];
}

function defineNonConfigurablePrototype(fn) {
    return Object.defineProperties(fn, {
        prototype: { value: {}, configurable: false },
        name: { value: `redefined ${fn.name}` },
    });
}

const functionsWithoutPrototype = [
    ...getFunctionsWithoutPrototype(),
    ...getFunctionsWithNonConfigurablePrototype().map(fn => fn.bind()),
    parseInt,
];

const functionsWithNonConfigurablePrototype = [
    ...getFunctionsWithoutPrototype().map(defineNonConfigurablePrototype),
    ...getFunctionsWithNonConfigurablePrototype(),
];

(function nonStrictModeDelete() {
    for (const fn of functionsWithoutPrototype) {
        for (let i = 0; i < 1e4; ++i)
            shouldBe(delete fn.prototype, true, fn.name);
    }

    for (const fn of functionsWithNonConfigurablePrototype) {
        for (let i = 0; i < 1e4; ++i)
            shouldBe(delete fn.prototype, false, fn.name);
    }
})();

(function strictModeDelete() {
    "use strict";

    for (const fn of functionsWithoutPrototype) {
        for (let i = 0; i < 1e4; ++i)
            shouldBe(delete fn.prototype, true, fn.name);
    }

    for (const fn of functionsWithNonConfigurablePrototype) {
        for (let i = 0; i < 1e4; ++i)
            shouldThrow(() => { delete fn.prototype }, "TypeError: Unable to delete property.", fn.name);
    }
})();

(function reflectDeleteProperty() {
    noInline(Reflect.deleteProperty);

    for (const fn of functionsWithoutPrototype) {
        for (let i = 0; i < 1e4; ++i)
            shouldBe(Reflect.deleteProperty(fn, "prototype"), true, fn.name);
    }

    for (const fn of functionsWithNonConfigurablePrototype) {
        for (let i = 0; i < 1e4; ++i)
            shouldBe(Reflect.deleteProperty(fn, "prototype"), false, fn.name);
    }
})();
