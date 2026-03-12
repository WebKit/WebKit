//@ requireOptions("--useExplicitResourceManagement=true")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`Expected ${expected} but got ${actual}`);
}

{
    let order = [];
    for (let i = 0; i < 3; i++) {
        using x = { val: i, [Symbol.dispose]() { order.push("dispose-" + this.val); } };
        order.push("use-" + x.val);
    }
    shouldBe(order.join(","), "use-0,dispose-0,use-1,dispose-1,use-2,dispose-2");
}

{
    let order = [];
    for (let i = 0; i < 3; i++) {
        using x = { val: i, [Symbol.dispose]() { order.push("dispose-" + this.val); } };
        if (i === 1) break;
        order.push("use-" + x.val);
    }
    shouldBe(order.join(","), "use-0,dispose-0,dispose-1");
}

{
    let order = [];
    for (let i = 0; i < 3; i++) {
        using x = { val: i, [Symbol.dispose]() { order.push("dispose-" + this.val); } };
        if (i === 1) continue;
        order.push("use-" + x.val);
    }
    shouldBe(order.join(","), "use-0,dispose-0,dispose-1,use-2,dispose-2");
}
