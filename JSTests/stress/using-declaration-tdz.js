//@ requireOptions("--useExplicitResourceManagement=true")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`Expected ${expected} but got ${actual}`);
}

function shouldThrow(func, errorType) {
    let threw = false;
    try {
        func();
    } catch (e) {
        threw = true;
        if (errorType && !(e instanceof errorType))
            throw new Error(`Expected ${errorType.name} but got ${e.constructor.name}: ${e.message}`);
    }
    if (!threw)
        throw new Error("Expected function to throw");
}

shouldThrow(function() {
    eval(`
        {
            x;
            using x = { [Symbol.dispose]() {} };
        }
    `);
}, ReferenceError);

shouldThrow(function() {
    eval(`
        {
            function f() { return x; }
            let val = f();
            using x = { [Symbol.dispose]() {} };
        }
    `);
}, ReferenceError);

{
    let accessed = false;
    {
        using x = { val: 10, [Symbol.dispose]() {} };
        accessed = (x.val === 10);
    }
    shouldBe(accessed, true);
}
