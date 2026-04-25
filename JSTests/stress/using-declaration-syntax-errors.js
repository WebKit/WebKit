//@ requireOptions("--useExplicitResourceManagement=true")

function shouldThrowSyntaxError(code) {
    let threw = false;
    try {
        eval(code);
    } catch (e) {
        threw = true;
        if (!(e instanceof SyntaxError))
            throw new Error(`Expected SyntaxError but got ${e.constructor.name}: ${e.message}`);
    }
    if (!threw)
        throw new Error(`Expected SyntaxError for: ${code}`);
}

shouldThrowSyntaxError("{ using { a } = { a: 1, [Symbol.dispose]() {} }; }");

shouldThrowSyntaxError("{ using x; }");

shouldThrowSyntaxError("for (using x in {}) {}");

shouldThrowSyntaxError("switch (0) { case 0: using x = null; }");
shouldThrowSyntaxError("switch (0) { default: using x = null; }");

{
    var using = 42;
    if (using !== 42)
        throw new Error("Expected using to be 42");
}
