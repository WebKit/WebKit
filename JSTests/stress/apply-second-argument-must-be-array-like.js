//@ runDefault

function assert(x) {
    if (!x)
        throw Error("Bad");
}

function shouldThrow(expr) {
    let testFunc = new Function(expr);
    for (let i = 0; i < 10000; i++) {
        let error;
        try {
            testFunc();
        } catch (e) {
            error = e;
        }
        assert(error);
    }
}

function foo() { }

shouldThrow("foo.apply(undefined, true)");
shouldThrow("foo.apply(undefined, false)");
shouldThrow("foo.apply(undefined, 100)");
shouldThrow("foo.apply(undefined, 123456789.12345)");
shouldThrow("foo.apply(undefined, 1.0/1.0)");
shouldThrow("foo.apply(undefined, 1.0/0)");
shouldThrow("foo.apply(undefined, 'hello')");
shouldThrow("foo.apply(undefined, Symbol())");

function bar() {
    return arguments.length;
}

for (let i = 0; i < 10000; i++) {
    new Function(`
        assert(bar.apply(undefined, undefined) === 0);
        assert(bar.apply(undefined, null) === 0);
        assert(bar.apply(undefined, {}) === 0);
        assert(bar.apply(undefined, []) === 0);
        assert(bar.apply(undefined, function() {}) === 0);
    `)();
}
