//@ requireOptions("--useExplicitResourceManagement=true")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`Expected ${expected} but got ${actual}`);
}

{
    let disposed = false;
    eval(`
        {
            using x = { [Symbol.dispose]() { disposed = true; } };
        }
    `);
    shouldBe(disposed, true);
}

{
    let order = [];
    eval(`
        {
            using a = { [Symbol.dispose]() { order.push("a"); } };
            using b = { [Symbol.dispose]() { order.push("b"); } };
        }
    `);
    shouldBe(order.join(","), "b,a");
}

{
    let caught;
    try {
        eval(`
            {
                using x = { [Symbol.dispose]() { throw new Error("eval-dispose"); } };
            }
        `);
    } catch (e) {
        caught = e;
    }
    shouldBe(caught instanceof Error, true);
    shouldBe(caught.message, "eval-dispose");
}

{
    let caught;
    try {
        eval(`using x = { [Symbol.dispose]() {} };`);
    } catch (e) {
        caught = e;
    }
    shouldBe(caught instanceof SyntaxError, true);
}
