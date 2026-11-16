//@ requireOptions("--useExplicitResourceManagement=true")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`Expected ${expected} but got ${actual}`);
}

{
    let caught;
    try {
        {
            using a = { [Symbol.dispose]() { throw new Error("a"); } };
            using b = { [Symbol.dispose]() { throw new Error("b"); } };
            throw new Error("body");
        }
    } catch (e) {
        caught = e;
    }
    shouldBe(caught instanceof SuppressedError, true);
    shouldBe(caught.error.message, "a");
    shouldBe(caught.suppressed instanceof SuppressedError, true);
    shouldBe(caught.suppressed.error.message, "b");
    shouldBe(caught.suppressed.suppressed.message, "body");
}

{
    let caught;
    try {
        {
            using a = { [Symbol.dispose]() {} };
            using b = { [Symbol.dispose]() { throw new Error("b"); } };
            throw new Error("body");
        }
    } catch (e) {
        caught = e;
    }
    shouldBe(caught instanceof SuppressedError, true);
    shouldBe(caught.error.message, "b");
    shouldBe(caught.suppressed.message, "body");
}

{
    let order = [];
    try {
        {
            using a = { [Symbol.dispose]() { order.push("a-ok"); } };
            using b = { [Symbol.dispose]() { order.push("b-ok"); } };
            throw new Error("body");
        }
    } catch (e) {
        shouldBe(e.message, "body");
    }
    shouldBe(order.join(","), "b-ok,a-ok");
}

{
    let caught;
    try {
        {
            using a = { [Symbol.dispose]() { throw new Error("a"); } };
            using b = { [Symbol.dispose]() { throw new Error("b"); } };
            using c = { [Symbol.dispose]() { throw new Error("c"); } };
        }
    } catch (e) {
        caught = e;
    }
    shouldBe(caught instanceof SuppressedError, true);
    shouldBe(caught.error.message, "a");
    shouldBe(caught.suppressed instanceof SuppressedError, true);
    shouldBe(caught.suppressed.error.message, "b");
    shouldBe(caught.suppressed.suppressed.message, "c");
}
