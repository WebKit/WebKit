//@ requireOptions("--useExplicitResourceManagement=true")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`Expected ${expected} but got ${actual}`);
}

{
    let disposed = false;
    for (using x = { [Symbol.dispose]() { disposed = true; } }; false; ) {
        throw new Error("unreachable");
    }
    shouldBe(disposed, true);
}

{
    let order = [];
    for (using x = { [Symbol.dispose]() { order.push("a"); } }, y = { [Symbol.dispose]() { order.push("b"); } }; false; ) {
        throw new Error("unreachable");
    }
    shouldBe(order.join(","), "b,a");
}

{
    let disposed = false;
    let i = 5;
    for (using x = { [Symbol.dispose]() { disposed = true; } }; i < 3; i++) {
        throw new Error("unreachable");
    }
    shouldBe(disposed, true);
}

{
    let disposed = false;
    let a = false, b = false;
    for (using x = { [Symbol.dispose]() { disposed = true; } }; a || b; ) {
        throw new Error("unreachable");
    }
    shouldBe(disposed, true);
}

{
    let disposed = false;
    let a = true, b = false;
    for (using x = { [Symbol.dispose]() { disposed = true; } }; a && b; ) {
        throw new Error("unreachable");
    }
    shouldBe(disposed, true);
}

{
    let order = [];
    let i = 0;
    for (using x = { [Symbol.dispose]() { order.push("dispose"); } }; i < 3; i++) {
        order.push("body" + i);
    }
    shouldBe(order.join(","), "body0,body1,body2,dispose");
}

{
    let disposed = false;
    label: for (using x = { [Symbol.dispose]() { disposed = true; } }; false; ) {
        throw new Error("unreachable");
    }
    shouldBe(disposed, true);
}

{
    let disposed = false;
    function f() {
        for (using x = { [Symbol.dispose]() { disposed = true; } }; false; ) {
            throw new Error("unreachable");
        }
        return 42;
    }
    shouldBe(f(), 42);
    shouldBe(disposed, true);
}

function loopTest() {
    let disposed = 0;
    for (using x = { [Symbol.dispose]() { disposed++; } }; false; ) {
    }
    return disposed;
}
noInline(loopTest);
for (let i = 0; i < testLoopCount; i++)
    shouldBe(loopTest(), 1);
