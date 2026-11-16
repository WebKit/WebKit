//@ requireOptions("--useExplicitResourceManagement=true")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`Expected ${expected} but got ${actual}`);
}

{
    let order = [];
    {
        using a = { [Symbol.dispose]() { order.push("outer-a"); } };
        {
            using b = { [Symbol.dispose]() { order.push("inner-b"); } };
            using c = { [Symbol.dispose]() { order.push("inner-c"); } };
        }
        using d = { [Symbol.dispose]() { order.push("outer-d"); } };
    }
    shouldBe(order.join(","), "inner-c,inner-b,outer-d,outer-a");
}

{
    let order = [];
    {
        using a = { [Symbol.dispose]() { order.push("1"); } };
        {
            using b = { [Symbol.dispose]() { order.push("2"); } };
            {
                using c = { [Symbol.dispose]() { order.push("3"); } };
            }
        }
    }
    shouldBe(order.join(","), "3,2,1");
}

{
    let order = [];
    {
        using a = { [Symbol.dispose]() { order.push("a"); } };
        if (true) {
            using b = { [Symbol.dispose]() { order.push("b"); } };
        }
        using c = { [Symbol.dispose]() { order.push("c"); } };
    }
    shouldBe(order.join(","), "b,c,a");
}
