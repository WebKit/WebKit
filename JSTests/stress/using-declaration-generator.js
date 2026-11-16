//@ requireOptions("--useExplicitResourceManagement=true")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`Expected ${expected} but got ${actual}`);
}

{
    let order = [];
    function* gen() {
        using x = { [Symbol.dispose]() { order.push("dispose"); } };
        order.push("before-yield");
        yield 1;
        order.push("after-yield");
        yield 2;
    }
    let g = gen();
    shouldBe(g.next().value, 1);
    shouldBe(order.join(","), "before-yield");
    shouldBe(g.next().value, 2);
    shouldBe(order.join(","), "before-yield,after-yield");
    g.next();
    shouldBe(order.join(","), "before-yield,after-yield,dispose");
}

{
    let order = [];
    function* gen() {
        using x = { [Symbol.dispose]() { order.push("dispose"); } };
        yield 1;
        yield 2;
    }
    let g = gen();
    g.next();
    g.return();
    shouldBe(order.join(","), "dispose");
}

{
    let order = [];
    function* gen() {
        using a = { [Symbol.dispose]() { order.push("a"); } };
        yield 1;
        using b = { [Symbol.dispose]() { order.push("b"); } };
        yield 2;
    }
    let g = gen();
    g.next();
    g.next();
    g.next();
    shouldBe(order.join(","), "b,a");
}
