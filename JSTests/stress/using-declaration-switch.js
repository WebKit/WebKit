//@ requireOptions("--useExplicitResourceManagement=true")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`Expected ${expected} but got ${actual}`);
}

{
    let order = [];
    switch (1) {
        case 1: {
            using x = { [Symbol.dispose]() { order.push("case1"); } };
            break;
        }
        case 2: {
            using y = { [Symbol.dispose]() { order.push("case2"); } };
            break;
        }
    }
    shouldBe(order.join(","), "case1");
}

{
    let order = [];
    switch (0) {
        default: {
            using x = { [Symbol.dispose]() { order.push("default"); } };
        }
    }
    shouldBe(order.join(","), "default");
}

{
    let order = [];
    function f(val) {
        switch (val) {
            case 1: {
                using a = { [Symbol.dispose]() { order.push("a"); } };
                return "one";
            }
            case 2: {
                using b = { [Symbol.dispose]() { order.push("b"); } };
                return "two";
            }
        }
    }
    shouldBe(f(1), "one");
    shouldBe(order.join(","), "a");
    order = [];
    shouldBe(f(2), "two");
    shouldBe(order.join(","), "b");
}
