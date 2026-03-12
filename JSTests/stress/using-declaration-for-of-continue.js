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
        if (x.name === "b") continue;
        order.push("use-" + x.name);
    }
    shouldBe(order.join(","), "use-a,dispose-a,dispose-b,use-c,dispose-c");
}

{
    let order = [];
    let resources = [
        { name: "a", [Symbol.dispose]() { order.push("dispose-" + this.name); } },
        { name: "b", [Symbol.dispose]() { order.push("dispose-" + this.name); } },
        { name: "c", [Symbol.dispose]() { order.push("dispose-" + this.name); } },
    ];
    try {
        for (using x of resources) {
            order.push("use-" + x.name);
            if (x.name === "b")
                throw new Error("stop");
        }
    } catch (e) {
        shouldBe(e.message, "stop");
    }
    shouldBe(order.join(","), "use-a,dispose-a,use-b,dispose-b");
}

{
    let order = [];
    function* gen() {
        yield { name: "a", [Symbol.dispose]() { order.push("dispose-a"); } };
        yield { name: "b", [Symbol.dispose]() { order.push("dispose-b"); } };
        order.push("gen-done");
    }
    for (using x of gen()) {
        order.push("use-" + x.name);
        if (x.name === "a") break;
    }
    shouldBe(order.join(","), "use-a,dispose-a");
}
