function shouldBe(a, b) {
    if (a !== b)
        throw new Error(`Expected ${b} but got ${a}`);
}

function shouldThrow(run, errorType, message) {
    let actual;
    var hadError = false;

    try {
        actual = run();
    } catch (e) {
        hadError = true;
        actual = e;
    }

    if (!hadError)
        throw new Error("Expected " + run + "() to throw " + errorType.name + ", but did not throw.");
    if (!(actual instanceof errorType))
        throw new Error("Expeced " + run + "() to throw " + errorType.name + " , but threw '" + actual + "'");
    if (message !== void 0 && actual.message !== message)
        throw new Error("Expected " + run + "() to throw '" + message + "', but threw '" + actual.message + "'");
}

{
    // Execute fast path and throw type error
    shouldThrow(() => {
        RegExp.prototype.test({});
    }, TypeError, "Builtin RegExp exec can only be called on a RegExp object");
}

{
    // Execute builtin exec
    const re = /foo/;
    let called = 0;
    Object.defineProperty(re, "exec", {
        get() {
            called++;
            return function () { return null };
        }
    });
    shouldBe(re.test("foo"), false);
    shouldBe(re.test("bar"), false);
    shouldBe(called, 2);
}

{
    // Execute non-builtin exec (native function)
    const re = /foo/;
    let called = 0;
    Object.defineProperty(re, "exec", {
        get() {
            called++;
            return Object.getPrototypeOf;
        }
    });
    shouldBe(re.test("foo"), true);
    shouldBe(re.test("bar"), true);
    shouldBe(called, 2);
}

// Invalidate watchpoint
Object.defineProperty(RegExp.prototype, "global", { get() { return false }});
{
    // Execute builtin exec
    const re = /foo/;
    shouldBe(re.test("foo"), true);
    shouldBe(re.test("bar"), false);
}

{
    // Execute builtin exec and throw type error
    shouldThrow(() => {
        RegExp.prototype.test({})
    }, TypeError, "Builtin RegExp exec can only be called on a RegExp object");
}
