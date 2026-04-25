//@ requireOptions("--useExplicitResourceManagement=true")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`Expected ${expected} but got ${actual}`);
}

{
    let caught;
    try {
        {
            using x = { [Symbol.dispose]() { throw new Error("dispose"); } };
        }
    } catch (e) {
        caught = e;
    }
    shouldBe(caught instanceof Error, true);
    shouldBe(caught.message, "dispose");
}

{
    let caught;
    try {
        {
            using a = { [Symbol.dispose]() { throw new Error("a"); } };
            using b = { [Symbol.dispose]() { throw new Error("b"); } };
        }
    } catch (e) {
        caught = e;
    }
    shouldBe(caught instanceof SuppressedError, true);
    shouldBe(caught.error.message, "a");
    shouldBe(caught.suppressed.message, "b");
}

{
    let caught;
    try {
        {
            using x = { [Symbol.dispose]: "not a function" };
        }
    } catch (e) {
        caught = e;
    }
    shouldBe(caught instanceof TypeError, true);
}
