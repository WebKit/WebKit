function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
}

function shouldThrowTypeError(func, argument) {
    let error;
    try {
        func(argument);
    } catch (e) {
        error = e;
    }
    if (!(error instanceof TypeError))
        throw new Error(`expected TypeError but got ${error}`);
}

let hasMethod, hasGetter, hasSetter, getMethod, callGetter, callSetter;

function checkWithoutBrand(object) {
    shouldBe(hasMethod(object), false);
    shouldBe(hasGetter(object), false);
    shouldBe(hasSetter(object), false);
    shouldThrowTypeError(getMethod, object);
    shouldThrowTypeError(callGetter, object);
    shouldThrowTypeError(callSetter, object);
}

function checkWithBrand(object) {
    shouldBe(hasMethod(object), true);
    shouldBe(hasGetter(object), true);
    shouldBe(hasSetter(object), true);
    shouldBe(getMethod(object)(), 1);
    shouldBe(callGetter(object), 2);
    shouldBe(callSetter(object), 3);
}

function abortIf(condition) {
    if (condition)
        throw new Error("abort");
    return "second";
}

function define(shouldAbort) {
    return class {
        static #method() { return 1; }
        static get #getter() { return 2; }
        static set #setter(value) { }
        static [(
            hasMethod = object => #method in object,
            hasGetter = object => #getter in object,
            hasSetter = object => #setter in object,
            getMethod = object => object.#method,
            callGetter = object => object.#getter,
            callSetter = object => object.#setter = 3,
            checkWithoutBrand({ }),
            "first"
        )]() { }
        static [abortIf(shouldAbort)]() { }
    };
}

for (let i = 0; i < testLoopCount; i++) {
    let shouldAbort = !!(i & 1);
    let constructor = null;
    try {
        constructor = define(shouldAbort);
    } catch { }

    if (shouldAbort) {
        shouldBe(constructor, null);
        checkWithoutBrand({ });
    } else
        checkWithBrand(constructor);
}
