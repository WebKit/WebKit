//@ requireOptions("--useExplicitResourceManagement=1")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
}

function shouldThrow(func, errorType, message) {
    let error;
    try {
        func();
    } catch (e) {
        error = e;
    }
    if (!(error instanceof errorType))
        throw new Error(`Expected ${errorType.name}!`);
    if (message !== undefined)
        shouldBe(String(error), message);
}

shouldBe(typeof DisposableStack, "function");
shouldBe(Object.getPrototypeOf(DisposableStack.prototype), Object.prototype);
shouldBe(new DisposableStack() instanceof Object, true);
shouldBe(new DisposableStack() instanceof DisposableStack, true);
shouldThrow(() => { DisposableStack(); }, TypeError);
