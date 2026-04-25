//@ requireOptions("--useExplicitResourceManagement=true")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`Expected ${expected} but got ${actual}`);
}

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

{
    var using;
    for (using of [10, 20, 30]) {}
    shouldBe(using, 30);
}

{
    var using;
    var of = [10, 20, 30];
    for (using of of) {}
    shouldBe(using, 30);
}

{
    var using;
    var results = [];
    for (using of [1, 2, 3]) {
        results.push(using);
    }
    shouldBe(results.join(","), "1,2,3");
}

shouldThrowSyntaxError("for (using x; false; ) {}");

shouldThrowSyntaxError("for (using x = null, y; false; ) {}");

shouldThrowSyntaxError("for (using let of []) {}");
