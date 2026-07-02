//@ requireOptions("--useExplicitResourceManagement=true")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`Expected ${expected} but got ${actual}`);
}

{
    let disposed = false;
    {
        using x = { [Symbol.dispose]() { disposed = true; } };
        shouldBe(disposed, false);
    }
    shouldBe(disposed, true);
}

{
    {
        using x = null;
        using y = undefined;
    }
}

{
    let order = [];
    {
        using a = { [Symbol.dispose]() { order.push("a"); } };
        using b = { [Symbol.dispose]() { order.push("b"); } };
        using c = { [Symbol.dispose]() { order.push("c"); } };
    }
    shouldBe(order.join(","), "c,b,a");
}

{
    let disposed = false;
    function test() {
        using x = { [Symbol.dispose]() { disposed = true; } };
        shouldBe(disposed, false);
    }
    test();
    shouldBe(disposed, true);
}

{
    let value;
    {
        using x = { val: 42, [Symbol.dispose]() {} };
        value = x.val;
    }
    shouldBe(value, 42);
}

{
    let disposed = false;
    try {
        using x = { [Symbol.dispose]() { disposed = true; } };
        throw new Error("test");
    } catch (e) {
        shouldBe(e.message, "test");
    }
    shouldBe(disposed, true);
}
