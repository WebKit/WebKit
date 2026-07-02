function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`Bad value: ${actual}!`);
}

function shouldThrow(func, expectedError) {
    let errorThrown = false;
    try {
        func();
    } catch (error) {
        errorThrown = true;
        if (error.toString() !== expectedError)
            throw new Error(`Bad error: ${error}!`);
    }
    if (!errorThrown)
        throw new Error("Didn't throw!");
}

(() => {
    eval(`
        try {
            function foo(a) {}
            eval('try { function foo(b) {} } catch {} function foo(c) {}');
        } catch {}
    `);

    shouldBe(foo.toString(), "function foo(a) {}");
})();

shouldThrow(() => {
    eval(`
        if (true) {
            function foo(a) {}
            eval('if (true) { function foo(b) {} } function foo(c) {}');
        }
    `);
}, "SyntaxError: Can't create duplicate variable in eval: 'foo'");
