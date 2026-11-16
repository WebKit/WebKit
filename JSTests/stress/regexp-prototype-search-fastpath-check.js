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

for (let i = 0; i < testLoopCount; i++) {
    // Execute fast path and throw type error
    shouldThrow(() => {
        RegExp.prototype[Symbol.search].call({});
    }, TypeError, "Builtin RegExp exec can only be called on a RegExp object");
}

for (let i = 0; i < testLoopCount; i++) {
    // Execute builtin exec
    const re = /foo/;
    let called = 0;
    Object.defineProperty(re, "exec", {
        get() {
            called++;
            return function () { return null };
        }
    });
    shouldBe(re[Symbol.search]("foo"), -1);
    shouldBe(re[Symbol.search]("bar"), -1);
    shouldBe(called, 2);
}

for (let i = 0; i < testLoopCount; i++) {
    // Execute non-builtin exec (native function)
    const re = /foo/;
    let called = 0;
    Object.defineProperty(re, "exec", {
        get() {
            called++;
            return String.prototype.match;
        }
    });
    shouldBe(re[Symbol.search]("foo"), 1);
    shouldBe(re[Symbol.search]("bar"), -1);
    shouldBe(called, 2);
}

// Invalidate watchpoint
Object.defineProperty(RegExp.prototype, "global", { get() { return false }});
for (let i = 0; i < testLoopCount; i++) {
    // Execute builtin exec
    const re = /foo/;
    shouldBe(re[Symbol.search]("foo"), 0);
    shouldBe(re[Symbol.search]("bar"), -1);
}

for (let i = 0; i < testLoopCount; i++) {
    // Execute builtin exec and throw type error
    shouldThrow(() => {
        RegExp.prototype[Symbol.search].call({})
    }, TypeError, "Builtin RegExp exec can only be called on a RegExp object");
}
