function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`Bad value: ${actual}!`);
}

function shouldThrow(func, errorMessage) {
    let errorThrown = false;
    let error = null;
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

const immutablePrototypeError = `TypeError: Cannot set prototype of immutable prototype object`;

// ProxyObject's handler traps caching depends on this
shouldThrow(() => { Object.prototype.__proto__ = { foo: 1 }; }, immutablePrototypeError);
shouldThrow(() => { Object.setPrototypeOf(Object.prototype, { bar: 2 }); }, immutablePrototypeError);

shouldBe(Reflect.setPrototypeOf(Object.prototype, {}), false);
shouldBe(Object.getPrototypeOf(Object.prototype), null);
