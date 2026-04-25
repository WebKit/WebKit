//@ requireOptions("--useExplicitResourceManagement=true")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`Expected ${expected} but got ${actual}`);
}

{
    let disposed = false;
    function f() {
        using x = { [Symbol.dispose]() { disposed = true; } };
        return 42;
    }
    shouldBe(f(), 42);
    shouldBe(disposed, true);
}

{
    let order = [];
    function f() {
        using a = { [Symbol.dispose]() { order.push("a"); } };
        using b = { [Symbol.dispose]() { order.push("b"); } };
        return "result";
    }
    shouldBe(f(), "result");
    shouldBe(order.join(","), "b,a");
}

{
    let disposed = false;
    function f() {
        using x = { [Symbol.dispose]() { disposed = true; } };
        if (true)
            return "early";
        return "late";
    }
    shouldBe(f(), "early");
    shouldBe(disposed, true);
}

{
    let order = [];
    let f = () => {
        using x = { [Symbol.dispose]() { order.push("arrow"); } };
        return 99;
    };
    shouldBe(f(), 99);
    shouldBe(order.join(","), "arrow");
}

{
    let order = [];
    function f() {
        {
            using a = { [Symbol.dispose]() { order.push("inner"); } };
            return "val";
        }
    }
    shouldBe(f(), "val");
    shouldBe(order.join(","), "inner");
}
