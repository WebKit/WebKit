//@ requireOptions("--useExplicitResourceManagement=true")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`Expected ${expected} but got ${actual}`);
}

{
    let order = [];
    let resources = [
        { name: "a", [Symbol.dispose]() { order.push("dispose-" + this.name); } },
        { name: "b", [Symbol.dispose]() { order.push("dispose-" + this.name); } },
        { name: "c", [Symbol.dispose]() { order.push("dispose-" + this.name); } },
    ];
    for (using x of resources) {
        order.push("use-" + x.name);
    }
    shouldBe(order.join(","), "use-a,dispose-a,use-b,dispose-b,use-c,dispose-c");
}

{
    let order = [];
    let resources = [
        { name: "a", [Symbol.dispose]() { order.push("dispose-" + this.name); } },
        { name: "b", [Symbol.dispose]() { order.push("dispose-" + this.name); } },
        { name: "c", [Symbol.dispose]() { order.push("dispose-" + this.name); } },
    ];
    for (using x of resources) {
        order.push("use-" + x.name);
        if (x.name === "b")
            break;
    }
    shouldBe(order.join(","), "use-a,dispose-a,use-b,dispose-b");
}
